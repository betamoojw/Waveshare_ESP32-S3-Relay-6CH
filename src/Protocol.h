#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <string>

class Protocol 
{
public:
    // Virtual destructor to ensure proper cleanup of derived classes
    virtual ~Protocol() = default;

    // Pure virtual function to initialize the protocol
    virtual void initialize() = 0;

    // Pure virtual function to encode data
    virtual std::string encode(const std::string& data) = 0;

    // Pure virtual function to decode data
    virtual std::string decode(const std::string& data) = 0;

    // Pure virtual function to handle protocol-specific tasks
    virtual void handle() = 0;
};

#endif // PROTOCOL_H