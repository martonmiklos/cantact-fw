#ifndef _LED_H
#define _LED_H

#include <stdbool.h>

#define LED_DURATION 50

void led_on(void);
void led_process(void);

void set_green_led(bool state);

#endif
