#include <Arduino.h>
#include "buzzer_control.h"

// Static method to get the singleton instance
BuzzerControl& BuzzerControl::getInstance() 
{
    static BuzzerControl instance; // Guaranteed to be destroyed and instantiated on first use
    return instance;
}

// Private constructor
BuzzerControl::BuzzerControl() 
{
    initBuzzer(); // Initialize the buzzer during construction
}

// Initialize the buzzer (e.g., set up the GPIO pin)
// ledcSetup and ledcAttachPin are removed APIs.
// New APIs: ledcAttach used to set up the LEDC pin (merged ledcSetup and ledcAttachPin functions).
void BuzzerControl::initBuzzer()
{
    // pinMode(GPIO_PIN_Buzzer, OUTPUT); // Initialize the control GPIO of Buzzer

    // ledcAttach(GPIO_PIN_Buzzer, Frequency, Resolution); // Set a LEDC channel
    ledcAttachChannel(GPIO_PIN_Buzzer, Frequency, Resolution, PWM_Channel);   // Connect the channel to the corresponding pin
}

// Play a tone
void BuzzerControl::playTone(uint8_t tone)
{
    // Play a tone
    int noteDuration = 1000 / noteDurations[tone];
    ledcWriteTone(GPIO_PIN_Buzzer, melody[tone]);
    delay(noteDuration);
    ledcWriteTone(GPIO_PIN_Buzzer, 0); // Stop tone
    delay(50);                       // short pause between notes
}