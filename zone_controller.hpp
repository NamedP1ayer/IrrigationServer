#ifndef zone_controller_hpp__
#define zone_controller_hpp__

#include <string>
#include <vector>
#include <map>

class Server;

class ZoneController
{
public:
    ZoneController(Server& comms);

public:
    void ShutAllValves(void);
    void Periodic(void);

public:
    int ParseLine(int filedes, std::string& sentence);

private:
    Server& comms_;

    typedef void (ZoneController::*Command)(int filedes, std::vector<std::string>& line);

    typedef std::map<std::string, Command> Commands;
    static Commands commands_;

    struct Zone
    {
        std::string name;
        int pin;
        int secondsRemaining;
        bool currentlyOn;
    };

    static Zone zones[];
    static int NUMBER_OF_ZONES;

    // These two should be read from a configuration file.
    static const int MASTER_PIN = 0;
    int pressureZone = 2;

    bool masterOn = false;
    int pressureRelease = 0;

    void On(int filedes, std::vector<std::string>& line);
    void Off(int filedes, std::vector<std::string>& line);
    void Status(int filedes, std::vector<std::string>& line);
};

#endif // zone_controller_hpp__
