#include "ModbusServer.h"
#include <cstring>

ModbusServer::ModbusServer()
    : m_server(nullptr), m_serverCoils {0}, m_serverInputs {0}, m_serverHoldingRegisters {0}, m_serverInputRegisters {0}
{
    // Initialize all arrays to zero
    std::memset(m_serverHoldingRegisters, 0, sizeof(m_serverHoldingRegisters));
    std::memset(m_serverInputRegisters, 0, sizeof(m_serverInputRegisters));
    std::memset(m_serverFile, 0, sizeof(m_serverFile));
}

ModbusServer::~ModbusServer()
{
    // Any cleanup if needed
}

bool ModbusServer::polling()
{
    if (m_server)
    {
        nmbs_error err = nmbs_server_poll(m_server);
        return (err == NMBS_ERROR_NONE);
    }
    return false;
}

void ModbusServer::setServerHandle(nmbs_t *handle)
{
    m_server = handle;
}

bool ModbusServer::setDigitalOutputs(const uint8_t outputs[], uint16_t address, uint16_t quantity)
{
    if (address + quantity > COILS_ADDR_MAX + 1)
    {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        bool value = nmbs_bitfield_read(outputs, i);
        nmbs_bitfield_write(m_serverCoils, address + i, value);
    }

    return true;
}

bool ModbusServer::setDigitalInputs(const uint8_t inputs[], uint16_t address, uint16_t quantity)
{
    if (address + quantity > INPUTS_ADDR_MAX + 1)
    {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        bool value = nmbs_bitfield_read(inputs, i);
        nmbs_bitfield_write(m_serverInputs, address + i, value);
    }

    return true;
}

bool ModbusServer::setAnalogInputs(const uint16_t inputs[], uint16_t address, uint16_t quantity)
{
    if (address + quantity > INPUT_REGISTERS_ADDR_MAX + 1)
    {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        m_serverInputRegisters[address + i] = inputs[i];
    }

    return true;
}

uint16_t *ModbusServer::getAnalogInputs()
{
    return m_serverInputRegisters;
}

bool ModbusServer::getParametersAtServer(uint16_t parameters[], uint16_t address, uint16_t quantity)
{
    if (address + quantity > HOLDING_REGISTERS_ADDR_MAX + 1)
    {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        parameters[i] = m_serverHoldingRegisters[address + i];
    }

    return true;
}

bool ModbusServer::setParametersOnServer(const uint16_t parameters[], uint16_t address, uint16_t quantity)
{
    if (address + quantity > HOLDING_REGISTERS_ADDR_MAX + 1)
    {
        return false;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        m_serverHoldingRegisters[address + i] = parameters[i];
    }

    return true;
}

uint16_t *ModbusServer::getParametersOnServer()
{
    return m_serverHoldingRegisters;
}

/* (0x0E) Read Device Identification */
nmbs_error ModbusServer::handleReadDeviceIdentification(uint8_t object_id, char buffer[NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH])
{
    if (!m_server)
    {
        return NMBS_ERROR_INVALID_ARGUMENT;
    }

    // Implement device identification logic
    // This is a placeholder and should be customized based on specific requirements
    switch (object_id)
    {
        case 0x00:
            std::strncpy(buffer, "VendorName", NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        case 0x01:
            std::strncpy(buffer, "ProductCode", NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        case 0x02:
            std::strncpy(buffer, "MajorMinorRevision", NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        case 0x90:
            std::strncpy(buffer, "Extended 1", NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        case 0xA0:
            std::strncpy(buffer, "Extended 2", NMBS_DEVICE_IDENTIFICATION_STRING_LENGTH);
            break;
        default:
            return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    return NMBS_ERROR_NONE;
}

/* (0x2B) Read Device Identification Map */
nmbs_error ModbusServer::handleReadDeviceIdentificationMap(nmbs_bitfield_256 map)
{
    if (!m_server)
    {
        return NMBS_ERROR_INVALID_ARGUMENT;
    }

    // Clear the map first
    std::memset(map, 0, sizeof(nmbs_bitfield_256));

    // Set bits for available object IDs
    nmbs_bitfield_set(map, 0x00); // Vendor name
    nmbs_bitfield_set(map, 0x01); // Product code
    nmbs_bitfield_set(map, 0x02); // Major minor revision
    nmbs_bitfield_set(map, 0x90);
    nmbs_bitfield_set(map, 0xA0);

    return NMBS_ERROR_NONE;
}

/* (0x15) Write File Record */
nmbs_error ModbusServer::handleWriteFileRecord(uint16_t file_number,
                                               uint16_t record_number,
                                               const uint16_t *registers,
                                               uint16_t count,
                                               uint8_t unit_id,
                                               void *arg)
{
    if (!m_server || file_number >= FILE_SIZE_MAX)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    // Calculate the starting index in the file
    uint16_t start_index = record_number * count;

    // Check if we're within bounds
    if (start_index + count > FILE_SIZE_MAX)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    // Copy the registers to the file
    std::memcpy(&m_serverFile[start_index], registers, count * sizeof(uint16_t));

    return NMBS_ERROR_NONE;
}

/* (0x14) Read File Record */
nmbs_error ModbusServer::handleReadFileRecord(uint16_t file_number,
                                              uint16_t record_number,
                                              uint16_t *registers,
                                              uint16_t count,
                                              uint8_t unit_id,
                                              void *arg)
{
    if (!m_server || file_number >= FILE_SIZE_MAX)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    // Calculate the starting index in the file
    uint16_t start_index = record_number * count;

    // Check if we're within bounds
    if (start_index + count > FILE_SIZE_MAX)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    // Copy the registers from the file
    std::memcpy(registers, &m_serverFile[start_index], count * sizeof(uint16_t));

    return NMBS_ERROR_NONE;
}

/* (0x10) Write Multiple Registers */
nmbs_error ModbusServer::handleWriteMultipleRegisters(uint16_t address,
                                                      uint16_t quantity,
                                                      const uint16_t *registers,
                                                      uint8_t unit_id,
                                                      void *arg)
{
    if (address + quantity > HOLDING_REGISTERS_ADDR_MAX + 1)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        m_serverHoldingRegisters[address + i] = registers[i];
    }

    return NMBS_ERROR_NONE;
}

/* (0x06) Write Single Register */
nmbs_error ModbusServer::handleWriteSingleRegister(uint16_t address, uint16_t value, uint8_t unit_id, void *arg)
{
    if (address > HOLDING_REGISTERS_ADDR_MAX)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    m_serverHoldingRegisters[address] = value;
    return NMBS_ERROR_NONE;
}

/* (0x0F) Write Multiple Coils */
nmbs_error ModbusServer::handleWriteMultipleCoils(uint16_t address,
                                                  uint16_t quantity,
                                                  const nmbs_bitfield coils,
                                                  uint8_t unit_id,
                                                  void *arg)
{
    if (address + quantity > COILS_ADDR_MAX + 1)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        bool value = nmbs_bitfield_read(coils, i);
        nmbs_bitfield_write(m_serverCoils, address + i, value);
    }

    return NMBS_ERROR_NONE;
}

/* (0x05) Write Single Coil */
nmbs_error ModbusServer::handleWriteSingleCoil(uint16_t address, bool value, uint8_t unit_id, void *arg)
{
    if (address > COILS_ADDR_MAX)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    nmbs_bitfield_write(m_serverCoils, address, value);
    return NMBS_ERROR_NONE;
}

/* (0x04) Read Input Registers */
nmbs_error ModbusServer::handleReadInputRegisters(uint16_t address, uint16_t quantity, uint16_t *registers_out, uint8_t unit_id, void *arg)
{
    if (address + quantity > INPUT_REGISTERS_ADDR_MAX + 1)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        registers_out[i] = m_serverInputRegisters[address + i];
    }

    return NMBS_ERROR_NONE;
}

/* (0x03) Read Holding Registers */
nmbs_error ModbusServer::handleReadHoldingRegisters(uint16_t address,
                                                    uint16_t quantity,
                                                    uint16_t *registers_out,
                                                    uint8_t unit_id,
                                                    void *arg)
{
    if (address + quantity > HOLDING_REGISTERS_ADDR_MAX + 1)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        registers_out[i] = m_serverHoldingRegisters[address + i];
    }

    return NMBS_ERROR_NONE;
}

/* (0x02) Read Discrete Inputs */
nmbs_error ModbusServer::handleReadDiscreteInputs(uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out, uint8_t unit_id, void *arg)
{
    if (address + quantity > INPUTS_ADDR_MAX + 1)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        bool value = nmbs_bitfield_read(m_serverInputs, address + i);
        nmbs_bitfield_write(inputs_out, i, value);
    }

    return NMBS_ERROR_NONE;
}

/* (0x01) Read Coils */
nmbs_error ModbusServer::handleReadCoils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void *arg)
{
    if (address + quantity > COILS_ADDR_MAX + 1)
    {
        return NMBS_EXCEPTION_ILLEGAL_DATA_ADDRESS;
    }

    for (uint16_t i = 0; i < quantity; i++)
    {
        bool value = nmbs_bitfield_read(m_serverCoils, address + i);
        nmbs_bitfield_write(coils_out, i, value);
    }

    return NMBS_ERROR_NONE;
}
