#ifndef COM_INTERFACE_H
#define COM_INTERFACE_H

#include <string>

class CommunicationInterface 
{
public:
    // Constructor
    CommunicationInterface() = default;

    // Virtual destructor to ensure proper cleanup of derived classes
    virtual ~CommunicationInterface() = default;

    // Pure virtual function to initialize the communication interface
    virtual void initialize() = 0;

    // Pure virtual function to send data
    virtual void sendData(const std::string& data) = 0;
    virtual void sendData(uint8_t* data, size_t length) = 0;

    // Pure virtual function to receive data
    virtual std::string receiveData() = 0;
    virtual void receiveData(uint8_t* buf, uint8_t length) = 0;

    // Pure virtual function to check the connection status
    virtual bool isConnected() const = 0;
};

#endif // COM_INTERFACE_H