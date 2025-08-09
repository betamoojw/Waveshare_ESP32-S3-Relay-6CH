// modbus.cpp
#include "modbus.h"
/*
int main() {
    Modbus modbus;

    // Set serial read and write functions
    modbus.setSerialRead(mySerialReadFunction);
    modbus.setSerialWrite(mySerialWriteFunction);

    // Set serial port
    modbus.setSerialPort("COM1");

    // Create Modbus client in RTU mode
    if (!modbus.createClientRTU(1)) {
        // Error handling
    }

    // Perform Modbus operations
    uint8_t outputs[10];
    if (!modbus.getDigitalOutputs(outputs, 0x0000, 10)) {
        // Error handling
    }

    return 0;
}
*/

Modbus::Modbus() : m_serialRead(nullptr), m_serialWrite(nullptr), m_client(nullptr), m_server(nullptr)
{
    m_nmbsClient = {0};
    m_nmbsServer = {0};
    m_port[0] = '\0';
}

Modbus::~Modbus()
{
    delete m_client;
    delete m_server;
}




void Modbus::setSerialRead(int32_t (*serialRead)(const char port[], uint8_t *, uint16_t, int32_t))
{
    m_serialRead = serialRead;
}

void Modbus::setSerialWrite(int32_t (*serialWrite)(const char port[], const uint8_t *, uint16_t, int32_t))
{
    m_serialWrite = serialWrite;
}

void Modbus::setSerialPort(const char port[])
{
    strcpy(m_port, port);
}




/* Client functions */
bool Modbus::createClientRTU(uint8_t address)
{
    // Create platform configuration
    nmbs_platform_conf platformConf;
    nmbs_platform_conf_create(&platformConf);
    platformConf.transport = NMBS_TRANSPORT_RTU;
    platformConf.read = readSerial;
    platformConf.write = writeSerial;
    platformConf.arg = this;

    // Create client
    nmbs_error err = nmbs_client_create(&m_nmbsClient, &platformConf);
    if (err != NMBS_ERROR_NONE)
    {
        return false;
    }

    // Set time out
    nmbs_set_read_timeout(&m_nmbsClient, 1000);
    nmbs_set_byte_timeout(&m_nmbsClient, 1000);

    // Set address
    nmbs_set_destination_rtu_address(&m_nmbsClient, address);

    // Create Modbus client object
    m_client = new ModbusClient(&m_nmbsClient);

    return true;
}

void Modbus::setClientRTUAddress(uint8_t address)
{
    nmbs_set_destination_rtu_address(&m_nmbsClient, address);
}

bool Modbus::getDigitalOutputs(std::vector<uint8_t> &outputs, uint16_t address, uint16_t quantity)
{
    return m_client->getDigitalOutputs(outputs, address, quantity);
}

bool Modbus::getDigitalInputs(std::vector<uint8_t> &inputs, uint16_t address, uint16_t quantity)
{
    return m_client->getDigitalInputs(inputs, address, quantity);
}

bool Modbus::getAnalogInputs(std::vector<uint16_t> &inputs, uint16_t address, uint16_t quantity)
{
    return m_client->getAnalogInputs(inputs, address, quantity);
}

bool Modbus::setParameters(const std::vector<uint16_t> &parameters, uint16_t address)
{
    return m_client->setParameters(parameters, address);
}

bool Modbus::getParameters(std::vector<uint16_t> &parameters, uint16_t address, uint16_t quantity)
{
    return m_client->getParameters(parameters, address, quantity);
}




/* Server functions */
bool Modbus::createServerRTU(uint8_t address)
{
    // Create platform configuration
    nmbs_platform_conf platformConf;
    nmbs_platform_conf_create(&platformConf);
    platformConf.transport = NMBS_TRANSPORT_RTU;
    platformConf.read = readSerial;
    platformConf.write = writeSerial;
    platformConf.arg = this;

    // Create Modbus server object
    m_server = new ModbusServer();
    
    // Create callbacks
    nmbs_callbacks callbacks;
    nmbs_callbacks_create(&callbacks);
    // Set callbacks ...
    callbacks.read_coils =  ModbusServer::handleReadCoilsStatic;
    callbacks.read_discrete_inputs = m_server->handleReadDiscreteInputsStatic;
	callbacks.read_holding_registers =  m_server->handleReadHoldingRegistersStatic;
	callbacks.read_input_registers = m_server->handleReadInputRegistersStatic;
	callbacks.write_single_coil = m_server->handleWriteSingleCoilStatic;
	callbacks.write_single_register = m_server->handleWriteSingleRegisterStatic;
	callbacks.write_multiple_coils = m_server->handleWriteMultipleCoilsStatic;
	callbacks.write_multiple_registers = m_server->handleWriteMultipleRegistersStatic;
	callbacks.read_file_record = m_server->handleReadFileRecordStatic;
	callbacks.write_file_record = m_server->handleWriteFileRecordStatic;
	// callbacks.read_device_identification_map = m_server->handleReadDeviceIdentificationMapStatic;
	// callbacks.read_device_identification = m_server->handleReadDeviceIdentificationStatic;

    // Create Modbus server
    nmbs_error err = nmbs_server_create(&m_nmbsServer, address, &platformConf, &callbacks);
    if (err != NMBS_ERROR_NONE)
    {
        return false;
    }

    // Set time out
    nmbs_set_read_timeout(&m_nmbsServer, 1000);
    nmbs_set_byte_timeout(&m_nmbsServer, 1000);

    // Set server handle
    m_server->setServerHandle(&m_nmbsServer);

    return true;
}

// ... other server functions ...
bool Modbus::serverPolling()
{
    return m_server->polling();
}

bool Modbus::setDigitalOutputs(const uint8_t outputs[], uint16_t address, uint16_t quantity)
{
    return m_server->setDigitalOutputs(outputs, address, quantity);
}

bool Modbus::setDigitalInputs(const uint8_t inputs[], uint16_t address, uint16_t quantity)
{
    return m_server->setDigitalInputs(inputs, address, quantity);
}

bool Modbus::setAnalogInputs(const uint16_t inputs[], uint16_t address, uint16_t quantity)
{
    return m_server->setAnalogInputs(inputs, address, quantity);
}

uint16_t * Modbus::getAnalogInputs()
{
    return m_server->getAnalogInputs();
}

bool Modbus:: getParametersAtServer(uint16_t parameters[], uint16_t address, uint16_t quantity)
{
    return m_server->getParametersAtServer(parameters, address, quantity);
}

bool Modbus:: setParametersOnServer(const uint16_t parameters[], uint16_t address, uint16_t quantity)
{
    return m_server->setParametersOnServer(parameters, address, quantity);
}

uint16_t * Modbus::getParametersArray()
{
    return m_server->getParametersOnServer();
}


int32_t Modbus::readSerial(uint8_t *buf, uint16_t count, int32_t byteTimeoutMs, void *arg)
{
    Modbus *modbus = static_cast<Modbus *>(arg);
    if (modbus->m_serialRead)
    {
        return modbus->m_serialRead(modbus->m_port, buf, count, byteTimeoutMs);
    }
    return 0;
}

int32_t Modbus::writeSerial(const uint8_t *buf, uint16_t count, int32_t byteTimeoutMs, void *arg)
{
    Modbus *modbus = static_cast<Modbus *>(arg);
    if (modbus->m_serialWrite)
    {
        return modbus->m_serialWrite(modbus->m_port, buf, count, byteTimeoutMs);
    }
    return 0;
}
