#include "ModbusServer.h"
#include <cstring>

ModbusServer::ModbusServer() : m_server(nullptr)
{
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
