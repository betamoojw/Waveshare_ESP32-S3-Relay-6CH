#include "cli_commands.h"
#include "HWCDC.h"

extern HWCDC USBSerial; // Declaration of the external USBSerial object

#define CLI_INTERFACE_VERSION "0.0.1"

void cli_command(EmbeddedCli *embeddedCli, CliCommand *command)
{
    USBSerial.println(F("Received command:"));
    USBSerial.println(command->name);
    embeddedCliTokenizeArgs(command->args);
    for (int i = 1; i <= embeddedCliGetTokenCount(command->args); ++i) {
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

void cli_get_led(EmbeddedCli *cli, char *args, void *context)
{
    const char *arg1 = embeddedCliGetToken(args, 1);
    if (arg1 == NULL) {
        USBSerial.println("Usage: get-led [arg1]");
        return;
    }

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[64];
    snprintf(msg, sizeof(msg), "Get LED with args: %s, status: %d", arg1, random(0, 2));
    USBSerial.println(msg);
}
void cli_set_led(EmbeddedCli *cli, char *args, void *context)
{
    if ((args == NULL) || (embeddedCliGetTokenCount(args) < 2))
    {
        USBSerial.println("Usage: set-led [arg1] [arg2]");
        return;
    }

    const char *arg1 = embeddedCliGetToken(args, 1);
    const char *arg2 = embeddedCliGetToken(args, 2);

    // Make sure to check if 'args' != NULL, printf's '%s' formatting does not like a null pointer.
    char msg[64];
    snprintf(msg, sizeof(msg), "Set LED with args: %s and %s", arg1, arg2);
    USBSerial.println(msg);
}
