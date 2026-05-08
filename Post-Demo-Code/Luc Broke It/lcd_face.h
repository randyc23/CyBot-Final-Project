/*
 * lcd_face.h
 *
 * Public interface for the animated LCD happy face.
 *
 * Usage:
 *   1. Call lcd_init() as normal.
 *   2. Call face_init() once to load characters and draw the static smile.
 *   3a. Call face_run() to loop forever (blocking), OR
 *   3b. Call face_tick() from your own loop every FACE_TICK_MS milliseconds
 *       if you need to do other work between frames.
 *
 * Example (blocking):
 *   int main(void) {
 *       lcd_init();
 *       face_init();
 *       face_run();   // never returns
 *   }
 *
 * Example (non-blocking, manual tick):
 *   int main(void) {
 *       lcd_init();
 *       face_init();
 *       while (1) {
 *           face_tick();
 *           Timer_delay_ms(FACE_TICK_MS);
 *           do_other_stuff();
 *       }
 *   }
 */

#ifndef LCD_FACE_H_
#define LCD_FACE_H_

#include <stdint.h>

/*
 * FACE_TICK_MS
 * Duration of one animation frame in milliseconds.
 * One full animation cycle is 13 frames (~5.2 seconds).
 * Adjust to taste — lower values make the face more lively.
 */
#define FACE_TICK_MS   400u

/*
 * face_init()
 * Clears the display, loads all 8 custom CGRAM characters, draws the
 * static smile on row 3, and renders the first animation frame.
 * Must be called once after lcd_init() before any other face function.
 */
void face_init(void);

/*
 * face_tick()
 * Advances the animation by one frame and redraws only the eyes.
 * The smile is static and does not need to be redrawn each tick.
 * Call this every FACE_TICK_MS milliseconds from your own scheduler
 * or loop if you need control between frames.
 */
void face_tick(void);

/*
 * face_run()
 * Convenience blocking loop: calls face_tick() then waits FACE_TICK_MS ms,
 * repeating forever. Does not return. Uses Timer_delay_ms() internally.
 */
void face_run(void);

#endif /* LCD_FACE_H_ */
