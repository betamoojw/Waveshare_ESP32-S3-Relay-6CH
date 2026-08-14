#include "app/application.h"

#include <Arduino.h>

namespace
{
switch_actuator::app::Application &application() noexcept
{
    static switch_actuator::app::Application instance{};
    return instance;
}
}

void setup()
{
    static_cast<void>(application().initialize(millis()));
}

void loop()
{
    application().update(millis());
}
