#include "Controller.h"
#include "cli_interface.h"
#include "peripherals/btn_interface.h"
#include "peripherals/relay_control.h"
#include "peripherals/board_def.h"


#include <Arduino.h>
#include <Logger.h>

Controller controller;

Controller::Controller()
{
    configs.push_back(MT_SA_6CH_REV_1X);
    configs.push_back(MT_SA_8CH_REV_1X);
}

void Controller::setup()
{
    String title = "Relay Controller";
    LOG_I(TAG, title + " booting start");

    // Initialize last ping time
    lastPingTime = millis();

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