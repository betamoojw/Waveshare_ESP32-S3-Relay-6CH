#ifndef UART_COMMUNICATION_H
#define UART_COMMUNICATION_H

#include <string>
#include <HardwareSerial.h> // ESP32 built-in serial port library
#include "ComInterface.h"
#include "board_def.h"

class UARTCommunication : public CommunicationInterface 
{
public:
    // Enumeration for RS485 baud rates
    enum class RS485BaudRate 
    {
        BAUD_9600 = 9600,
        BAUD_19200 = 19200,
        BAUD_38400 = 38400,
        BAUD_57600 = 57600,
        BAUD_115200 = 115200
    };

    // Get the singleton instance
    static UARTCommunication& getInstance();

    // Delete copy constructor and assignment operator to prevent copying
    UARTCommunication(const UARTCommunication&) = delete;
    UARTCommunication& operator=(const UARTCommunication&) = delete;

    // Override CommunicationInterface methods
    void initialize() override;
    void sendData(const std::string& data) override;
    void sendData(uint8_t* data, size_t length) override;
    std::string receiveData() override;
    void receiveData(uint8_t* buf, uint8_t length) override;
    bool isConnected() const override;

    // Set baud rate
    void setBaudRate(RS485BaudRate baudRate);

    // Set timeout
    void setTimeout(int timeout_ms);

private:
    // Private constructor for Singleton
    UARTCommunication(int uartPort = 1, RS485BaudRate baudRate = RS485BaudRate::BAUD_9600, int rxdPin = RXD1, int txdPin = TXD1);

    // Private destructor
    ~UARTCommunication() override;

    int uartPort;                  // UART port number
    RS485BaudRate baudRate;        // Baud rate
    int rxdPin = RXD1;                    // RXD pin
    int txdPin = TXD1;                    // TXD pin
    HardwareSerial* hwSerial;      // Pointer to HardwareSerial instance

    void setupUART();              // Setup UART
    void teardownUART();           // Teardown UART
};

#endif // UART_COMMUNICATION_H