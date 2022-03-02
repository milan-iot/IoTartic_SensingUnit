#ifndef _LED_H
#define _LED_H

#include <NeoPixelBus.h>

/// Available LED colors
typedef enum {RED, GREEN, BLUE, YELLOW, PURPLE, CYAN, WHITE, BLACK} LED_COLOR;

/**
 * Function that sets up LED
 * @return No return value
 */
void LED_init(void);

/**
 * Function that sets color saturation for LED
 * @param saturation - Color saturation
 * @return No return value
 */
void LED_setSaturation(uint8_t saturation);

/**
 * Function that sets color of LED
 * @param color - Color of the LED
 * @return No return value
 */
void LED_setColor(LED_COLOR color);

#endif
