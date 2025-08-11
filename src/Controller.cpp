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

    }

    {
        modbus_set_serial_read(read_serial);
        modbus_set_serial_write(write_serial);

        if (modbus_server_create_RTU(10))
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


    // {
    //     std::string receivedData;
    //     if (hwSerial && hwSerial->available()) 
    //     {
    //         while (hwSerial->available()) 
    //         {
    //             receivedData += static_cast<char>(hwSerial->read());
    //         }
    //     }

    //     // If data is received, print it to the SERIAL console
    //     if (!receivedData.empty()) 
    //     {
    //         LOG_I(TAG, "Msg from UART: " + String(receivedData.c_str()));
    //     }
    // }

    {
        const uint8_t digital_outputs = random(0, 1);
	    modbus_server_set_digital_outputs(&digital_outputs, 0, 1);

        std::vector<uint16_t> params = {40, 50, 60, 70};
        uint16_t address = 48;
        uint16_t quantity = params.size();

        // Call function with vector's data
        modbus_server_set_parameters(params.data(), address, quantity);
        
        modbus_server_polling();
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