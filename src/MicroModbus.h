#ifndef MICROMODBUS_H
#define MICROMODBUS_H


#include <cstdint>
#include <functional>

#include "nanomodbus.h"

class MicroModbus
{
    public:
        // Callback types
        using ReadCallback = std::function<int32_t(uint8_t *buffer, uint16_t count, int32_t timeout)>;
        using WriteCallback = std::function<int32_t(const uint8_t *buffer, uint16_t count, int32_t timeout)>;
        using TimeCallback = std::function<uint32_t()>;

        // Modbus exception codes
        enum class ExceptionCode : uint8_t
        {
            NO_ERROR = 0x00,
            ILLEGAL_FUNCTION = 0x01,
            ILLEGAL_DATA_ADDRESS = 0x02,
            ILLEGAL_DATA_VALUE = 0x03,
            SERVER_DEVICE_FAILURE = 0x04,
            ACKNOWLEDGE = 0x05,
            SERVER_DEVICE_BUSY = 0x06,
            NEGATIVE_ACKNOWLEDGE = 0x07,
            MEMORY_PARITY_ERROR = 0x08,
            GATEWAY_PATH_UNAVAILABLE = 0x0A,
            GATEWAY_TARGET_NO_RESPONSE = 0x0B
        };

        // Constructor/Destructor
        MicroModbus(uint8_t slaveAddress = 1, bool isServer = true, bool isTcp = false);
        ~MicroModbus();

        // Configuration
        void configureCallbacks(ReadCallback readCb, WriteCallback writeCb, TimeCallback timeCb);

        // Master operations
        ExceptionCode readHoldingRegisters(uint16_t startAddress, uint16_t quantity, uint16_t *output);
        ExceptionCode writeSingleRegister(uint16_t address, uint16_t value);
        ExceptionCode writeMultipleRegisters(uint16_t startAddress, uint16_t quantity, const uint16_t *values);

        // Slave operations
        void setSlaveDataHandler(
            std::function<ExceptionCode(uint8_t function, uint16_t address, uint16_t quantity, uint16_t *data)> handler);
        void poll();

    private:
        bool m_isServer;
        bool m_isTcp;
        nmbs_t m_nmbs;
        nmbs_platform_conf m_platformConf;

        // Internal callback wrappers for serial read/write
        static int32_t staticReadCallback(uint8_t *buf, uint16_t count, int32_t timeout, void *context);
        static int32_t staticWriteCallback(const uint8_t *buf, uint16_t count, int32_t timeout, void *context);

        // Instance callbacks for serial read/write
        ReadCallback m_readCallback;
        WriteCallback m_writeCallback;
        TimeCallback m_timeCallback;

        // Internal callback wrappers for serial read/write
        // static int32_t staticHandleReadCoilsCallback(uint8_t *buf, uint16_t count, int32_t timeout, void *context);

        // Slave data handler
        std::function<ExceptionCode(uint8_t, uint16_t, uint16_t, uint16_t *)> m_slaveDataHandler;

        // Convert between error types
        static ExceptionCode convertError(nmbs_error error);
        static nmbs_error convertException(ExceptionCode code);
};

#endif // MICROMODBUS_H