#include <HardwareSerial.h> // ESP32 built-in serial port library
#include "UARTCommunication.h"
#include "board_def.h"

// Static method to get the singleton instance
UARTCommunication& UARTCommunication::getInstance() 
{
    static UARTCommunication instance; // Guaranteed to be destroyed and instantiated on first use
    return instance;
}

// Private constructor
UARTCommunication::UARTCommunication(int uartPort, RS485BaudRate baudRate, int rxdPin, int txdPin)
    : uartPort(uartPort), baudRate(baudRate), rxdPin(rxdPin), txdPin(txdPin), hwSerial(nullptr) {}

// Private destructor
UARTCommunication::~UARTCommunication() 
{
    teardownUART();
}

// Initialize the UART communication
void UARTCommunication::initialize() 
{
    setupUART();
}

// Setup UART
void UARTCommunication::setupUART() 
{
    if (uartPort == 1) 
    {
        hwSerial = &Serial1;
    } 
    else if (uartPort == 2) 
    {
        hwSerial = &Serial2;
    } 
    else 
    {
        hwSerial = &Serial; // Default to Serial0
    }

    if (rxdPin != -1 && txdPin != -1) 
    {
        hwSerial->begin(static_cast<int>(baudRate), SERIAL_8N1, rxdPin, txdPin);
    } 
    else 
    {
        hwSerial->begin(static_cast<int>(baudRate));
    }
}

// Teardown UART
void UARTCommunication::teardownUART() 
{
    if (hwSerial) 
    {
        hwSerial->end();
    }
}

// Send data as a string
void UARTCommunication::sendData(const std::string& data) 
{
    if (hwSerial) 
    {
        hwSerial->write(data.c_str(), data.length());
    }
}

// Send data as a byte array
void UARTCommunication::sendData(uint8_t* data, size_t length) 
{
    if (hwSerial) 
    {
        hwSerial->write(data, length);
    }
}

// Receive data as a string
std::string UARTCommunication::receiveData() 
{
    std::string receivedData;
    if (hwSerial && hwSerial->available()) 
    {
        while (hwSerial->available()) 
        {
            receivedData += static_cast<char>(hwSerial->read());
        }
    }
    return receivedData;
}

// Receive data into a buffer
void UARTCommunication::receiveData(uint8_t* buf, uint8_t length) 
{
    if (hwSerial && hwSerial->available()) 
    {
        hwSerial->readBytes(buf, length);
    }
}

// Check if UART is connected
bool UARTCommunication::isConnected() const 
{
    return hwSerial != nullptr;
}

// Set the baud rate
void UARTCommunication::setBaudRate(RS485BaudRate baudRate) 
{
    this->baudRate = baudRate;
    if (hwSerial) 
    {
        hwSerial->updateBaudRate(static_cast<int>(baudRate));
    }
}