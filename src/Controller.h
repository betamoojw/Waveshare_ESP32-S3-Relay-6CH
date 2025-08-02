#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "ControllerConfig.h"
#include "Logger.h"
#include "path.h"
#include "Settings.h"

#include <Arduino.h>
#include <LittleFS.h>
#include <vector>


constexpr double PING_TIMEOUT_SECONDS = 20;

class Controller
{
    public:
        Controller();
        void setup(void);
        void loop(void);

        void registerBoardConfig(ControllerConfig config);
        
        void loadSystemConfig();
        void saveSystemConfig();
        void setDefaultSystemConfig(ControllerConfig config);


    public:
        // Settings settings("/system_config.json", 2048);
        

    private:
    private:
        ControllerConfig _config = ControllerConfig{};
        std::vector<ControllerConfig> configs;
        unsigned long lastPingTime = 0;
        const char *TAG = "Controller";
        
};

extern Controller controller;

#endif // CONTROLLER_H