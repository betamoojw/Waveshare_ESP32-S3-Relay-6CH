# Hardware Abstraction Layer

The HAL is the firmware's hardware-facing contract. It is standard C++17,
allocation-free, and uses fixed function tables rather than virtual dispatch.

```text
Application and services
          |
          v
  src/hal contracts
          |
          v
ESP32 BSP implementations and board descriptors
          |
          v
Selected ESP32 relay product
```

## Ownership

| File | Responsibility |
|---|---|
| `Board.h` | Select the active board for the build. |
| `BoardDescriptor.h` | Describe model, capabilities, polarity, and physical assignments. |
| `RelayHal.h` | Apply typed relay states. Missing handlers fail closed. |
| `ButtonHal.h` | Initialize and read the physical button. Gesture policy remains outside HAL. |
| `RgbLedHal.h` | Write RGB output values. |
| `BuzzerHal.h` | Initialize and write bounded tone/duty output. |
| `Rs485Hal.h` | Provide byte transport callbacks to protocol adapters. |
| `NetworkHal.h` | Aggregate network status and control handles. |

Concrete ESP32 implementations live under `src/adapters/bsp/`. Arduino, GPIO,
LEDC, UART, Wi-Fi, and board-vendor APIs MUST NOT appear in domain or
application services. `Application` is the composition root: it owns concrete
drivers but passes only HAL values into services and protocol wiring.

## Adding A Board

1. Add a board descriptor package under `boards/<product>/`. `hal::isValid()`
   rejects missing metadata, invalid pins, duplicate relay pins, and collisions
   among relays, the button, indicators, and RS-485.
2. Add or reuse concrete ESP32 HAL drivers for relay, button, RGB, buzzer,
   RS-485, and network capabilities.
3. Select the descriptor with `SWITCH_ACTUATOR_BOARD`. Product selection MUST
   remain confined to `Board.cpp`:

| Value | Package |
|---:|---|
| `0` | `boards/waveshare_s3_relay_6ch` |
| `1` | `boards/custom_relay_6ch` |
| `2` | `boards/custom_relay_12ch` |

Custom packages intentionally have no assumed GPIO map. Their headers require
`CUSTOM_RELAY_PIN_1` through the product relay count plus
`CUSTOM_BUTTON_PIN`, `CUSTOM_RGB_PIN`, `CUSTOM_BUZZER_PIN`,
`CUSTOM_RS485_TX_PIN`, and `CUSTOM_RS485_RX_PIN` as build definitions.

4. Add a PlatformIO environment and board-level tests. Verify every relay starts
   inactive before output mode and follows `design/relay-safety-policy.md`.
5. Keep protocol/application code unchanged. A new product requiring changes to
   relay policy or protocol code has crossed the HAL boundary incorrectly.

Descriptors support up to 12 relays, but the current domain, configuration,
protocol maps, and web contract compile for six. Startup therefore rejects a
selected descriptor whose `relayCount` differs from `domain::relayChannelCount`.
The 12-channel package is a validated hardware description, not an enabled
12-channel firmware target; that target requires widening those shared
contracts first.

`src/adapters/bsp/board_descriptor.h` and `ports/relay_output_port.h` are
compatibility aliases for existing integrations. New code uses the HAL headers
directly.
