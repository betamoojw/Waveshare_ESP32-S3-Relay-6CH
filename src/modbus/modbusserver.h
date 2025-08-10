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

    private:
        // Server handle
        nmbs_t *m_server; /**< Pointer to the nmbs_t server handle. Not owned by this class. */
};

#endif // MODBUS_SERVER_H
