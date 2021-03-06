#include <stdint.h>
#include "adc.h"
#include "gpio.h"
#include "config.h"
#include "util.h"
#include "channels.h"
#include "cypress.h"

static const uint8_t stick_map[4] = { STICK_THROTTLE, STICK_ROLL, STICK_PITCH, STICK_YAW };
extern uint8_t telem_ack_value;
static uint8_t last_telem_ack_value;
static uint8_t telem_ack_send_count;
static uint8_t telem_extra_type;

extern uint8_t get_bl_version(void);

/*
  latch left button with debouncing
 */
static bool latched_left_button(void)
{
    static uint8_t counter;
    static bool latched;
    static bool ignore_left_button;

    /*
      the left button is latching to make flight mode changes more
      reliable on lossy links. If any other button is pressed when
      left button is held, then it is a button combination and mode
      does not change
     */
    
    if (gpio_get(PIN_LEFT_BUTTON) != 0) {
        if (counter >= 10 && !ignore_left_button) {
            latched = !latched;
        }
        counter = 0;
        ignore_left_button = false;
    } else {
        if (counter < 11) {
            counter++;
        }
        if (gpio_get(PIN_SW1)==0 ||
            gpio_get(PIN_SW2)==0 ||
            gpio_get(PIN_USER)!=0 ||
            gpio_get(PIN_RIGHT_BUTTON)==0) {
            ignore_left_button = true;
        }
        if (ignore_left_button) {
            counter = 0;
        }
    }
    return latched;
}

/*
  return an 11 bit channel output value
 */
uint16_t channel_value(uint8_t chan)
{
    int16_t v = 0;
    
    switch (chan) {
    case 0:
    case 1:
    case 2:
    case 3: {
        uint8_t stick = stick_map[chan];
        v = adc_value(stick);
        if (v > 1000) {
            v = 1000;
        }
        if (stick != STICK_THROTTLE) {
            // fix reversals
            v = 1000 - v;
        }
        break;
    }
    case 4:
        v = latched_left_button()?1000:0;
        if (!gpio_get(PIN_LEFT_BUTTON)) {
            // this allows for long-press vs short-press actions
            v += 100;
        }
        break;
    case 5:
        v = gpio_get(PIN_RIGHT_BUTTON)==0?1000:0;
        break;
    case 6:
        // encode 3 switches onto channel 7
        v = 0;
        if (gpio_get(PIN_SW1)==0) {
            v |= 1;
        }
        if (gpio_get(PIN_SW2)==0) {
            v |= 2;
        }
        if (gpio_get(PIN_USER)!=0) {
            v |= 4;
        }
        v *= 100;
        break;
    case 7: {
        uint8_t tvalue=0;
        /* return extra data in channel 8. Use top 3 bits for data type */
        telem_extra_type = (telem_extra_type+1) % 7;

        if (telem_extra_type == 0 ||
            telem_ack_value != last_telem_ack_value ||
            telem_ack_send_count < 20) {
            if (last_telem_ack_value != telem_ack_value) {
                telem_ack_send_count = 0;
            }
            last_telem_ack_value = telem_ack_value;
            telem_ack_send_count++;
            // key 0 is telem ack
            return telem_ack_value;
        }
        switch (telem_extra_type) {
        case 1:
            // key 1 is year
            tvalue = BUILD_DATE_YEAR-2017;
            break;
        case 2:
            // key 2 is month
            tvalue = BUILD_DATE_MONTH;
            break;
        case 3:
            // key 3 is day
            tvalue = BUILD_DATE_DAY;
            break;
        case 4:
            // key 4 telem RSSI
            tvalue = get_telem_rssi();
            break;
        case 5:
            // key 5 telem pps
            tvalue = get_telem_pps();
            break;
        case 6:
            // bl version
            tvalue = get_bl_version();
            break;
        }

        return (((uint16_t)telem_extra_type)<<8) | tvalue;
    }
    }

    // map into 11 bit range
    v = (((v - 500) * 27 / 32) + 512) * 2;

    return (uint16_t)v;
}

// get buttons
uint8_t get_buttons(void)
{
    uint8_t ret = 0;

    if (gpio_get(PIN_LEFT_BUTTON) == 0) {
        ret |= BUTTON_LEFT;
    }
    if (gpio_get(PIN_RIGHT_BUTTON) == 0) {
        ret |= BUTTON_RIGHT;
    }
    if (gpio_get(PIN_SW1)==0) {
        ret |= BUTTON_LEFT_SHOULDER;
    }
    if (gpio_get(PIN_SW2)==0) {
        ret |= BUTTON_RIGHT_SHOULDER;
    }
    if (gpio_get(PIN_USER)!=0) {
        ret |= BUTTON_POWER;
    }
    return ret;
}
