// modbus.h
#ifndef MODBUS_H
#define MODBUS_H

#include "modbusclient.h"
#include "modbusserver.h"

class Modbus
{
    public:
        // Constructor
        Modbus();

        // Destructor
        ~Modbus();

        // Set serial read/write functions shared by both client and server
        void setSerialRead(int32_t (*serialRead)(const char port[], uint8_t *, uint16_t, int32_t));
        void setSerialWrite(int32_t (*serialWrite)(const char port[], const uint8_t *, uint16_t, int32_t));

        // Set serial port shared by both Modbus client and server
        void setSerialPort(const char port[]);

        // Create Modbus client in RTU mode
        bool createClientRTU(uint8_t address);

        // Client functions
        void setClientRTUAddress(uint8_t address);
        bool getDigitalOutputs(std::vector<uint8_t> &outputs, uint16_t address, uint16_t quantity);
        bool getDigitalInputs(std::vector<uint8_t> &inputs, uint16_t address, uint16_t quantity);
        bool getAnalogInputs(std::vector<uint16_t> &inputs, uint16_t address, uint16_t quantity);
        bool setParameters(const std::vector<uint16_t> &parameters, uint16_t address);
        bool getParameters(std::vector<uint16_t> &parameters, uint16_t address, uint16_t quantity);

        // Create Modbus server in RTU mode
        bool createServerRTU(uint8_t address);
        
        // Server functions
        bool serverPolling();
        bool setDigitalOutputs(const uint8_t outputs[], uint16_t address, uint16_t quantity);
        bool setDigitalInputs(const uint8_t inputs[], uint16_t address, uint16_t quantity);
        bool setAnalogInputs(const uint16_t inputs[], uint16_t address, uint16_t quantity);
        uint16_t *getAnalogInputs();
        bool getParametersAtServer(uint16_t parameters[], uint16_t address, uint16_t quantity);
        bool setParametersOnServer(const uint16_t parameters[], uint16_t address, uint16_t quantity);
        uint16_t *getParametersArray();

    private:
        // Serial read and write function pointers
        int32_t (*m_serialRead)(const char port[], uint8_t *, uint16_t, int32_t);
        int32_t (*m_serialWrite)(const char port[], const uint8_t *, uint16_t, int32_t);

        // Serial port
        char m_port[20];

        // Modbus client and server objects
        ModbusClient *m_client;
        ModbusServer *m_server;

        // nmbs_t objects for client and server
        nmbs_t m_nmbsClient;
        nmbs_t m_nmbsServer;

        // Read and write via serial
        static int32_t readSerial(uint8_t *buf, uint16_t count, int32_t byteTimeoutMs, void *arg);
        static int32_t writeSerial(const uint8_t *buf, uint16_t count, int32_t byteTimeoutMs, void *arg);
};

#endif // MODBUS_H
