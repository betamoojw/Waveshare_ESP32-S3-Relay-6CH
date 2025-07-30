#include "XctrlProtocol.h"
#include <iostream>
#include "relay_control.h"
#include "HWCDC.h"


extern HWCDC USBSerial; // Declaration of the external USBSerial object

// Constructor
XctrlProtocol::XctrlProtocol() : comm(nullptr) 
{
    std::cout << "XctrlProtocol: Constructor called." << std::endl;
}

// Destructor
XctrlProtocol::~XctrlProtocol() 
{
    disconnect();
    std::cout << "XctrlProtocol: Destructor called." << std::endl;
}

// Initialize the protocol
void XctrlProtocol::initialize() 
{
    if (comm) 
    {
        comm->initialize();
        std::cout << "XctrlProtocol: Initialized with communication interface." << std::endl;
    } 
    else 
    {
        std::cout << "XctrlProtocol: No communication interface connected." << std::endl;
    }
}

// Encode data into protocol-specific format
std::string XctrlProtocol::encode(const std::string& data) 
{
    // Example: Add a protocol-specific header
    return "[XCTRL]" + data;
}

// Decode data from protocol-specific format
std::string XctrlProtocol::decode(const std::string& data) 
{
    // Example: Remove the protocol-specific header
    if (data.find("[XCTRL]") == 0) 
    {
        return data.substr(7); // Remove "[XCTRL]"
    }
    return data;
}

// Handle protocol-specific tasks
void XctrlProtocol::handle() 
{
    std::cout << "XctrlProtocol: Handling protocol-specific tasks." << std::endl;
}

// Connect to a communication interface
void XctrlProtocol::connect(CommunicationInterface* comm) 
{
    this->comm = comm;
    std::cout << "XctrlProtocol: Connected to communication interface." << std::endl;
}

// Disconnect from the communication interface
void XctrlProtocol::disconnect() 
{
    if (comm) 
    {
        comm = nullptr;
        std::cout << "XctrlProtocol: Disconnected from communication interface." << std::endl;
    }
}

// Send data as a string packet
void XctrlProtocol::sendData(std::string packet) 
{
    if (comm) 
    {
        std::string encodedPacket = encode(packet);
        comm->sendData(encodedPacket);
        std::cout << "XctrlProtocol: Sent data - " << encodedPacket << std::endl;
    } 
    else 
    {
        std::cout << "XctrlProtocol: No communication interface connected." << std::endl;
    }
}

// Send data as a byte array
void XctrlProtocol::sendData(uint8_t* data, size_t length) 
{
    if (comm) 
    {
        comm->sendData(data, length);
        std::cout << "XctrlProtocol: Sent binary data of length " << length << "." << std::endl;
    } 
    else 
    {
        std::cout << "XctrlProtocol: No communication interface connected." << std::endl;
    }
}

// Receive data as a string
std::string XctrlProtocol::receiveData() 
{
    if (comm) 
    {
        std::string received = comm->receiveData();
        std::cout << "XctrlProtocol: Received data - " << received << std::endl;
        return decode(received);
    }
    std::cout << "XctrlProtocol: No communication interface connected." << std::endl;
    return "";
}

// Receive data into a buffer
void XctrlProtocol::receiveData(uint8_t* buf, uint8_t length) 
{
    if (comm) 
    {
        comm->receiveData(buf, length);
        std::cout << "XctrlProtocol: Received binary data of length " << (int)length << "." << std::endl;
    } 
    else 
    {
        std::cout << "XctrlProtocol: No communication interface connected." << std::endl;
    }
}

// Show help information for the protocol
void XctrlProtocol::showHelp() 
{
    USBSerial.println("XctrlProtocol: Help - This protocol uses the XCTRL format for communication.");

    char msg[256];
    snprintf(msg, sizeof(msg), "sa switch <channel> 0-1: set (1) / reset (0) channel a-%c", RELAY_CHANNEL_COUNT - 1 + 'a');
    USBSerial.println(msg);

    snprintf(msg, sizeof(msg), "sa toggle <channel>: toggle channel a-%c", RELAY_CHANNEL_COUNT - 1 + 'a');
    USBSerial.println(msg);

    USBSerial.println("sa run test mode: Test all channels one after the other.");
}

