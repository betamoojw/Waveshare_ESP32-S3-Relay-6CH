#include "Controller.h"
#include "cli_interface.h"
#include "peripherals/btn_interface.h"
#include "peripherals/relay_control.h"
#include "peripherals/UARTCommunication.h"
#include "XctrlProtocol.h"
#include "peripherals/board_def.h"


#include <Arduino.h>
#include <Logger.h>

Controller controller;

Settings settings(SYS_CONFIG_FIL_PATH, SYS_CONFIG_FIL_SIZE);

Controller::Controller()
{
    configs.push_back(MT_SA_6CH_REV_1X);
}

void Controller::setup()
{
    String title = "Relay Controller";
    LOG_I(TAG, title + " booting start");


    // Initialize last ping time
    lastPingTime = millis();

    if (!LittleFS.begin())
    {
        LOG_W(TAG, "Failed to initilize the File System!!!");
        return;
    }
    // Initialize the settings
    if (!settings.begin())
    {
        LOG_W(TAG, "Failed to initilize the System Configure!!!");
        return;
    }

    if (!settings.exists("first_run"))
    {
        LOG_I(TAG, "Loading the default configuration when first boot...");
        settings.setBool("first_run", true);
        // config.resetToDefaults();
    }

    {
        // Initialize UART communication
        UARTCommunication& uartComm = UARTCommunication::getInstance();
        uartComm.setBaudRate(UARTCommunication::RS485BaudRate::BAUD_115200);
        uartComm.initialize();

        // Send a test message
        uartComm.sendData("UART initialized on Serial1 with TXD1 and RXD1.\n");

        // Connect the protocol to the UART communication interface
        xctrlProtocol.connect(&uartComm);

        // Initialize the protocol
        xctrlProtocol.initialize();

        // Wait for a second
        delay(1000);
    }

    // Get the singleton instance of BtnInterface
    BtnInterface& btnInterface = BtnInterface::getInstance();

    // Initialize the button interface with the specified pin
    btnInterface.initialize(GPIO_PIN_BTN);

    cli_init();

    LOG_I(TAG, title + " booting end");
}

void Controller::loop()
{
    unsigned long now = millis();
    if ((now - lastPingTime) / 1000 > PING_TIMEOUT_SECONDS)
    {
        //Todo: Implement ping logic
    }
    
    // Perform periodic tasks
    // For example, check for button presses, handle UART communication, etc.

    cli_task();

    UARTCommunication& uartComm = UARTCommunication::getInstance();
    std::string received = uartComm.receiveData();

    // If data is received, print it to the SERIAL console
    if (!received.empty()) 
    {
        LOG_I(TAG, "Msg from UART: " + String(received.c_str()));

        std::string msg = xctrlProtocol.decode(received);
        bool result = xctrlProtocol.processMsg(msg);

        if (false == result) 
        {
            LOG_W(TAG, "Unspported protocol!!!");
        }
    }
    

    // Get the singleton instance of BtnInterface
    BtnInterface& btnInterface = BtnInterface::getInstance();

    // Update the button state
    btnInterface.update();

    delay(100); // Add a small delay to avoid flooding the SERIAL console
}

void Controller::registerBoardConfig(ControllerConfig config)
{
    configs.push_back(config);
}

void Controller::loadSystemConfig()
{
    ControllerConfig config;

    config.basicConf.deviceName = settings.getString("deviceName", "MTech Switch Actuator 6Ch Rev 1.x");
    config.basicConf.fwVersion = settings.getString("fwVersion", "1.0.0");
    config.basicConf.model = settings.getString("model", "MT_SA_6CH_REV_1X");
    config.basicConf.sn = settings.getString("sn", "123456");   
    config.basicConf.uuid = settings.getString("uuid", "00000000-0000-0000-0000-000000000001");
    config.networkConf.wifiSSID = settings.getString("wifiSSID", "MTech_SA_6CH");
    config.networkConf.wifiPassword = settings.getString("wifiPassword", "password");
    config.networkConf.staticIP = settings.getBool("staticIP", false);
    config.networkConf.ipAddress = settings.getString("ipAddress", "192.168.1.100");
    config.networkConf.subnet = settings.getString("subnet", "255.255.  255.0");
    config.networkConf.gateway = settings.getString("gateway", "192.168.1.1");
    config.networkConf.dns = settings.getString("dns", "8.8.8.8");
    configs.push_back(config);
}

void Controller::saveSystemConfig()
{   
    const ControllerConfig config = configs.at(0);

    settings.setString("deviceName", config.basicConf.deviceName);
    settings.setString("fwVersion", config.basicConf.fwVersion);
    settings.setString("model", config.basicConf.model);
    settings.setString("sn", config.basicConf.sn);
    settings.setString("uuid", config.basicConf.uuid); 
    settings.setString("wifiSSID", config.networkConf.wifiSSID);
    settings.setString("wifiPassword", config.networkConf.wifiPassword);
    settings.setBool("staticIP", config.networkConf.staticIP);
    settings.setString("ipAddress", config.networkConf.ipAddress);
    settings.setString("subnet", config.networkConf.subnet);
    settings.setString("gateway", config.networkConf.gateway);
    settings.setString("dns", config.networkConf.dns);

    if (!settings.save())
    {
        LOG_E(TAG, "Failed to save system configuration!");
    }
}

void Controller::setDefaultSystemConfig(ControllerConfig config)
{
    // _config = config;
    // settings.setString("system_config", _config.toJson());
    // saveSystemConfig();
}