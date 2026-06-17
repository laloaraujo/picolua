# PicoLua-RP2040

An extended version of PicoLua for the RP2040 microcontroller, featuring enhanced peripheral support and Arduino-inspired APIs.

## Overview

PicoLua-RP2040 is a lightweight Lua interpreter for the Raspberry Pi Pico and other RP2040-based boards. This project extends the original PicoLua with additional functionality aimed at embedded applications, rapid prototyping, and educational use.

## Features

### Advanced GPIO

- Digital input and output configuration

- Internal pull-up support

- Internal pull-down support

- Fast GPIO state toggling (`toggle`)

- Arduino-style aliases and API compatibility

### PWM

- Full PWM support

- Frequency configuration

- Duty cycle control

- Multiple independent PWM channels

### Analog Input

- ADC channel reading

- Voltage measurement

- Internal RP2040 temperature sensor access

### Timing

- Millisecond counter (`millis`)

- Microsecond counter (`micros`)

- Precise delay functions

### Arduino Compatibility

Familiar function names and aliases to simplify migration from Arduino-based projects.

## Dependencies

```bash
sudo apt update
sudo apt install build-essential gcc-arm-none-eabi git cmake python3 tio

git clone https://github.com/laloaraujo/PicoLua-RP2040.git
cd PicoLua-RP2040
```

## Build

```bash
mkdir build
cd build

cmake ..
make
```

## Run

Hold the **BOOTSEL** button while connecting the board via USB.

```bash
sudo cp picolua.uf2 /dev/disk/by-label/RPI-RP2
tio -b 115200 /dev/ttyACM0
```

## REPL Usage

```text
Ctrl-C  Clear buffer
Ctrl-D  Execute buffer
Ctrl-L  Clear screen
```

## Example

```lua
set_output(25, true)
for i = 1, 10 do
  set_pin(25, true)
  sleep_ms(500)
  set_pin(25, false)
  sleep_ms(500)
end
```

## PWM Example

```lua
pwm_init(25, 1000)
pwm_set_duty(25, 80)
sleep_ms(2000)
pwm_set_duty(25, 20)
sleep_ms(2000)
pwm_set_duty(25, 0)
```

## Temperature Sensor Example

```lua
print("Temperature:", adc_read_temp())
```

## Notes

The Lua 5.5.0 distribution included in this repository was copied from the official release source tarball with the following modifications:

- Added `src/CMakeLists.txt` listing all Lua source files except `lua.c` and `luac.c`

- Changed `LUA_32BITS` to `1` in `luaconf.h`

## Supported Hardware

- Raspberry Pi Pico

- Other RP2040-based boards

## Project Goals

- Make embedded programming more accessible

- Provide a lightweight alternative to MicroPython

- Enable low-level hardware access through Lua

- Serve as an educational platform for embedded systems development

## Credits

Based on the original PicoLua project by Jeremy Grosser.

RP2040 extensions and enhancements by Eduardo Araujo.

## License

This project follows the license terms of the original PicoLua project unless otherwise stated.
