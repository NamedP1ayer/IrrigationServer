#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string>
#include <stdarg.h>
#include <signal.h>
#include <sstream>
#include <algorithm>
#include <iterator>
#include <map>
#include <vector>

#define PORT    5555
#define MAXMSG  512

void fprintfsock(int s, const char* f, ...)
{
    va_list a;
    va_start( a, f );
    int l = vsnprintf( 0, 0, f, a );
    char* buf = (char*) malloc( l + 1 );
    va_start( a, f );
    vsnprintf( buf, l, f, a );
    send( s, buf, l, 0 );
    free( buf );
}

typedef void (*Command)(int filedes, std::vector<std::string>& line);

void On(int filedes, std::vector<std::string>& line);
void Off(int filedes, std::vector<std::string>& line);
void Status(int filedes, std::vector<std::string>& line);
void Periodic(void);

typedef std::map<std::string, Command> Commands;
Commands commands_ = {{"on" , On}, {"off", Off}, {"status", Status}};;

struct Connection
{
  std::string current_line;
};

typedef std::map<int, Connection> Connections;


Connections connections_;

void printallsockets(const char* f, ...)
{
    va_list a;
    va_start( a, f );
    int l = vsnprintf( 0, 0, f, a );
    char* buf = (char*) malloc( l + 1 );
    va_start( a, f );
    vsnprintf( buf, l + 1, f, a );

    for(Connections::iterator it = connections_.begin(); it != connections_.end(); ++it)
    {
       send((*it).first, buf, l, 0);
    }
    fprintf(stderr, "%s", buf);
    free( buf );
}

int parse_line(int filedes, std::string& sentence)
{
  std::istringstream iss(sentence);
  std::vector<std::string> tokens{std::istream_iterator<std::string>{iss},
                      std::istream_iterator<std::string>{}};
  if (tokens.size() > 0)
  {
    if (commands_.find(tokens.front()) != commands_.end())
    {
      commands_[tokens.front()](filedes, tokens);
    }
  }
}

int read_from_client(int filedes)
{
    char buffer[MAXMSG];
    ssize_t nbytes;

    nbytes = read(filedes, buffer, MAXMSG);
    if (nbytes < 0)
    {
        /* Read error. */
        perror ("read");
        exit (EXIT_FAILURE);
    }
    else if (nbytes == 0)
    {
    /* End-of-file. */
        connections_.erase(filedes);
        printallsockets("Disconnected %i\r\n", filedes);
        return -1;
    }
    else
    {
      for (int i = 0; i < nbytes; i++)
      {
        if (buffer[i] == '\r')
        {

        }
        else if (buffer[i] == '\n')
        {
          parse_line(filedes, connections_[filedes].current_line);
          connections_[filedes].current_line.clear();
        }
        else
        {
          connections_[filedes].current_line += buffer[i];
        }
      }
        return 0;
    }
}

int make_socket(uint16_t port)
{
    int sock;
    struct sockaddr_in name;

    /* Create the socket. */
    sock = socket (PF_INET, SOCK_STREAM, 0);
    if (sock < 0)
    {
        perror ("socket");
        exit (EXIT_FAILURE);
    }

    /* Give the socket a name. */
    name.sin_family = AF_INET;
    name.sin_port = htons (port);
    name.sin_addr.s_addr = htonl (INADDR_ANY);
    if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0)
    {
        perror ("bind");
        exit (EXIT_FAILURE);
    }

    return sock;
}


void ShutAllValves(void);


void SignalHandler(int signum)
{
    ShutAllValves();
}

int main (void)
{
    atexit(ShutAllValves);
    if (signal (SIGINT, SignalHandler) == SIG_IGN)
        signal (SIGINT, SIG_IGN);
    if (signal (SIGHUP, SignalHandler) == SIG_IGN)
        signal (SIGHUP, SIG_IGN);
    if (signal (SIGTERM, SignalHandler) == SIG_IGN)
        signal (SIGTERM, SIG_IGN);

    int sock;
    fd_set active_fd_set, read_fd_set;
    int i;
    struct sockaddr_in clientname;
    socklen_t size;
    struct timeval timeout;

    /* Create the socket and set it up to accept connections. */
    sock = make_socket (PORT);
    if (listen (sock, 1) < 0)
    {
        perror ("listen");
        exit (EXIT_FAILURE);
    }

    /* Initialize the set of active sockets. */
    FD_ZERO (&active_fd_set);
    FD_SET (sock, &active_fd_set);

    while (1)
    {
        /* Block until input arrives on one or more active sockets. */
        read_fd_set = active_fd_set;
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int select_result = select (FD_SETSIZE, &read_fd_set, NULL, NULL, &timeout);
        if (select_result < 0)
        {
            perror ("select");
            exit (EXIT_FAILURE);
        }
        Periodic();
        /* Service all the sockets with input pending. */
        for (i = 0; i < FD_SETSIZE; ++i)
            if (FD_ISSET (i, &read_fd_set))
            {
                if (i == sock)
                {
                    /* Connection request on original socket. */
                    int new_con;
                    size = sizeof (clientname);
                    new_con = accept (sock,
                                  (struct sockaddr *) &clientname,
                                  &size);
                    if (new_con < 0)
                    {
                        perror ("accept");
                        exit (EXIT_FAILURE);
                    }
                    printallsockets("Server: connect from host %s, port %hd.\r\n",
                             inet_ntoa (clientname.sin_addr),
                             ntohs (clientname.sin_port));
                    FD_SET (new_con, &active_fd_set);
                }
                else
                {
                    /* Data arriving on an already-connected socket. */
                    if (read_from_client (i) < 0)
                    {
                        close (i);
                        FD_CLR (i, &active_fd_set);
                    }
                }
            }
    }
}

struct Zone
{
    std::string name;
    int pin;
    int secondsRemaining;
};

Zone zones[] = {
    {"citrus", 2, 0},
    {"grass", 3, 0},
    {"vedgie", 4, 0},
    {"side", 5, 0}
};

static const int NUMBER_OF_ZONES = sizeof(zones) / sizeof(Zone);

static const int MASTER_PIN = 0;

void On(int filedes, std::vector<std::string>& line)
{
  if (line.size() == 3)
  {
    bool found = false;
    for (int i = 0; i < NUMBER_OF_ZONES && found == false; i++)
    {
      found = line[2] == zones[i].name;
    }
    long seconds = strtol(line[2].c_str(), NULL, 10);

    if (found == false)
    {
      fprintfsock(filedes, "Unable to find zone %s.\r\n", line[1].c_str());
    }
    else
    {

      for (int j = 0; j < NUMBER_OF_ZONES; j++)
      {
        if ((i != j) && zones[j].secondsRemaining > 0)
        {
          zones[j].secondsRemaining = 0;
          fprintfsock(filedes, "Forced zone %s off, ", zones[j].name.c_str());
        }
      }

      zones[i].secondsRemaining = seconds;
      fprintfsock(filedes, "Zone %s on for %i.\r\n", zones[i].name.c_str(), zones[i].secondsRemaining);
    }
  }
  else
  {
    fprintfsock(filedes, "Bad arguements to On.\r\n");
  }
}

void Off(int filedes, std::vector<std::string>& line)
{
  if (line.size() == 2)
  {
    bool found = false;
    for (int i = 0; i < NUMBER_OF_ZONES && found == false; i++)
    {
      found = line[2] == zones[i].name;
    }

    if (found == false)
    {
      fprintfsock(filedes, "Unable to find zone %s.\r\n", line[1].c_str());
    }
    else
    {
      zones[i].secondsRemaining = 0;
      fprintfsock(filedes, "Zone %s off.\r\n", zones[i].name.c_str());
    }
  }
  else
  {
    fprintfsock(filedes, "Bad arguements to On.\r\n");
  }
}

void Status(int filedes, std::vector<std::string>& line)
{
    for (int j = 0; j < NUMBER_OF_ZONES; j++)
    {
      fprintfsock(filedes, "(%s, %i)", zones[j].name.c_str(), zones[j].secondsRemaining);
    }
}

void Periodic(void)
{
  //printallsockets("Periodic!\r\n");
}

void ShutAllValves(void)
{
    printallsockets("Shutting all values\r\n");
    printallsockets("Shutting MASTER - %i\r\n", MASTER_PIN);
    for (int i = 0; i < NUMBER_OF_ZONES; i++)
    {
        printallsockets("Shutting %s - %i\r\n", zones[i].name.c_str(), zones[i].pin);
    }
}
