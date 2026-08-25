# Hardware Abstraction Layer

The HAL contains the firmware's hardware-facing outbound ports. It is standard
C++17, allocation-free, and uses copyable function tables rather than virtual
dispatch. `Interfaces.h` is the aggregate include for all hardware contracts.

```text
Application and services
     |
     v
 HAL and persistence ports
     |
     +----+----+
     |         |
 ESP32-S3   Future MCU
 adapters    adapters
```

## Ownership

| Interface | Current header | Responsibility |
|---|---|---|
| `IRelay` | `RelayHal.h` | Apply typed relay states. Missing handlers fail closed. |
| `IButton` | `ButtonHal.h` | Initialize and read a physical button. Gesture policy remains outside HAL. |
| `IIndicator` | `RgbLedHal.h` | Write physical RGB indicator output. Status policy remains in the indicator adapter. |
| `IBuzzer` | `BuzzerHal.h` | Initialize and write bounded tone/duty output. |
| `IUart` | `Rs485Hal.h` | Provide bounded byte transport for RS-485 and other UART-backed adapters. |
| `IStorage` | `../ports/settings_store.h` | Load, save, and erase transactional device configuration. |
| `IWatchdog` | `WatchdogHal.h` | Initialize, feed, and query watchdog health. |
| `INetwork` | `NetworkHal.h` | Aggregate network status and control ports. |
| `IClock` | `../ports/clock_port.h` | Supply monotonic milliseconds without exposing an MCU timer API. |

`Board.h` selects the active board at build time. `BoardDescriptor.h` describes
model, capabilities, polarity, and physical assignments. `IStorage` does not
own LittleFS deployment files; those use `ConfigurationFilePort` because
transactional settings and deployment bundles have different lifecycles.

Legacy names such as `RelayHal`, `ButtonHal`, `RgbLedHal`, `BuzzerHal`,
`Rs485Hal`, `NetworkHal`, `ClockPort`, and `SettingsStore` remain type aliases
for source compatibility. New production code uses the canonical `I*` names.

Concrete ESP32 implementations live under `src/adapters/`. Arduino, FreeRTOS,
GPIO, LEDC, UART, Wi-Fi, NVS, and board-vendor APIs MUST NOT appear in domain or
application services. `Application` is the composition root: it owns concrete
drivers but passes only interface values into services and protocol wiring.

To target another MCU, implement the same interfaces and replace construction
in `Application`; relay policy, scenes, timers, KNX, Modbus, and Web behavior do
not change. Product variants may share adapters independently: a wall panel can
provide `IButton`, `IIndicator`, and `INetwork` without relays, while DIN-rail
actuators can provide a larger `IRelay` implementation.

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
