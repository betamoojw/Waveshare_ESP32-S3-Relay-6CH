#ifndef MODBUS_CLIENT_H
#define MODBUS_CLIENT_H

#include "nanomodbus/nanomodbus.h"

#include <cstdint>
#include <vector>

// Forward declaration of nmbs_t if it's an opaque struct in the C header
// struct nmbs_t; // Uncomment if only forward declares it without typedef

/**
 * @brief C++ wrapper class for Modbus client operations using the nmbs library.
 *
 * This class encapsulates the Modbus client functionality, providing a more
 * object-oriented interface compared to the original C functions. It manages
 * the underlying nmbs_t handle, which is expected to be provided externally.
 */
class ModbusClient
{
    public:
        /**
         * @brief Constructs a ModbusClient instance.
         * @param handle A pointer to an initialized nmbs_t client handle.
         *               The ModbusClient class does not take ownership of this handle.
         *               It is the caller's responsibility to manage its lifecycle.
         */
        explicit ModbusClient(nmbs_t *handle);

        // Prevent default construction, copy, and move operations to avoid
        // issues with the raw nmbs_t* pointer and external handle management.
        ModbusClient() = delete;
        ModbusClient(const ModbusClient &) = delete;
        ModbusClient &operator=(const ModbusClient &) = delete;
        ModbusClient(ModbusClient &&) = delete;
        ModbusClient &operator=(ModbusClient &&) = delete;

        // Destructor (default, as it doesn't own m_nmbs_client)
        ~ModbusClient() = default;

        /**
         * @brief Reads digital output (coil) states from the Modbus server.
         * @param[out] outputs A vector to store the read coil states (0 for OFF, 1 for ON).
         *                     The vector will be resized to 'quantity'.
         * @param[in] address The starting Modbus address of the coils.
         * @param[in] quantity The number of coils to read.
         * @return true on success, false on Modbus communication error or if the client handle is invalid.
         */
        bool getDigitalOutputs(std::vector<uint8_t> &outputs, uint16_t address, uint16_t quantity);

        /**
         * @brief Reads digital input (discrete input) states from the Modbus server.
         * @param[out] inputs A vector to store the read discrete input states (0 for OFF, 1 for ON).
         *                    The vector will be resized to 'quantity'.
         * @param[in] address The starting Modbus address of the discrete inputs.
         * @param[in] quantity The number of discrete inputs to read.
         * @return true on success, false on Modbus communication error or if the client handle is invalid.
         */
        bool getDigitalInputs(std::vector<uint8_t> &inputs, uint16_t address, uint16_t quantity);

        /**
         * @brief Reads analog input (input register) values from the Modbus server.
         * @param[out] inputs A vector to store the read input register values.
         *                    The vector will be resized to 'quantity'.
         * @param[in] address The starting Modbus address of the input registers.
         * @param[in] quantity The number of input registers to read.
         * @return true on success, false on Modbus communication error or if the client handle is invalid.
         */
        bool getAnalogInputs(std::vector<uint16_t> &inputs, uint16_t address, uint16_t quantity);

        /**
         * @brief Writes multiple holding registers (parameters) to the Modbus server.
         * @param[in] parameters A constant reference to a vector containing the 16-bit values to write.
         *                       The number of registers written is determined by parameters.size().
         * @param[in] address The starting Modbus address to write the parameters to.
         * @return true on success, false on Modbus communication error or if the client handle is invalid.
         */
        bool setParameters(const std::vector<uint16_t> &parameters, uint16_t address);

        /**
         * @brief Reads multiple holding registers (parameters) from the Modbus server.
         * @param[out] parameters A vector to store the read 16-bit values.
         *                        The vector will be resized to 'quantity'.
         * @param[in] address The starting Modbus address of the holding registers.
         * @param[in] quantity The number of holding registers to read.
         * @return true on success, false on Modbus communication error or if the client handle is invalid.
         */
        bool getParameters(std::vector<uint16_t> &parameters, uint16_t address, uint16_t quantity);

    private:
        nmbs_t *m_nmbs_client; /**< Pointer to the nmbs_t client handle. Not owned by this class. */

        /**
         * @brief Helper function to calculate the number of bytes required to store a given quantity of bits.
         * @param quantity The number of bits.
         * @return The number of bytes required.
         */
        static size_t get_byte_count_for_bits(uint16_t quantity);
};

#endif // MODBUS_CLIENT_H
