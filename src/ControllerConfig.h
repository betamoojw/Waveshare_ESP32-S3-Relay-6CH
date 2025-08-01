#ifndef CONTROLLERCONFIG_H
#define CONTROLLERCONFIG_H
#include <string>

struct ControllerConfig
{
        std::string name;
        std::string model;
        std::string sn;
        std::string uuid;
};

const ControllerConfig MT_SA_6CH_REV_1X = {.name = "MTech Switch Actuator 6Ch Rev 1.x",
                                           .model = "MT_SA_6CH_REV_1X",
                                           .sn = "123456",
                                           .uuid = "00000000-0000-0000-0000-000000000001"};

const ControllerConfig MT_SA_8CH_REV_1X = {.name = "MTech Switch Actuator 8Ch Rev 1.x",
                                           .model = "MT_SA_8CH_REV_1X",
                                           .sn = "654321",
                                           .uuid = "00000000-0000-0000-0000-000000000002"};

#endif // CONTROLLERCONFIG_H