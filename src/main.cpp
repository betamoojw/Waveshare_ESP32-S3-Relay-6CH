#include <Arduino.h>
#include "HWCDC.h"
#include "cli_interface.h"

HWCDC USBSerial; // Definition of the USBSerial object

void setup()
{
    String title = "Relay Controller";
    USBSerial.begin(115200);

    USBSerial.println(title + " start");

    cli_init();

    USBSerial.println(title + " end");
}

void loop()
{
    cli_task();
}