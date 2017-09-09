#include "zone_controller.hpp"
#include "server.hpp"
#include <sstream>
#include <algorithm>
#include <iterator>
#include <time.h>
#include <stdlib.h>


ZoneController::Commands commands_ = {
    {"on" , ZoneController::On},
    {"off", ZoneController::Off},
    {"status", ZoneController::Status}
};

// This should be read from a configuration file....
ZoneController::Zone zones[] = {
  {"citrus", 2, 0, false},
  {"grass", 3, 0, false},
  {"vedgie", 4, 0, false},
  {"side", 5, 0, false}
};

static int ZoneController::NUMBER_OF_ZONES = sizeof(zones) / sizeof(Zone);

ZoneController::ZoneController(Server& comms)
: comms_(comms) {}

int ZoneController::ParseLine(int filedes, std::string& sentence)
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

void ZoneController::On(int filedes, std::vector<std::string>& line)
{
  if (line.size() == 3)
  {
    bool found = false;
    int i = 0;
    for (; i < NUMBER_OF_ZONES && found == false; i++)
    {
      found = line[1] == zones[i].name;
    }
    long seconds = strtol(line[2].c_str(), NULL, 10);

    if (found == false)
    {
      comms_.PrintfSock(filedes, "Unable to find zone %s.\r\n", line[1].c_str());
    }
    else
    {
      i--;
      for (int j = 0; j < NUMBER_OF_ZONES; j++)
      {
        if ((i != j) && zones[j].secondsRemaining > 0)
        {
          zones[j].secondsRemaining = 0;
          comms_.PrintfSock(filedes, "Forced zone %s off, ", zones[j].name.c_str());
        }
      }

      zones[i].secondsRemaining = seconds;
      comms_.PrintfSock(filedes, "Zone %s on for %i.\r\n", zones[i].name.c_str(), zones[i].secondsRemaining);
    }
  }
  else
  {
    comms_.PrintfSock(filedes, "Bad arguements to On.\r\n");
  }
}

void ZoneController::Off(int filedes, std::vector<std::string>& line)
{
  if (line.size() == 2)
  {
    bool found = false;
    int i = 0;
    for (; i < NUMBER_OF_ZONES && found == false; i++)
    {
      found = line[1] == zones[i].name;
    }

    if (found == false)
    {
      comms_.PrintfSock(filedes, "Unable to find zone %s.\r\n", line[1].c_str());
    }
    else
    {
      i--;
      zones[i].secondsRemaining = 0;
      comms_.PrintfSock(filedes, "Zone %s off.\r\n", zones[i].name.c_str());
    }
  }
  else
  {
    comms_.PrintfSock(filedes, "Bad arguements to On.\r\n");
  }
}

void ZoneController::Status(int filedes, std::vector<std::string>& line)
{
    for (int j = 0; j < NUMBER_OF_ZONES; j++)
    {
      comms_.PrintfSock(filedes, "(%s, %i)", zones[j].name.c_str(), zones[j].secondsRemaining);
    }
    comms_.PrintfSock(filedes, "\n\r");
}

void ZoneController::Periodic(void)
{
  static time_t lastSeconds = time(NULL);

  time_t thisTime = time(NULL);
  time_t delta = thisTime - lastSeconds;
  lastSeconds = thisTime;
  bool oneOn = false;

 // after the system is disconnected from the mains, let one of the zones
 // release the preassure on the manafold by opening it for some amount of
 // time.
  pressureRelease -= delta;

  // first shut them down
  for (int i = 0; i < NUMBER_OF_ZONES; i++)
  {
    Zone& z(zones[i]);
    z.secondsRemaining -= delta;
    if (z.secondsRemaining <= 0)
    {
      z.secondsRemaining = 0;
      // don't shut a zone if it is releaving preassure in the manafold
      if (z.currentlyOn && (pressureRelease < 0 || pressureZone != i))
      {
        //TODO fprintf(stderr, "Shutting %s\n\r", zones[i].name.c_str());
        z.currentlyOn = false;
      }
    }
  }

  for (int i = 0; i < NUMBER_OF_ZONES; i++)
  {
    Zone& z(zones[i]);
    if (z.secondsRemaining > 0)
    {
      oneOn = true;
      pressureRelease = 0;
      if (z.currentlyOn == false)
      {
        //TODO fprintf(stderr, "Opening %s\n\r", zones[i].name.c_str());
        z.currentlyOn = true;
      }
    }
  }

  if (oneOn)
  {
    if (masterOn == false)
    {
      //TODO fprintf(stderr, "Opening MASTER\n\r");
      masterOn = true;
    }
  }
  else
  {
    if (masterOn == true)
    {
      //TODO fprintf(stderr, "Shutting MASTER\n\r");
      masterOn = false;
      pressureRelease = 20;
      //TODO fprintf(stderr, "Opening preassure zone %s\n\r", zones[pressureZone].name.c_str());
      zones[pressureZone].currentlyOn = true;
    }
  }


}

void ZoneController::ShutAllValves(void)
{
    comms_.PrintfAllSockets("Shutting all values\r\n");
    comms_.PrintfAllSockets("Shutting MASTER - %i\r\n", MASTER_PIN);
    for (int i = 0; i < NUMBER_OF_ZONES; i++)
    {
        comms_.PrintfAllSockets("Shutting %s - %i\r\n", zones[i].name.c_str(), zones[i].pin);
        zones[i].currentlyOn = false;
    }
}
