#include <Arduino.h>
#include "HWCDC.h"
#include "cli_interface.h"
#include "btn_interface.h"
#include "relay_control.h"
#include "UARTCommunication.h"
#include "board_def.h"

HWCDC USBSerial; // Definition of the USBSerial object

void setup()
{
    String title = "Relay Controller";
    USBSerial.begin(115200);

    USBSerial.println(title + " start");

    {
        // Initialize UART communication
        UARTCommunication& uartComm = UARTCommunication::getInstance();
        uartComm.setBaudRate(UARTCommunication::RS485BaudRate::BAUD_115200);
        uartComm.initialize();

        // Send a test message
        uartComm.sendData("UART initialized on Serial1 with TXD1 and RXD1.\n");

        // Wait for a second
        delay(1000);
    }

    // Get the singleton instance of BtnInterface
    BtnInterface& btnInterface = BtnInterface::getInstance();

    // Initialize the button interface with the specified pin
    btnInterface.initialize(GPIO_PIN_BTN);

    cli_init();

    USBSerial.println(title + " end");
}

void loop()
{
    cli_task();

    UARTCommunication& uartComm = UARTCommunication::getInstance();
    std::string received = uartComm.receiveData();

    // If data is received, print it to the USBSerial    
    if (!received.empty()) 
    {
        USBSerial.println("Msg from UART: " + String(received.c_str()));
    }
    delay(100); // Add a small delay to avoid flooding the serial output

    // Get the singleton instance of BtnInterface
    BtnInterface& btnInterface = BtnInterface::getInstance();

    // Update the button state
    btnInterface.update();
}