# Indicator Adapters

`StatusIndicator` drives the board's WS2812 LED and passive buzzer from semantic
application conditions. Call `update(nowMs)` regularly from the cooperative
scheduler; it never delays or blocks.

Pattern priority is critical fault, commissioning, degraded bus, command
feedback, then normal operation. Protocol adapters must request semantic state
changes and must not call RGB or PWM APIs directly.