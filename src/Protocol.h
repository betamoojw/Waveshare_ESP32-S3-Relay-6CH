#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>
#include "peripherals/ComInterface.h"   

class Protocol 
{
public:
    // Virtual destructor to ensure proper cleanup of derived classes
    virtual ~Protocol() = default;

    // Pure virtual function to initialize the protocol
    // This method is used to set up the protocol before use.
    virtual void initialize() = 0;

    // Pure virtual function to encode data
    // Encodes the given data into a protocol-specific format.
    virtual std::string encode(const std::string& data) = 0;

    // Pure virtual function to decode data
    // Decodes the given data from a protocol-specific format.
    virtual std::string decode(const std::string& data) = 0;

    // Pure virtual function to handle protocol-specific tasks
    // This method is used to process protocol-specific operations.
    virtual void handle() = 0;

    // Connect to a communication interface
    // This allows the protocol to use a specific communication interface (e.g., UART, I2C, etc.).
    virtual void connect(CommunicationInterface* comm) = 0;

    // Disconnect from the communication interface
    // This ensures the protocol releases any resources associated with the communication interface.
    virtual void disconnect() = 0;

    // Send data as a string packet
    // This method allows sending protocol-specific packets as strings.
    virtual void sendData(std::string packet) = 0;

    // Send data as a byte array
    // This method allows sending raw binary data.
    virtual void sendData(uint8_t* data, size_t length) = 0;

    // Receive data as a string
    // This method retrieves protocol-specific packets as strings.
    virtual std::string receiveData() = 0;

    // Receive data into a buffer
    // This method retrieves raw binary data into a provided buffer.
    virtual void receiveData(uint8_t* buf, uint8_t length) = 0;

    // Show help information for the protocol
    // This method provides details about the protocol's usage and supported commands.
    virtual void showHelp() = 0;
};

#endif // PROTOCOL_H