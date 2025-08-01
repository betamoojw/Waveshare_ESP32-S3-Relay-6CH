#include <Arduino.h>
#include "digital_led_control.h"


// Static method to get the singleton instance
DigitalLedControl& DigitalLedControl::getInstance() 
{
    static DigitalLedControl instance; // Guaranteed to be destroyed and instantiated on first use
    return instance;
}

// Private constructor
DigitalLedControl::DigitalLedControl() 
    : brightness(128), color(0) // Default brightness and color
{
    // Initialize the LEDs
    initLeds();
}

// Initialize the LEDs
void DigitalLedControl::initLeds() 
{
    FastLED.addLeds<WS2812B, GPIO_PIN_RGB, RGB>(leds, NUM_LEDS);
    FastLED.setBrightness(brightness); // Set default brightness
    turnOff(); // Ensure all LEDs are off initially
    show();    // Update the LED state
}

// Set the color of all LEDs using RGB values
void DigitalLedControl::setColor(uint8_t red, uint8_t green, uint8_t blue) 
{
    color = (red << 16) | (green << 8) | blue; // Pack RGB into uint32_t
    setColor(color);
}

// Set the color of all LEDs using a uint32_t value
void DigitalLedControl::setColor(uint32_t color) 
{
    this->color = color; // Store the color
    uint8_t red = (color >> 16) & 0xFF;
    uint8_t green = (color >> 8) & 0xFF;
    uint8_t blue = color & 0xFF;

    for (int i = 0; i < NUM_LEDS; i++) 
    {
        leds[i] = CRGB(red, green, blue);
    }
    show();
}

// Set the brightness of all LEDs
void DigitalLedControl::setBrightness(uint8_t brightness) 
{
    this->brightness = brightness;
    FastLED.setBrightness(brightness);
    show();
}

// Get the current color of the LEDs
uint32_t DigitalLedControl::getColor() const 
{
    return color;
}

// Get the current brightness of the LEDs
uint8_t DigitalLedControl::getBrightness() const 
{
    return brightness;
}

// Turn off all LEDs
void DigitalLedControl::turnOff() 
{
    setColor(0); // Set all LEDs to black
}

// Update the LEDs to reflect the current colors
void DigitalLedControl::show() 
{
    FastLED.show();
}

