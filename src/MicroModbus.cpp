
#include <cstring>

#include "nanomodbus.h"
#include "MicroModbus.h"

MicroModbus::MicroModbus(uint8_t slaveAddress, bool isServer, bool isTcp) : m_isServer(isServer), m_isTcp(isTcp)
{
    memset(&m_nmbs, 0, sizeof(m_nmbs));
    memset(&m_platformConf, 0, sizeof(m_platformConf));

    nmbs_platform_conf_create(&m_platformConf);

    m_platformConf.read = staticReadCallback;
    m_platformConf.write = staticWriteCallback;
    m_platformConf.arg = NULL;

    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    // callbacks.read_coils = handle_read_coils;
    // callbacks.write_multiple_coils = handle_write_multiple_coils;
    // callbacks.read_holding_registers = handler_read_holding_registers;
    // callbacks.write_multiple_registers = handle_write_multiple_registers;


    // Modbus Server/Slave
    if (m_isServer) 
    {
        // Modbus TCP Transport
        if (m_isTcp)
        {

        }
        // Modbus RTU Transport
        else
        {
            m_platformConf.transport = NMBS_TRANSPORT_RTU;

            nmbs_error err = nmbs_server_create(&m_nmbs, slaveAddress, &m_platformConf, &callbacks);
            if (err != NMBS_ERROR_NONE)
            {
                // onError();
            }
        }
    }
    // Modbus Client/Master
    else
    {
        // Modbus TCP Transport
        if (m_isTcp)
        {

        }
        // Modbus RTU Transport
        else
        {
            m_platformConf.transport = NMBS_TRANSPORT_RTU;

            nmbs_error err = nmbs_client_create(&m_nmbs, &m_platformConf);;
            if (err != NMBS_ERROR_NONE)
            {
                // onError();
            }
        }
    }

    nmbs_set_read_timeout(&m_nmbs, 1000);
    nmbs_set_byte_timeout(&m_nmbs, 100);
}

MicroModbus::~MicroModbus()
{
    // Cleanup if needed
}

void MicroModbus::configureCallbacks(ReadCallback readCb, WriteCallback writeCb, TimeCallback timeCb)
{
    m_readCallback = readCb;
    m_writeCallback = writeCb;
    m_timeCallback = timeCb;
}

MicroModbus::ExceptionCode MicroModbus::readHoldingRegisters(uint16_t startAddress, uint16_t quantity, uint16_t *output)
{
    nmbs_error err = nmbs_read_holding_registers(&m_nmbs, startAddress, quantity, output);
    return convertError(err);
}

MicroModbus::ExceptionCode MicroModbus::writeSingleRegister(uint16_t address, uint16_t value)
{
    nmbs_error err = nmbs_write_single_register(&m_nmbs, address, value);
    return convertError(err);
}

MicroModbus::ExceptionCode MicroModbus::writeMultipleRegisters(uint16_t startAddress, uint16_t quantity, const uint16_t *values)
{
    nmbs_error err = nmbs_write_multiple_registers(&m_nmbs, startAddress, quantity, values);
    return convertError(err);
}

void MicroModbus::setSlaveDataHandler(std::function<ExceptionCode(uint8_t, uint16_t, uint16_t, uint16_t *)> handler)
{
    m_slaveDataHandler = handler;
}

void MicroModbus::poll()
{
    // Modbus Server/Slave
    if (m_isServer) 
    {
        nmbs_error err = nmbs_server_poll(&m_nmbs);

        // poll service led indicator

        // This will probably never happen, since we don't return < 0 in our platform funcs
        if (err == NMBS_ERROR_TRANSPORT)
        {
            // Handle transport error
            // onError();
        }
    }
    // Modbus Client/Master
    else
    {

    }
}

// Static callback wrappers
int32_t MicroModbus::staticReadCallback(uint8_t *buf, uint16_t count, int32_t timeout, void *context)
{
    MicroModbus *instance = static_cast<MicroModbus *>(context);
    if (instance && instance->m_readCallback)
    {
        return instance->m_readCallback(buf, count, timeout);
    }
    return -1;
}

int32_t MicroModbus::staticWriteCallback(const uint8_t *buf, uint16_t count, int32_t timeout, void *context)
{
    MicroModbus *instance = static_cast<MicroModbus *>(context);
    if (instance && instance->m_writeCallback)
    {
        return instance->m_writeCallback(buf, count, timeout);
    }
    return -1;
}

// Error conversion
MicroModbus::ExceptionCode MicroModbus::convertError(nmbs_error error)
{
    return static_cast<ExceptionCode>(error);
}

nmbs_error MicroModbus::convertException(ExceptionCode code)
{
    return static_cast<nmbs_error>(code);
}