/*
 * Original work:
 * Copyright 2023 Jeremy Grosser <jeremy@synack.me>
 *
 * Modifications:
 * Copyright 2026 Eduardo Araujo <cyberlalo@
 * Added timer-related Lua functions (sleep_ms, millis, micros).
 * Added ADC Lua functions (adc_init_pin, adc_read, adc_read_voltage, adc_read_temp).
 * Added GPIO Lua functions (pull_up, pull_down, disable_pulls, toggle).
 * Added validations, state machine tracker, Arduino aliases, and PWM support.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdio.h>

#include "hardware/watchdog.h"
#include "hardware/adc.h"
#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/bootrom.h"

#include <lua.h>
#include <lualib.h>
#include <lauxlib.h>

#define PROMPT "lua> "

// State machine to track the actual usage of each pin.
typedef enum {
    PIN_UNUSED,
    PIN_GPIO_IN,
    PIN_GPIO_OUT,
    PIN_ADC,
    PIN_PWM
} pin_mode_t;

static pin_mode_t pin_state[32] = {PIN_UNUSED};
static bool adc_initialized = false;

// Auxiliary function to validate GPIO limits (0 to 29 on the standard RP2040)
static int check_gpio(lua_State *L, int arg) {
    int pin = luaL_checkinteger(L, arg);
    if (pin < 0 || pin > 29) {
        luaL_error(L, "Error: Invalid GPIO pin. Must be between 0 and 29.");
    }
    return pin;
}

// reset()
static int l_reset(lua_State *L) {
    watchdog_reboot(0, 0, 0);
    return 0;
}

// bootsel()
static int l_bootsel(lua_State *L) {
    reset_usb_boot(0, 0);
    return 0;
}

// set_output(pin, bool) / pin_mode(pin, bool)
static int l_set_output(lua_State *L) {
    int pin = check_gpio(L, 1);
    int output = lua_toboolean(L, 2);
    lua_pop(L, 2);
    
    gpio_init(pin);
    gpio_set_dir(pin, output);
    pin_state[pin] = output ? PIN_GPIO_OUT : PIN_GPIO_IN;
    return 0;
}

// set_pin(pin, bool) / digital_write(pin, bool)
static int l_set_pin(lua_State *L) {
    int pin = check_gpio(L, 1);
    int state = lua_toboolean(L, 2);
    lua_pop(L, 2);
    
    // Force reconfiguration if the pin is not an active GPIO output.
    if (pin_state[pin] != PIN_GPIO_OUT) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        pin_state[pin] = PIN_GPIO_OUT;
    }
    
    gpio_put(pin, state);
    return 0;
}

// bool get_pin(pin) / digital_read(pin)
static int l_get_pin(lua_State *L) {
    int pin = check_gpio(L, 1);
    
    // It is automatically configured as an input only if it is not a valid GPIO.
    if (pin_state[pin] != PIN_GPIO_IN && pin_state[pin] != PIN_GPIO_OUT) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_IN);
        pin_state[pin] = PIN_GPIO_IN;
    }
    
    int state = gpio_get(pin);
    lua_pop(L, 1);
    lua_pushboolean(L, state);
    return 1;
}

// sleep_ms(ms)
static int l_sleep_ms(lua_State *L) {
    int ms = luaL_checkinteger(L, 1);
    lua_pop(L, 1);
    sleep_ms(ms);
    return 0;
}

// millis()
static int l_millis(lua_State *L) {
    uint64_t ms = to_ms_since_boot(get_absolute_time());
    lua_pushinteger(L, (lua_Integer)ms);
    return 1;
}

// micros()
static int l_micros(lua_State *L) {
    uint64_t us = to_us_since_boot(get_absolute_time());
    lua_pushinteger(L, (lua_Integer)us);
    return 1;
}

// adc_init_pin(pin) -- Mapeia de 26 a 29
static int l_adc_init_pin(lua_State *L) {
    int pin = check_gpio(L, 1);
    lua_pop(L, 1);
    
    if (pin < 26 || pin > 29) {
        return luaL_error(L, "Error: Valid ADC pins are 26 to 29 only.");
    }
    
    if (!adc_initialized) {
        adc_init();
        adc_initialized = true;
    }
    
    adc_gpio_init(pin);
    pin_state[pin] = PIN_ADC;
    return 0;
}

// adc_read(channel)
static int l_adc_read(lua_State *L) {
    int channel = luaL_checkinteger(L, 1);
    lua_pop(L, 1);
    
    if (channel < 0 || channel > 3) {
        return luaL_error(L, "Error: Valid ADC channels are 0 to 3.");
    }
    
    adc_select_input(channel);
    uint16_t result = adc_read();
    lua_pushinteger(L, result);
    return 1;
}

// adc_read_voltage(channel)
static int l_adc_read_voltage(lua_State *L) {
    int channel = luaL_checkinteger(L, 1);
    lua_pop(L, 1);
    
    if (channel < 0 || channel > 3) {
        return luaL_error(L, "Error: Valid ADC channels are 0 to 3.");
    }
    
    adc_select_input(channel);
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / (1 << 12);
    lua_pushnumber(L, voltage);
    return 1;
}

// adc_read_temp()
static int l_adc_read_temp(lua_State *L) {
    if (!adc_initialized) {
        adc_init();
        adc_initialized = true;
    }
    adc_set_temp_sensor_enabled(true);
    adc_select_input(4);
    uint16_t raw = adc_read();
    float voltage = raw * 3.3f / (1 << 12);
    float temp = 27.0f - (voltage - 0.706f) / 0.001721f;
    lua_pushnumber(L, temp);
    return 1;
}

// pull_up(pin)
static int l_pull_up(lua_State *L) {
    int pin = check_gpio(L, 1);
    lua_pop(L, 1);
    gpio_pull_up(pin);
    return 0;
}

// pull_down(pin)
static int l_pull_down(lua_State *L) {
    int pin = check_gpio(L, 1);
    lua_pop(L, 1);
    gpio_pull_down(pin);
    return 0;
}

// disable_pulls(pin)
static int l_disable_pulls(lua_State *L) {
    int pin = check_gpio(L, 1);
    lua_pop(L, 1);
    gpio_disable_pulls(pin);
    return 0;
}

// toggle(pin)
static int l_toggle(lua_State *L) {
    int pin = check_gpio(L, 1);
    lua_pop(L, 1);
    
    // If it is not an active output, it forces the pin to be converted to a GPIO output.
    if (pin_state[pin] != PIN_GPIO_OUT) {
        gpio_init(pin);
        gpio_set_dir(pin, GPIO_OUT);
        pin_state[pin] = PIN_GPIO_OUT;
    }
    
    gpio_put(pin, !gpio_get(pin));
    return 0;
}

// pwm_init(pin, freq)
static int l_pwm_init(lua_State *L) {
    int pin = check_gpio(L, 1);
    int freq = luaL_checkinteger(L, 2);
    lua_pop(L, 2);
    
    if (freq <= 0) {
        return luaL_error(L, "Error: PWM frequency must be > 0.");
    }
    
    gpio_set_function(pin, GPIO_FUNC_PWM);
    uint slice_num = pwm_gpio_to_slice_num(pin);
    
    uint32_t clock_hz = 125000000;
    uint32_t wrap = 12500;
    float clkdiv = (float)clock_hz / (freq * wrap);
    
    if (clkdiv < 1.0f) clkdiv = 1.0f;
    if (clkdiv > 255.0f) clkdiv = 255.0f;
    
    pwm_set_clkdiv(slice_num, clkdiv);
    pwm_set_wrap(slice_num, wrap);
    pwm_set_enabled(slice_num, true);
    
    pin_state[pin] = PIN_PWM;
    return 0;
}

// pwm_set_duty(pin, duty_percent)
static int l_pwm_set_duty(lua_State *L) {
    int pin = check_gpio(L, 1);
    int duty = luaL_checkinteger(L, 2);
    lua_pop(L, 2);
    
    if (pin_state[pin] != PIN_PWM) {
        return luaL_error(L, "Error: Pin is not configured for PWM. Call pwm_init first.");
    }
    
    if (duty < 0) duty = 0;
    if (duty > 100) duty = 100;
    
    uint slice_num = pwm_gpio_to_slice_num(pin);
    uint channel = pwm_gpio_to_channel(pin);
    
    uint32_t level = (duty * 12500) / 100;
    pwm_set_chan_level(slice_num, channel, level);
    
    return 0;
}

int main() {
    lua_State *L;
    luaL_Buffer buf;
    int status;
    size_t len;
    char ch;

    stdio_init_all();

    L = luaL_newstate();
    luaL_openlibs(L);
    luaL_buffinit(L, &buf);

    // Records
    lua_register(L, "reset",            l_reset);
    lua_register(L, "bootsel",          l_bootsel);
    lua_register(L, "set_output",       l_set_output);
    lua_register(L, "set_pin",          l_set_pin);
    lua_register(L, "get_pin",          l_get_pin);
    lua_register(L, "sleep_ms",         l_sleep_ms);
    lua_register(L, "millis",           l_millis);
    lua_register(L, "micros",           l_micros);
    lua_register(L, "adc_init_pin",     l_adc_init_pin);
    lua_register(L, "adc_read",         l_adc_read);
    lua_register(L, "adc_read_voltage", l_adc_read_voltage);
    lua_register(L, "adc_read_temp",    l_adc_read_temp);
    lua_register(L, "pull_up",          l_pull_up);
    lua_register(L, "pull_down",        l_pull_down);
    lua_register(L, "disable_pulls",    l_disable_pulls);
    lua_register(L, "toggle",           l_toggle);
    lua_register(L, "pwm_init",         l_pwm_init);
    lua_register(L, "pwm_set_duty",     l_pwm_set_duty);

    // Aliases
    lua_register(L, "pin_mode",         l_set_output);
    lua_register(L, "digital_write",    l_set_pin);
    lua_register(L, "digital_read",     l_get_pin);

    printf("\n*** picolua \n Ctrl-C  Clear buffer\n Ctrl-D  Execute buffer\n Ctrl-L  Clear screen\n\n" PROMPT);

    while(1) {
        ch = (char)getchar();
        if(ch == '\r') {
            ch = '\n';
        }

        if(ch == 0x7F || ch == 0x08) {
            if(luaL_bufflen(&buf) > 0) {
                luaL_buffsub(&buf, 1);
                printf("\b \b");
            }
        }else if(ch == 0x0C) {
            printf("\x1b[2J\x1b[1;1H" PROMPT);
        }else if(ch == 0x03) {
            luaL_buffinit(L, &buf);
            printf("\n" PROMPT);
        }else if(ch == 0x04) {
            luaL_pushresult(&buf);
            const char *s = lua_tolstring(L, -1, &len);
            status = luaL_loadbuffer(L, s, len, "picolua");
            if(status != LUA_OK) {
                const char *msg = lua_tostring(L, -1);
                fprintf(stderr, "parse error: %s\n", msg);
            }else{
                status = lua_pcall(L, 0, 0, 0);
                if(status != LUA_OK) {
                    const char *msg = lua_tostring(L, -1);
                    fprintf(stderr, "execute error: %s\n", msg);
                }
            }

            lua_pop(L, 1);
            luaL_buffinit(L, &buf);
            printf(PROMPT);
        }else if((ch >= 0x20 && ch < 0x7F) || ch == '\t' || ch == '\n') {
            putchar(ch);
            luaL_addchar(&buf, ch);
        }
    }

    lua_close(L);
    return 0;
}