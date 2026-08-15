# AGENTS.md

Project-specific guidance for AI Coding Agent & AI Code Review Reference in this repository.

## Guiding Principle

**Readability comes first.** Favor the clearest implementation that satisfies the mandatory rules as below:
- Follow the modern softwacre/firmware enginerring design principle and code in idiomatic, producntion-class modern C++17
- Performance-aware first code implementation and transformation
- Safe refactor with preserving exact runtime behavior if review/improvement/refinement is required
- Eliminate undefined behavior, and improve safety, readability, and maintainability if review/improvement/refinement is required.

### MODERNIZATION RULES (Strictly C++17)
- Replace raw `malloc`/`free`, `new`/`delete` with RAII and smart pointers (`std::unique_ptr`/`std::shared_ptr` with custom deleters when needed)
- Convert C-style arrays/buffers to `std::vector`, `std::array`, or `std::string_view` (for read-only views; never assume null-termination)
- Replace C-style casts with `static_cast`, `const_cast`, or `reinterpret_cast` (only when unavoidable; justify usage)
- Use `auto`, structured bindings, range-based `for`, and `constexpr`/`if constexpr` where applicable
- Prefer `std::optional` for validity/nullable returns, `std::variant` for type-safe unions, `std::any` sparingly
- Replace manual loops with `<algorithm>` functions (`std::transform`, `std::for_each`, `std::accumulate`, `std::find_if`, etc.)
- Apply modern attributes: `[[nodiscard]]`, `[[maybe_unused]]`, `[[fallthrough]]`
- Use `std::exchange`, `std::invoke`, `std::scoped_lock` (C++17), `std::lock_guard`, `std::unique_lock`
- Convert C error codes to exceptions or `std::optional`/result patterns (C++17 compatible)
- Replace `<stdio.h>`/`<stdlib.h>` with `<cstdio>`/`<cstdlib>` or C++ equivalents (`std::cout`, `std::stoi`, etc.) where appropriate
- Maintain thread-safety semantics; do not introduce data races or remove existing synchronization

### CONSTRAINTS & GUARDRAILS
- Strictly C++17. Do not use C++20/23 features (no `std::ranges`, `std::expected`, `std::format`, concepts, etc.) unless explicitly requested
- Preserve public API contracts unless a breaking change is unavoidable (document rationale & migration path)
- Do not introduce unnecessary abstractions, virtual dispatch, or runtime overhead
- Keep performance equal or better; note assumptions if benchmarks would be needed
- Eliminate Undefined Behaviors (array overruns, signed overflow, dangling pointers, missing null checks, etc.)
- Never introduce undefined behavior or hide resource leaks.
- Prefer standard library solutions over custom implementations.
- Use `[[nodiscard]]`, `noexcept`, `const`, and value semantics appropriately.
- Avoid premature abstraction; keep interfaces minimal and testable.
- Explicitly state compiler requirements (e.g., C++17, GCC 8+/Clang 7+/MSVC 2017+).
- If a requested change violates SOLID or introduces regression risk, warn clearly and propose compliant alternatives.

### Goals: 

Readability, maintainability, lower coupling, duplication removal, long-function extraction, SRP（Single Responsibility Principle）, modern C++17 style, naming, const correctness, RAII, reduced raw `new`/`delete`, unused parameter cleanup, and return-type review, create new type accessor if neccessory, boundary check and safe.


## Switch Actuator

Switch Actuator is C++ firmware for ESP32/ESP8266 microcontrollers controlling relays via KNX IP/Modbus RTU protocol, with a web UI (HTML/JS/CSS). Built with PlatformIO (Arduino framework) and Node.js tooling.
Note: 
  1. modbus rtu could run in client and server by dynamical switching via cli, webui
  2. use json file as data-driven



## Hardware Platform Info

### Waveshare ESP32-S3-Relay-6CH board

The GPIO Assignment as below should be treated as the board-level reference for firmware/BSP implementation.

| GPIO | Function | Direction | Description |
|---:|---|---|---|
| GPIO0 | BOOT | Input | BOOT button control pin |
| GPIO1 | CH1 | Output | Relay 1 control pin |
| GPIO2 | CH2 | Output | Relay 2 control pin |
| GPIO17 | TXD | Output | RS485 TX / UART TX |
| GPIO18 | RXD | Input | RS485 RX / UART RX |
| GPIO21 | BUZZER | Output | Passive buzzer control |
| GPIO38 | RGB | Output | WS2812 RGB LED control |
| GPIO41 | CH3 | Output | Relay 3 control pin |
| GPIO42 | CH4 | Output | Relay 4 control pin |
| GPIO45 | CH5 | Output | Relay 5 control pin |
| GPIO46 | CH6 | Output | Relay 6 control pin |

**Important** GPIO0 has a special function on the ESP32-S3 boot process. Firmware should therefore avoid treating GPIO0 as a completely ordinary application GPIO.
Recommended handling:

```text
GPIO0
 |
 +-- ESP32-S3 boot/download mode
 |
 +-- Application BOOT button detection
```

Long-press or factory-reset functionality can be implemented at the application layer, but it must not interfere with normal ESP32-S3 firmware download behavior.

### Other boards

## Software/Firmware Architecture 

Check: `design/software-architecture-instructions.md`

## Front-end Web UI 

Check: `design/fontend-instructions`

## KNX Switch Actuator

Check: `design/knx_ip_switching_actuator-instructions.md`


## Network Connectivity

Check: `design/network-architecture.md`
1. for Wi-Fi provisioning leverage the (https://github.com/tostmann/improv-wifi-busware.git) lib.
2. for ethernet manaager leverage the networking-for-arduino/EthernetESP32@^1.0.2 lib. However this is out of scope in the current implementation.
