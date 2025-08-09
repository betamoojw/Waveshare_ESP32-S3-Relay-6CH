#include "modbusclient.h"
#include "modbus.h"

// Constructor implementation
ModbusClient::ModbusClient(nmbs_t *handle) : m_nmbs_client(handle)
{
    // Optionally, you could add validation here, e.g.:
    if (m_nmbs_client == nullptr)
    {
        // Log an error or throw an exception if a null handle is not allowed
    }
}

// Helper function implementation
size_t ModbusClient::get_byte_count_for_bits(uint16_t quantity)
{
    return (quantity + 7) / 8; // Integer division for ceiling
}

bool ModbusClient::getDigitalOutputs(std::vector<uint8_t> &outputs, uint16_t address, uint16_t quantity)
{
    if (m_nmbs_client == nullptr)
    {
        // Client handle not set, cannot perform operation
        outputs.clear(); // Ensure output vector is clear on failure
        return false;
    }
    if (quantity == 0)
    {
        outputs.clear(); // Nothing to read, return success
        return true;
    }

    // Allocate a temporary buffer for the raw bitfield response from the nmbs library.
    // The nmbs_read_coils function expects a uint8_t* buffer.
    size_t buffer_size = get_byte_count_for_bits(quantity);
    std::vector<uint8_t> coils_out_buffer(buffer_size); // std::vector initializes elements to zero by default

    nmbs_error err = nmbs_read_coils(m_nmbs_client, address, quantity, coils_out_buffer.data());
    if (err != NMBS_ERROR_NONE)
    {
        outputs.clear(); // Ensure output vector is clear on failure
        return false;
    }

    // Resize the output vector to hold 'quantity' individual boolean values (0 or 1).
    outputs.resize(quantity);
    for (uint16_t i = 0; i < quantity; ++i)
    {
        // Corrected logic: Read the i-th bit from the received buffer.
        // The 'address' parameter is for the Modbus request, not for indexing the received buffer.
        bool value = nmbs_bitfield_read(coils_out_buffer.data(), i);
        outputs[i] = value ? 1 : 0; // Store as 0 or 1
    }

    return true;
}

bool ModbusClient::getDigitalInputs(std::vector<uint8_t> &inputs, uint16_t address, uint16_t quantity)
{
    if (m_nmbs_client == nullptr)
    {
        inputs.clear();
        return false;
    }
    if (quantity == 0)
    {
        inputs.clear();
        return true;
    }

    size_t buffer_size = get_byte_count_for_bits(quantity);
    std::vector<uint8_t> inputs_out_buffer(buffer_size);

    nmbs_error err = nmbs_read_discrete_inputs(m_nmbs_client, address, quantity, inputs_out_buffer.data());
    if (err != NMBS_ERROR_NONE)
    {
        inputs.clear();
        return false;
    }

    inputs.resize(quantity);
    for (uint16_t i = 0; i < quantity; ++i)
    {
        // Corrected logic: Read the i-th bit from the received buffer.
        bool value = nmbs_bitfield_read(inputs_out_buffer.data(), i);
        inputs[i] = value ? 1 : 0;
    }

    return true;
}

bool ModbusClient::getAnalogInputs(std::vector<uint16_t> &inputs, uint16_t address, uint16_t quantity)
{
    if (m_nmbs_client == nullptr)
    {
        inputs.clear();
        return false;
    }
    if (quantity == 0)
    {
        inputs.clear();
        return true;
    }

    inputs.resize(quantity); // Resize vector to hold the expected quantity of 16-bit registers
    // The nmbs_read_input_registers function expects a pointer to uint16_t array
    return nmbs_read_input_registers(m_nmbs_client, address, quantity, inputs.data()) == NMBS_ERROR_NONE;
}

bool ModbusClient::setParameters(const std::vector<uint16_t> &parameters, uint16_t address)
{
    if (m_nmbs_client == nullptr)
    {
        return false;
    }
    uint16_t quantity = static_cast<uint16_t>(parameters.size());
    if (quantity == 0)
    {
        return true; // Nothing to write, return success
    }

    // The nmbs_write_multiple_registers function expects a pointer to const uint16_t array
    return nmbs_write_multiple_registers(m_nmbs_client, address, quantity, parameters.data()) == NMBS_ERROR_NONE;
}

bool ModbusClient::getParameters(std::vector<uint16_t> &parameters, uint16_t address, uint16_t quantity)
{
    if (m_nmbs_client == nullptr)
    {
        parameters.clear();
        return false;
    }
    if (quantity == 0)
    {
        parameters.clear();
        return true;
    }

    parameters.resize(quantity); // Resize vector to hold the expected quantity of 16-bit registers
    // The nmbs_read_holding_registers function expects a pointer to uint16_t array
    return nmbs_read_holding_registers(m_nmbs_client, address, quantity, parameters.data()) == NMBS_ERROR_NONE;
}
