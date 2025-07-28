#include "cli_commands.h"
#include "HWCDC.h"
#include "relay_control.h"
#include "buzzer_control.h"
#include "digital_led_control.h"

extern HWCDC USBSerial; // Declaration of the external USBSerial object

#define CLI_INTERFACE_VERSION "0.0.1"

void cli_command(EmbeddedCli *embeddedCli, CliCommand *command)
{
    USBSerial.println(F("Received command:"));
    USBSerial.println(command->name);
    embeddedCliTokenizeArgs(command->args);
    for (int i = 1; i <= embeddedCliGetTokenCount(command->args); ++i) 
    {
        USBSerial.print(F("arg "));
        USBSerial.print((char) ('0' + i));
        USBSerial.print(F(": "));
        USBSerial.println(embeddedCliGetToken(command->args, i));
    }
}
void cli_clear(EmbeddedCli *cli, char *args, void *context)
{
    USBSerial.println(F("\33[2J"));
}
void cli_version(EmbeddedCli *cli, char *args, void *context)
{
    USBSerial.print("CLI-Version: ");
    USBSerial.println(CLI_INTERFACE_VERSION);
}
void cli_reboot(EmbeddedCli *cli, char *args, void *context)
{
    USBSerial.println("Rebooting...");
    delay(1000);
    ESP.restart();
}
void cli_hello(EmbeddedCli *cli, char *args, void *context)
{
    USBSerial.print(F("Hello "));
    if (args == NULL || embeddedCliGetTokenCount(args) == 0)
    {
        USBSerial.print((const char *)context);
    }
    else
    {
        const char *name = embeddedCliGetToken(args, 1);
        if (name != NULL)
        {
            USBSerial.print(name);
        }
        else
        {
            USBSerial.print(F("stranger"));
        }
    }
    USBSerial.print("\r\n");
}

void cli_get_led_brightness(EmbeddedCli *cli, char *args, void *context)
{
    // Set the LED color using the DigitalLedControl class
    DigitalLedControl& ledControl = DigitalLedControl::getInstance();
    uint8_t color = ledControl.getBrightness();

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[64];
    snprintf(msg, sizeof(msg), "Get LED brightness: %d", color);
    USBSerial.println(msg);
}

void cli_set_led_brightness(EmbeddedCli *cli, char *args, void *context)
{
    if ((args == NULL) || (embeddedCliGetTokenCount(args) < 1))
    {
        USBSerial.println("Usage: set-led-brightness [arg1]");
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);
    uint8_t brightness = constrain(atoi(arg1), 0, 255); // Constrain brightness value

    // Set the LED brightness using the DigitalLedControl class
    DigitalLedControl& ledControl = DigitalLedControl::getInstance();
    ledControl.setBrightness(brightness); 

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[64];
    snprintf(msg, sizeof(msg), "Set LED brightness: %d", brightness);
    USBSerial.println(msg);
}   

void cli_get_led_color(EmbeddedCli *cli, char *args, void *context)
{
    // Set the LED color using the DigitalLedControl class
    DigitalLedControl& ledControl = DigitalLedControl::getInstance();
    uint32_t color = ledControl.getColor();
    uint8_t rVal = (color >> 16) & 0xFF;
    uint8_t gVal = (color >> 8) & 0xFF;
    uint8_t bVal = color & 0xFF;

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[128];
    snprintf(msg, sizeof(msg), "Get LED color: %d, R-%d, G-%d, B-%d", color, rVal, gVal, bVal);
    USBSerial.println(msg);
}

void cli_set_led_color(EmbeddedCli *cli, char *args, void *context)
{
    if ((args == NULL) || (embeddedCliGetTokenCount(args) < 3))
    {
        USBSerial.println("Usage: set-led [arg1] [arg2] [arg3]");
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);
    const char *arg2 = embeddedCliGetToken(args, 2);
    const char *arg3 = embeddedCliGetToken(args, 3);

    uint8_t rVal = constrain(atoi(arg1), 0, 255); // Constrain red value
    uint8_t gVal = constrain(atoi(arg2), 0, 255); // Constrain green value
    uint8_t bVal = constrain(atoi(arg3), 0, 255); // Constrain blue value

    // Set the LED color using the DigitalLedControl class
    DigitalLedControl& ledControl = DigitalLedControl::getInstance();
    uint8_t brightness = ledControl.getBrightness();
    ledControl.setBrightness(brightness);
    ledControl.setColor(rVal, gVal, bVal);

    // Print the result
    char msg[128];
    snprintf(msg, sizeof(msg), "Set LED with RGB values: R-%d, G-%d, B-%d", rVal, gVal, bVal);
    USBSerial.println(msg);
}

void cli_get_relay(EmbeddedCli *cli, char *args, void *context)
{
    RelayControl& relayCtrl = RelayControl::getInstance();

    if (args == NULL)
    {
        std::string status = relayCtrl.printStatus();
        char msg[256];
        snprintf(msg, sizeof(msg), "All Relay Status: %s", status.c_str());
        USBSerial.println(msg);

        return;
    }
    
    const char *arg1 = embeddedCliGetToken(args, 1);
    const uint8_t channel = atoi(arg1);
    // Validate channel number
    if (channel < 0 || channel >= RELAY_CHANNEL_COUNT) 
    {    
        USBSerial.println("Invalid channel number. Use a number between 0 and 5.");
        return;
    }

    bool status = relayCtrl.getChannel(channel);

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[128];
    snprintf(msg, sizeof(msg), "Get relay - %d; status %s", channel, status ? "ON" : "OFF");
    USBSerial.println(msg);
}
void cli_set_relay(EmbeddedCli *cli, char *args, void *context)
{
    if ((args == NULL) || (embeddedCliGetTokenCount(args) < 2))
    {
        USBSerial.println("Usage: set-relay [arg1] [arg2]");
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);
    const char *arg2 = embeddedCliGetToken(args, 2);

    const uint8_t channel = atoi(arg1);
    // Validate channel number
    if (channel < 0 || channel >= RELAY_CHANNEL_COUNT) 
    {    
        USBSerial.println("Invalid channel number. Use a number between 0 and 5.");
        return;
    }

    // Validate state
    if (arg2 == NULL) 
    {
        USBSerial.println("Invalid state. Use on/1 or off/0.");
        return;
    }
    // Convert arg2 to boolean state
    // Assuming arg2 is either "on", "off", "1", or "0"
    // If arg2 is not one of these, we will return an error message
    RelayControl& relayCtrl = RelayControl::getInstance();
    bool newState = false;
    if ((strcmp(arg2, "on") == 0) || (strcmp(arg2, "1") == 0))
    {
        newState = true;
    }
    else if ((strcmp(arg2, "off") == 0) || (strcmp(arg2, "0") == 0))
    {
        newState = false;
    }
    else if (strcmp(arg2, "ON") == 0)
    {
        relayCtrl.allOn();
        USBSerial.println("All relays turned ON.");
        return;
    }
    else if (strcmp(arg2, "OFF") == 0)
    {
        relayCtrl.allOff();
        USBSerial.println("All relays turned OFF.");
        return;
    }
    else
    {
        USBSerial.println("Invalid state. Use on/1 or off/0, ON/OFF for all relays.");
        return;
    }   
    
    relayCtrl.setChannel(channel, newState);

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[128];
    snprintf(msg, sizeof(msg), "Set relay - %d; status %s", channel, newState ? "ON" : "OFF");
    USBSerial.println(msg);
}

void cli_toggle_relay(EmbeddedCli *cli, char *args, void *context)
{
    if ((args == NULL) || (embeddedCliGetTokenCount(args) < 1))
    {
        USBSerial.println("Usage: toggle-relay [arg1]");
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);
    const uint8_t channel = atoi(arg1);
    // Validate channel number
    if (channel < 0 || channel >= RELAY_CHANNEL_COUNT) 
    {    
        USBSerial.println("Invalid channel number. Use a number between 0 and 5.");
        return;
    }

    RelayControl& relayCtrl = RelayControl::getInstance();
    relayCtrl.toggleChannel(channel);
    bool status = relayCtrl.getChannel(channel);

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[128];
    snprintf(msg, sizeof(msg), "Toggle relay - %d; new status %s", channel, status ? "ON" : "OFF");
    USBSerial.println(msg);
}

void cli_ctrl_buzzer(EmbeddedCli *cli, char *args, void *context)
{
    const char *arg1 = embeddedCliGetToken(args, 1);
    if (arg1 == NULL) 
    {
        USBSerial.println("Usage: ctrl-buzzer [arg1]");
        return;
    }

    const uint8_t tone = atoi(arg1);
    // Validate tone
    if (tone < 0 || tone >= 8) 
    {    
        USBSerial.println("Invalid channel number. Use a number between 0 and 7.");
        return;
    }

    BuzzerControl& buzzer = BuzzerControl::getInstance();
    buzzer.playTone(tone); 

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[64];
    snprintf(msg, sizeof(msg), "Control buzzer with tone %d", tone);
    USBSerial.println(msg);
}