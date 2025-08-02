#include "XctrlProtocol.h"
#include "utils.h"

#include "peripherals/relay_control.h"
#include <Logger.h>
#include <iostream>
#include <string>

XctrlProtocol xctrlProtocol; // Create an instance of XctrlProtocol


// Constructor
XctrlProtocol::XctrlProtocol() : comm(nullptr) 
{
    std::cout << "Constructor called." << std::endl;
}

// Destructor
XctrlProtocol::~XctrlProtocol() 
{
    disconnect();
    std::cout << "Destructor called." << std::endl;
}

// Initialize the protocol
void XctrlProtocol::initialize() 
{
    if (comm) 
    {
        comm->initialize();
        std::cout << "Initialized with communication interface." << std::endl;
    } 
    else 
    {
        std::cout << "No communication interface connected." << std::endl;
    }
}

// Encode data into protocol-specific format
std::string XctrlProtocol::encode(const std::string& data) 
{
    return data;
}

// Decode data from protocol-specific format
std::string XctrlProtocol::decode(const std::string& data) 
{
    if (data.find("[Xctrl] ") == 0) 
    {
        return data.substr(8);
    }
    return data;
}

// Handle protocol-specific tasks
void XctrlProtocol::handle() 
{
    std::cout << "Handling protocol-specific tasks." << std::endl;
}

// Connect to a communication interface
void XctrlProtocol::connect(CommunicationInterface* comm) 
{
    this->comm = comm;
    std::cout << "Connected to communication interface." << std::endl;
}

// Disconnect from the communication interface
void XctrlProtocol::disconnect() 
{
    if (comm) 
    {
        comm = nullptr;
        std::cout << "Disconnected from communication interface." << std::endl;
    }
}

// Send data as a string packet
void XctrlProtocol::sendData(const std::string& packet)
{
    if (comm) 
    {
        std::string encodedPacket = encode(packet);
        comm->sendData(encodedPacket);
    }
}

// Send data as a byte array
void XctrlProtocol::sendData(uint8_t* data, size_t length) 
{
    if (comm) 
    {
        comm->sendData(data, length);
        std::cout << "Sent binary data of length " << length << "." << std::endl;
    } 
    else 
    {
        std::cout << "No communication interface connected." << std::endl;
    }
}

// Receive data as a string
std::string XctrlProtocol::receiveData() 
{
    if (comm) 
    {
        std::string received = comm->receiveData();
        std::cout << "Received data - " << received << std::endl;
        return decode(received);
    }
    std::cout << "No communication interface connected." << std::endl;
    return "";
}

// Receive data into a buffer
void XctrlProtocol::receiveData(uint8_t* buf, uint8_t length) 
{
    if (comm) 
    {
        comm->receiveData(buf, length);
        std::cout << "Received binary data of length " << (int)length << "." << std::endl;
    } 
    else 
    {
        std::cout << "No communication interface connected." << std::endl;
    }
}

// Show help information for the protocol
void XctrlProtocol::showHelp() 
{
    char channel_char = RELAY_CHANNEL_COUNT - 1 + 'a';
    
    LOG_I(TAG, "Help - This protocol uses the XCTRL format for communication.\n");
    LOG_I(TAG, "-- [Xctrl] sa switch <channel> 0-1: set (1) / reset (0) channel a-%c", channel_char);
    LOG_I(TAG, "-- [Xctrl] sa toggle <channel>: toggle channel a-%c", channel_char);
    LOG_I(TAG, "-- [Xctrl] sa test mode: Test all channels one after the other.");

    std::string packet = "Help - This protocol uses the XCTRL format for communication.\n";
    packet += stringFormat("-- [Xctrl] sa switch <channel> 0-1: set (1) / reset (0) channel a-%c\n", channel_char);
    packet += stringFormat("-- [Xctrl] sa toggle <channel>: toggle channel a-%c\n", channel_char);
    packet += "-- [Xctrl] sa test mode: Test all channels one after the other.\n";

    sendData(packet);
}


bool XctrlProtocol::processMsg(const std::string& msg)
{
    bool ret = false;

    if (msg.substr(0, 2) == "sa")
    {
        ret = true;
    }

    if (true == ret)
    {
        if (msg.length() == 12 && msg.substr(0, 12) == "sa test mode")
        {
            relay_control_test_mode();
        }
        else if (msg.length() == 7 && msg.substr(0, 7) == "sa help")
        {
            showHelp();
        }
        else if (msg.length() == 13 && msg.substr(0, 10) == "sa switch ")
        {
            uint8_t chIdx = msg.at(10) - 'a';
            uint8_t value = std::stoi(msg.substr(12, 1));
            std::string packet;

            RelayControl& relayCtrl = RelayControl::getInstance();
            uint8_t chICount = relayCtrl.getChannelCount();

            if(chIdx > chICount - 1 || (value != 0 && value != 1))
            {
                packet = "wrong sytnax of command sa switch";
                LOG_I(TAG, packet.c_str());
                sendData(packet);
            }

            relayCtrl.setChannel(chIdx, value);
            packet = stringFormat("Switch Channel %c to %s\n", 
                                  chIdx +'a',
                                  relayCtrl.getChannel(chIdx) ? "ON" : "OFF");
            
            LOG_I(TAG, packet.c_str());
            sendData(packet);
        }
        else if (msg.length() == 11 && msg.substr(0, 10) == "sa toggle ")
        {
            uint8_t chIdx = msg.at(10) - 'a';
            std::string packet;

            RelayControl& relayCtrl = RelayControl::getInstance();
            uint8_t chICount = relayCtrl.getChannelCount();

            if(chIdx > (chICount - 1))
            {
                packet = "wrong sytnax of command sa toggle";
                LOG_I(TAG, packet.c_str());
                sendData(packet);
            }

            relayCtrl.toggleChannel(chIdx);
            packet = stringFormat("Switch Channel %c to %s\n", 
                                  chIdx +'a',
                                  relayCtrl.getChannel(chIdx) ? "ON" : "OFF");
            
            LOG_I(TAG, packet.c_str());
            sendData(packet);
        }
        else if (msg.length() == 15 && msg.substr(0, 14) == "sa switch all ")
        {
            int state = msg[14] - '0';

            RelayControl& relayCtrl = RelayControl::getInstance();
            std::string packet;

            if (state == 0)
            {
                relayCtrl.allOff();
            }
            else if (state == 1)
            {
                relayCtrl.allOn();
            }
            else
            {
                packet = "Invalid state for sa switch all command. Use 0 or 1.\n";
                LOG_I(TAG, packet.c_str());
                sendData(packet);
            }

            packet = relayCtrl.printStatus();
            LOG_I(TAG, packet.c_str());
            sendData(packet);
        }
        
    } 
    else 
    {
        // LOG_W(TAG, "Unknown command - " + msg);
    }

    return ret;
}

void XctrlProtocol::relay_control_test_mode()
{   
    std::string packet = "Test mode starting for all relays.\n";
    
    LOG_I(TAG, packet.c_str());
    sendData(packet);

    // Run test mode for all relays
    LOG_I(TAG, "Running test mode for all relays...\n");


    RelayControl& relayCtrl = RelayControl::getInstance();
    for (uint8_t idx = 0; idx < relayCtrl.getChannelCount(); idx++)
    {
        relayCtrl.setChannel(idx, true);
        delay(500);
        relayCtrl.setChannel(idx, false);
        delay(500);
    }
    // Turn all relays on and then off
    relayCtrl.allOn();
    delay(1000);
    relayCtrl.allOff();

    packet = "Test mode completed for all relays.\n";
    
    LOG_I(TAG, packet.c_str());
    sendData(packet);
}
void XctrlProtocol::relay_control_set_all(int state)
{
    LOG_I(TAG, "Setting all relays to state " + String(state));
}

void XctrlProtocol::relay_control_set(int channel, int state)
{
    LOG_I(TAG, "Setting relay " + String(channel) + " to state " + String(state));
}

void XctrlProtocol::relay_control_toggle(int channel)
{
    RelayControl& relayCtrl = RelayControl::getInstance();
    relayCtrl.toggleChannel(channel);
    bool status = relayCtrl.getChannel(channel);

    std::string packet = stringFormat("Toggle relay - %d; new status %s\n", channel, status ? "ON" : "OFF");

    LOG_I(TAG, packet.c_str());
    sendData(packet);
}