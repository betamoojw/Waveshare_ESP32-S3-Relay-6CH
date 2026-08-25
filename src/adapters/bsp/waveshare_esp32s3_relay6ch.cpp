#include "waveshare_esp32s3_relay6ch.h"

#include <cstddef>

namespace switch_actuator::adapters::bsp
{
static_assert(hal::isValid(waveshareEsp32S3Relay6Ch), "Invalid Waveshare board descriptor");
}