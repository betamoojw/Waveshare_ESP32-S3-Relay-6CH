#ifndef MICROMODBUS_H
#define MICROMODBUS_H


#include <cstdint>
#include <functional>

#include "nanomodbus.h"

class MicroModbus
{
    public:
        // Callback types
        using ReadCallback = std::function<int32_t(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg)>;
        using WriteCallback = std::function<int32_t(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg)>;

        using ReadCoilsCallback = std::function<nmbs_error(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg)>;
        using WriteMultipleCoilsCallback = std::function<nmbs_error(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void* arg)>;

        using ReadHoldingRegistersCallback = std::function<nmbs_error(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg)>;
        using WriteMultipleRegistersCallback = std::function<nmbs_error(uint16_t address, uint16_t quantity, const uint16_t* registers, uint8_t unit_id, void* arg)>;

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
        void configureCallbacks(ReadCallback readCb, 
                                WriteCallback writeCb, 
                                ReadCoilsCallback readCoilsCb, 
                                WriteMultipleCoilsCallback writeMultipleCoilsCb,
                                ReadHoldingRegistersCallback readHoldingRegistersCb, 
                                WriteMultipleRegistersCallback writeMultipleRegistersCb);     

        // Client/Master operations
        void setDestinationRtuAddress(uint8_t address);
        ExceptionCode readCoils(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out);
        ExceptionCode readDiscreteInputs(uint16_t address, uint16_t quantity, nmbs_bitfield inputs_out);
        ExceptionCode readHoldingRegisters(uint16_t address, uint16_t quantity, uint16_t* registers_out);
        ExceptionCode readInputRegisters(uint16_t address, uint16_t quantity, uint16_t* registers_out);
        ExceptionCode writeSingleCoil(uint16_t address, bool value);
        ExceptionCode writeSingleRegister(uint16_t address, uint16_t value);
        ExceptionCode writeMultipleCoils(uint16_t address, uint16_t quantity, const nmbs_bitfield coils);
        ExceptionCode writeMultipleRegisters(uint16_t address, uint16_t quantity, const uint16_t* registers);

        // Server/Slave operations
        void setSlaveDataHandler(
            std::function<ExceptionCode(uint8_t function, uint16_t address, uint16_t quantity, uint16_t *data)> handler);
        void poll();

    private:
        bool m_isServer;
        bool m_isTcp;
        nmbs_t m_nmbs;
        nmbs_platform_conf m_platformConf;

        // Internal callback wrappers for serial read/write
        static int32_t staticReadCallback(uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);
        static int32_t staticWriteCallback(const uint8_t* buf, uint16_t count, int32_t byte_timeout_ms, void* arg);

        // Instance callbacks for serial read/write
        ReadCallback m_readCallback;
        WriteCallback m_writeCallback;

        // Internal callback wrappers for handling read/write coils
        static nmbs_error staticHandleReadCoilsCallback(uint16_t address, uint16_t quantity, nmbs_bitfield coils_out, uint8_t unit_id, void* arg);
        static nmbs_error staticHandleWriteMultipleCoilsCallback(uint16_t address, uint16_t quantity, const nmbs_bitfield coils, uint8_t unit_id, void* arg);

        // Instance callbacks for handling read/write coils
        ReadCoilsCallback m_readCoilsCallback;
        WriteMultipleCoilsCallback m_writeMultipleCoilsCallback;
        
        // Internal callback wrappers for handling read/write registers
        static nmbs_error staticHandleReadHoldingRegistersCallback(uint16_t address, uint16_t quantity, uint16_t* registers_out, uint8_t unit_id, void* arg);
        static nmbs_error staticHandleWriteMultipleRegistersCallback(uint16_t address, uint16_t quantity, const uint16_t* registers, uint8_t unit_id, void* arg);

        // Instance callbacks for handling read/write registers
        ReadHoldingRegistersCallback m_readHoldingRegistersCallback;
        WriteMultipleRegistersCallback m_writeMultipleRegistersCallback;

        // Slave data handler
        std::function<ExceptionCode(uint8_t, uint16_t, uint16_t, uint16_t *)> m_slaveDataHandler;

        // Convert between error types
        static ExceptionCode convertError(nmbs_error error);
        static nmbs_error convertException(ExceptionCode code);

        // onError handler
        void onError();
};

#endif // MICROMODBUS_H