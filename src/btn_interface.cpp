#include "btn_interface.h"

// Static method to get the singleton instance
BtnInterface& BtnInterface::getInstance() 
{
    static BtnInterface instance; // Guaranteed to be destroyed and instantiated on first use
    return instance;
}

// Private constructor
BtnInterface::BtnInterface() 
    : button(Button2()) // Initialize Button2 object
{
    USBSerial.println("BtnInterface: Constructor called.");
}

// Initialize the button interface
void BtnInterface::initialize(int pin) 
{
    USBSerial.println("BtnInterface: Initializing...");

    // Attach the button to the specified pin
    button.begin(pin);
    // button.setLongClickTime(1000);
    // button.setDoubleClickTime(400);

    USBSerial.println(" Longpress Time:\t" + String(button.getLongClickTime()) + "ms");
    USBSerial.println(" DoubleClick Time:\t" + String(button.getDoubleClickTime()) + "ms");
    USBSerial.println();

    // Attach callbacks for button events
    button.setPressedHandler(onPress);
    button.setReleasedHandler(onRelease);
    // button.setChangedHandler(onChanged);
    button.setClickHandler(onClick);
    button.setLongClickDetectedHandler(onLongClickDetected);
    button.setLongClickHandler(onLongClick);
    button.setLongClickDetectedRetriggerable(false);
    
    button.setDoubleClickHandler(onDoubleClick);
    button.setTripleClickHandler(onTripleClick);
    // button.setTapHandler(onTap);
}

// Update the button state (should be called in the loop)
void BtnInterface::update() 
{
    button.loop(); // Update the Button2 object
}

// Callback for button press event
void BtnInterface::onPress(Button2& btn) 
{
    USBSerial.println("BtnInterface: Button pressed.");
}

// Callback for button release event
void BtnInterface::onRelease(Button2& btn) 
{
    USBSerial.println("BtnInterface: Button released.");
    USBSerial.println(btn.wasPressedFor());
}

// Callback for button state change
void BtnInterface::onChanged(Button2& btn) 
{
    USBSerial.println("BtnInterface: Button state changed.");
}

// Callback for single click event
void BtnInterface::onClick(Button2& btn) 
{
    USBSerial.println("BtnInterface: Button clicked.");
}

// Callback for long click detected event
void BtnInterface::onLongClickDetected(Button2& btn) 
{
    USBSerial.println("BtnInterface: Long click detected.");
}

// Callback for long click event
void BtnInterface::onLongClick(Button2& btn) 
{
    USBSerial.println("BtnInterface: Long click performed.");
}

// Callback for double click event
void BtnInterface::onDoubleClick(Button2& btn) 
{
    USBSerial.println("BtnInterface: Button double-clicked.");
}

// Callback for triple click event
void BtnInterface::onTripleClick(Button2& btn) 
{
    USBSerial.println("BtnInterface: Button triple-clicked.");
    USBSerial.println(btn.getNumberOfClicks());
}

// Callback for tap event
void BtnInterface::onTap(Button2& btn) 
{
    USBSerial.println("BtnInterface: Button tapped.");
}