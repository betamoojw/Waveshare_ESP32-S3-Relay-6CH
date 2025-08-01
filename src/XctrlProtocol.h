#ifndef XCTRL_PROTOCOL_H
#define XCTRL_PROTOCOL_H

#include "Protocol.h"
#include "peripherals/ComInterface.h"
#include "peripherals/UARTCommunication.h"
#include <string>

class XctrlProtocol : public Protocol 
{
public:
    // Constructor
    XctrlProtocol();

    // Destructor
    ~XctrlProtocol() override;

    // Override Protocol methods
    void initialize() override;
    std::string encode(const std::string& data) override;
    std::string decode(const std::string& data) override;
    void handle() override;
    void connect(CommunicationInterface* comm) override;
    void disconnect() override;
    void sendData(std::string packet) override;
    void sendData(uint8_t* data, size_t length) override;
    std::string receiveData() override;
    void receiveData(uint8_t* buf, uint8_t length) override;
    void showHelp() override;

private:
    CommunicationInterface* comm; // Pointer to the communication interface
};

extern  XctrlProtocol xctrlProtocol;

#endif // XCTRL_PROTOCOL_H