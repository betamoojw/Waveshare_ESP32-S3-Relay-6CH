#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "ControllerConfig.h"

#include <vector>



constexpr double PING_TIMEOUT_SECONDS = 20;

class Controller
{
    public:
        Controller();
        void setup(void);
        void loop(void);

        void registerBoardConfig(ControllerConfig config);

    public:
    private:
    private:
        ControllerConfig _config = ControllerConfig{};
        std::vector<ControllerConfig> configs;
        unsigned long lastPingTime = 0;
        const char *TAG = "Controller";
};

extern Controller controller;

#endif // CONTROLLER_H