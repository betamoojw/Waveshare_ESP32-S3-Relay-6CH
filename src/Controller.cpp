#include "Controller.h"
#include "cli_interface.h"
#include "peripherals/btn_interface.h"
#include "peripherals/relay_control.h"
#include "peripherals/board_def.h"
#include "modbus/modbus.h"

#include <Arduino.h>
#include <Logger.h>

Controller controller;

HardwareSerial* hwSerial;
Modbus mbsInf;

int32_t read_serial(const char port[], uint8_t *buf, uint16_t count, int32_t byte_timeout_ms)
{
    Serial1.setTimeout(byte_timeout_ms);
    return Serial1.readBytes(buf, count);
}

int32_t write_serial(const char port[], const uint8_t *buf, uint16_t count, int32_t byte_timeout_ms)
{
    Serial1.setTimeout(byte_timeout_ms);
    return Serial1.write(buf, count);
}

Controller::Controller()
{
    configs.push_back(MT_SA_6CH_REV_1X);
    configs.push_back(MT_SA_8CH_REV_1X);
}

void Controller::setup()
{
    String title = "Relay Controller";
    LOG_I(TAG, title + " booting start");

    {
        hwSerial = &Serial1;
        hwSerial->begin(115200, SERIAL_8N1, RXD1, TXD1);

        const std::string data = "UART initialized on Serial1 with TXD1 and RXD1.\n";
        hwSerial->write(data.c_str(), data.length());

        // Set serial read and write functions
        mbsInf.setSerialRead(read_serial);
        mbsInf.setSerialWrite(write_serial);

        // Create Modbus client in RTU mode
        if (!mbsInf.createClientRTU(1))
        {
            LOG_I(TAG, "Failed to create the modbus RTU client");
        }
    }

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


    {
        std::string receivedData;
        if (hwSerial && hwSerial->available()) 
        {
            while (hwSerial->available()) 
            {
                receivedData += static_cast<char>(hwSerial->read());
            }
        }

        // If data is received, print it to the SERIAL console
        if (!receivedData.empty()) 
        {
            LOG_I(TAG, "Msg from UART: " + String(receivedData.c_str()));
        }
    }

    {
        // Perform Modbus operations
        uint8_t outputs_array[10];
        std::vector<uint8_t> outputs_vector(outputs_array, outputs_array + 10);
        if (!mbsInf.getDigitalOutputs(outputs_vector, 0x0000, 10))
        {
            LOG_I(TAG, "Failed to getDigitalOutputs");
        }

        String outputString = "Vector elements: "; // Initialize the string

        for (int i = 0; i < outputs_vector.size(); ++i)
        {
            outputString += String(outputs_vector[i]); // Convert int to String and append
            if (i < outputs_vector.size() - 1)
            {
                outputString += ", "; // Add a comma and space between elements
            }
        }
        LOG_I(TAG, "modbusc client getDigitalOutputs" + outputString);
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