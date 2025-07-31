#include <Arduino.h>
#include <Logger.h>
#include <Relay.h>
#include "relay_control.h"


RelayControl& RelayControl::getInstance() 
{
    static RelayControl instance; // Guaranteed to be destroyed and instantiated on first use
    return instance;
}

// Private constructor
RelayControl::RelayControl() 
{
    initRelays(); // Initialize relays during construction
}

void RelayControl::setChannel(uint8_t channel, bool state) 
{
    if (channel < RELAY_CHANNEL_COUNT) 
    {
        relays[channel].setState(state);
    }
}

bool RelayControl::getChannel(uint8_t channel) 
{
    if (channel < RELAY_CHANNEL_COUNT) 
    {
        return relays[channel].getState();
    }
    return false; // Return false if the channel is out of range
}

void RelayControl::toggleChannel(uint8_t channel) 
{
    if (channel < RELAY_CHANNEL_COUNT) 
    {
        bool state = !getChannel(channel);
        relays[channel].setState(state);
    }
}

void RelayControl::allOff() 
{
    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; i++) 
    {
        relays[i].setState(false);
    }
}

void RelayControl::allOn() 
{
    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; i++) 
    {
        relays[i].setState(true);
    }
}

std::string RelayControl::printStatus() 
{
    std::string json = "{";
    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; i++) 
    {
        json += "\"Relay" + std::to_string(i + 1) + "\": ";
        json += relays[i].getState() ? "true" : "false";
        if (i < RELAY_CHANNEL_COUNT - 1) 
        {
            json += ", ";
        }
    }
    json += "}";
    return json;
}

void RelayControl::initRelays() 
{
    for (uint8_t i = 0; i < RELAY_CHANNEL_COUNT; i++) 
    {
        relays[i] = Relay(RELAY_PINS[i]);
    }
}

