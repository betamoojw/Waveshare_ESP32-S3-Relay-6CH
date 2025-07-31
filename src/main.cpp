#include <Arduino.h>
#include <Logger.h>
#include "cli_interface.h"
#include "btn_interface.h"
#include "relay_control.h"
#include "UARTCommunication.h"
#include "XctrlProtocol.h"
#include "board_def.h"


XctrlProtocol xctrlProtocol; // Create an instance of XctrlProtocol

#define TAG "SYSTEM"

void setup()
{
    String title = "Relay Controller";

    LOG_I(TAG, title + " booting start");

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

void loop()
{
    cli_task();

    UARTCommunication& uartComm = UARTCommunication::getInstance();
    std::string received = uartComm.receiveData();

    // If data is received, print it to the SERIAL console
    if (!received.empty()) 
    {
        LOG_I(TAG, "Msg from UART: " + String(received.c_str()));

        // Check if the received data equals "help"
        if (received == "help") 
        {
            xctrlProtocol.showHelp(); // Execute the showHelp() function
        }
    }
    delay(100); // Add a small delay to avoid flooding the SERIAL console

    // Get the singleton instance of BtnInterface
    BtnInterface& btnInterface = BtnInterface::getInstance();

    // Update the button state
    btnInterface.update();
}