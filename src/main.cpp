#include "HWCDC.h"
#include <Arduino.h>

HWCDC USBSerial; // Definition of the USBSerial object

void setup()
{
    USBSerial.begin(115200);
    USBSerial.println("USBSerial initialized.");

    String title = "Smart Panel";
    USBSerial.println(title + " start");
}

void loop()
{
    USBSerial.println("Hello World");
    delay(2000);
}