#ifndef MODBUS_SERVER_H
#define MODBUS_SERVER_H

#include "nanomodbus/nanomodbus.h"

#include <cstdint>

class ModbusServer
{
    public:
        // Constructor
        ModbusServer();

        // Destructor
        ~ModbusServer();

        // Server polling
        bool polling();

        // Server handle setter
        void setServerHandle(nmbs_t *handle);

        // Digital Outputs
        bool setDigitalOutputs(const uint8_t outputs[], uint16_t address, uint16_t quantity);

        // Digital Inputs
        bool setDigitalInputs(const uint8_t inputs[], uint16_t address, uint16_t quantity);

        // Analog Inputs
        bool setAnalogInputs(const uint16_t inputs[], uint16_t address, uint16_t quantity);
        uint16_t *getAnalogInputs();

        // Parameters
        bool getParametersAtServer(uint16_t parameters[], uint16_t address, uint16_t quantity);
        bool setParametersOnServer(const uint16_t parameters[], uint16_t address, uint16_t quantity);
        uint16_t *getParametersOnServer();

        // Device Identification Handlers
        nmbs_error handleReadDeviceIdentification(uint8_t object_id, char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH]);
        static nmbs_error handleReadDeviceIdentificationStatic(uint8_t object_id, char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH])
        {
            ModbusServer *server;
            return server->handleReadDeviceIdentification(object_id, buffer);
        }

        nmbs_error handleReadDeviceIdentificationMap(nmbs_bitfield_256 map);
        static nmbs_error handleReadDeviceIdentificationMapStatic(uint8_t* map, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleReadDeviceIdentificationMap(map);
        }

        // File Record Handlers
        nmbs_error handleWriteFileRecord(uint16_t file_number, uint16_t record_number, const uint16_t *registers, uint16_t count, uint8_t unit_id, void *arg);
        static nmbs_error handleWriteFileRecordStatic(uint16_t file_number, uint16_t record_number, const uint16_t *registers, uint16_t count, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleWriteFileRecord(file_number, record_number, registers, count, unit_id, arg);
        }

        nmbs_error handleReadFileRecord(uint16_t file_number, uint16_t record_number, uint16_t *registers, uint16_t count, uint8_t unit_id, void *arg);
        static nmbs_error handleReadFileRecordStatic(uint16_t file_number, uint16_t record_number, uint16_t *registers, uint16_t count, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleReadFileRecord(file_number, record_number, registers, count, unit_id, arg);
        }

        // Register Handlers
        nmbs_error handleWriteMultipleRegisters(uint16_t address, uint16_t quantity, const uint16_t *registers, uint8_t unit_id, void *arg);
        static nmbs_error handleWriteMultipleRegistersStatic(uint16_t address, uint16_t quantity, const uint16_t *registers, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleWriteMultipleRegisters(address, quantity, registers, unit_id, arg);
        }

        nmbs_error handleWriteSingleRegister(uint16_t address, uint16_t value, uint8_t unit_id, void *arg);
        static nmbs_error handleWriteSingleRegisterStatic(uint16_t address, uint16_t value, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleWriteSingleRegister(address, value, unit_id, arg);
        }

        // Coil Handlers
        nmbs_error handleWriteMultipleCoils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void *arg);
        static nmbs_error handleWriteMultipleCoilsStatic(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleWriteMultipleCoils(address, quantity, coils, unit_id, arg);
        }

        nmbs_error handleWriteSingleCoil(uint16_t address, bool value, uint8_t unit_id, void *arg);
        static nmbs_error handleWriteSingleCoilStatic(uint16_t address, bool value, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleWriteSingleCoil(address, value, unit_id, arg);
        }

        // Read Handlers
        nmbs_error handleReadInputRegisters(uint16_t address, uint16_t quantity, uint16_t *registers_out, uint8_t unit_id, void *arg);
        static nmbs_error handleReadInputRegistersStatic(uint16_t address, uint16_t quantity, uint16_t *registers_out, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleReadInputRegisters(address, quantity, registers_out, unit_id, arg);
        }

        nmbs_error handleReadHoldingRegisters(uint16_t address, uint16_t quantity, uint16_t *registers_out, uint8_t unit_id, void *arg);
        static nmbs_error handleReadHoldingRegistersStatic(uint16_t address, uint16_t quantity, uint16_t *registers_out, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleReadHoldingRegisters(address, quantity, registers_out, unit_id, arg);
        }

        nmbs_error handleReadDiscreteInputs(uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out, uint8_t unit_id, void *arg);
        static nmbs_error handleReadDiscreteInputsStatic(uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleReadDiscreteInputs(address, quantity, inputs_out, unit_id, arg);
        }

        nmbs_error handleReadCoils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void *arg);
        static nmbs_error handleReadCoilsStatic(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void *arg)
        {
            ModbusServer *server = static_cast<ModbusServer *>(arg);
            return server->handleReadCoils(address, quantity, coils_out, unit_id, arg);
        }

    private:
        // Sizes
        static const uint16_t COILS_ADDR_MAX = 0;
        static const uint16_t INPUTS_ADDR_MAX = 0;
        static const uint16_t HOLDING_REGISTERS_ADDR_MAX = 48;
        static const uint16_t INPUT_REGISTERS_ADDR_MAX = 13;
        static const uint16_t FILE_SIZE_MAX = 0;

        // Server handle
        nmbs_t *m_server; /**< Pointer to the nmbs_t server handle. Not owned by this class. */

        // Memory arrays
        nmbs_bitfield m_serverCoils;
        nmbs_bitfield m_serverInputs;
        uint16_t m_serverHoldingRegisters[HOLDING_REGISTERS_ADDR_MAX + 1];
        uint16_t m_serverInputRegisters[INPUT_REGISTERS_ADDR_MAX + 1];
        uint16_t m_serverFile[FILE_SIZE_MAX];
};

#endif // MODBUS_SERVER_H
