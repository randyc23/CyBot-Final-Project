/*
 * lcd_face.c
 *
 *  Created on: May 4, 2026
 *      Author: Bot Maxxers
 *
 * lcd_face.c
 *
 * Animated happy face for a 20x4 HD44780 LCD.
 *
 * Face layout (0-indexed rows and columns):
 *   Row 0: blank (forehead)
 *   Row 1: left eye  cols 2-3  |  right eye cols 16-17
 *   Row 2: blank
 *   Row 3: smile arc           cols 6-13
 *
 * Custom CGRAM characters (8 slots):
 *   0 - Eye left  outer half  (open)
 *   1 - Eye left  inner half  (open)
 *   2 - Eye right inner half  (open)
 *   3 - Eye right outer half  (open)
 *   4 - Eye left/right half   (squint) — reused for both sides
 *   5 - Eye wink half         — reused for both halves
 *   6 - Smile corner          — used for both left and right corners
 *   7 - Smile centre segment
 *
 * Because the squint and wink are symmetric we only need one bitmap
 * each and mirror them by column position.
 *
 * NOTE: The HD44780 has exactly 8 CGRAM slots (indices 0-7).
 *       We use all 8. Do not add more without removing one.
 */

#include "lcd_face.h"
#include "lcd.h"
#include "Timer.h"

/* ------------------------------------------------------------------ */
/*  Timing                                                              */
/* ------------------------------------------------------------------ */
#define TICK_MS     400u    /* duration of one animation frame (ms)   */
#define ANIM_LEN    13u     /* total number of frames in one cycle     */

/* ------------------------------------------------------------------ */
/*  CGRAM slot assignments                                              */
/* ------------------------------------------------------------------ */
#define CG_EYE_OL    0u   /* eye open — left  outer half              */
#define CG_EYE_OI    1u   /* eye open — left  inner half (has pupil)  */
#define CG_EYE_OR    2u   /* eye open — right inner half (has pupil)  */
#define CG_EYE_ORO   3u   /* eye open — right outer half              */
#define CG_EYE_SQ    4u   /* eye squint half (symmetric, reuse both)  */
#define CG_EYE_WK    5u   /* eye wink  half  (symmetric, reuse both)  */
#define CG_SMILE_CO  6u   /* smile corner segment                     */
#define CG_SMILE_CE  7u   /* smile centre segment                     */

/* ------------------------------------------------------------------ */
/*  LCD positions                                                       */
/* ------------------------------------------------------------------ */
#define EYE_ROW          1u
#define EYE_L_COL_OUTER  2u
#define EYE_L_COL_INNER  3u
#define EYE_R_COL_INNER  16u
#define EYE_R_COL_OUTER  17u

#define SMILE_ROW        3u
#define SMILE_COL_START  6u   /* cols 6-13, 8 cells total             */

/* ------------------------------------------------------------------ */
/*  CGRAM bitmaps  (5 wide x 8 tall, top row = index 0)                */
/* ------------------------------------------------------------------ */

/* Eye open — left outer half */
static const uint8_t bmp_eye_ol[8] = {
    0b00111,
    0b01111,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b01111,
    0b00111
};

/* Eye open — left inner half (pupil cutout on right side) */
static const uint8_t bmp_eye_oi[8] = {
    0b11100,
    0b11110,
    0b11111,
    0b11001,
    0b11001,
    0b11111,
    0b11110,
    0b11100
};

/* Eye open — right inner half (pupil cutout on left side) */
static const uint8_t bmp_eye_or[8] = {
    0b00111,
    0b01111,
    0b11111,
    0b10011,
    0b10011,
    0b11111,
    0b01111,
    0b00111
};

/* Eye open — right outer half */
static const uint8_t bmp_eye_oro[8] = {
    0b11100,
    0b11110,
    0b11111,
    0b11111,
    0b11111,
    0b11111,
    0b11110,
    0b11100
};

/* Eye squint — symmetric, reused for both left and right halves */
static const uint8_t bmp_eye_sq[8] = {
    0b00000,
    0b00000,
    0b00111,
    0b01111,
    0b11111,
    0b11111,
    0b01111,
    0b00111
};

/* Eye wink — closed, symmetric */
static const uint8_t bmp_eye_wk[8] = {
    0b00000,
    0b00000,
    0b00000,
    0b01111,
    0b11111,
    0b01111,
    0b00000,
    0b00000
};

/*
 * Smile arc — spans cols 6-13 on row 3 only.
 *
 * The arc is a crescent (hollow) shape. Thinking of the 8-cell mouth
 * as a 40px-wide x 8px-tall canvas:
 *
 *   Corners (col 6, col 13): full vertical stroke on the outer edge
 *                             plus the arc top pixels at rows 0-1.
 *   Segments (col 7, col 12): arc at rows 3-4.
 *   Segments (col 8, col 11): arc at rows 5-6.
 *   Centre   (col 9, col 10): arc at row 7 only (bottom of character).
 *
 * We only need TWO bitmaps because of symmetry:
 *   CG_SMILE_CO — corner  (used at col 6 left-edge and col 13 right-edge)
 *   CG_SMILE_CE — centre  (used for cols 7-12, arc position encoded by
 *                           which rows are lit)
 *
 * BUT: the corner cells differ (left corner has stroke on bit4, right
 * on bit0) and the centre cells all have different row positions — so
 * we cannot reuse just two bitmaps for everything. We only have 8 CGRAM
 * slots total and 6 are already used, leaving 2.
 *
 * Solution: use lcd_sendCommand() to reload CGRAM on the fly between
 * drawing each mouth cell — OR — accept that with 2 slots we can draw
 * a simplified mouth using repeated writes.
 *
 * Simpler practical approach: use the 2 remaining slots for the LEFT
 * corner and the CENTRE, then mirror for RIGHT corner by reloading
 * CG_SMILE_CO before drawing col 13. The centre columns 7-12 each need
 * a different row pattern so we reload CG_SMILE_CE for each one.
 * The mouth is static (drawn once at init) so this reload cost is paid
 * only once at startup.
 *
 * Left corner  (col 6):  outer left edge stroke, arc at rows 0-1
 * Right corner (col 13): outer right edge stroke, arc at rows 0-1 (mirror)
 *
 * Centre bitmaps for each column offset (cols 7-12):
 *   offset 0 (col 7):  rows 3,4 lit
 *   offset 1 (col 8):  rows 5,6 lit
 *   offset 2 (col 9):  row  7   lit
 *   offset 3 (col 10): row  7   lit  (same as offset 2)
 *   offset 4 (col 11): rows 5,6 lit  (same as offset 1)
 *   offset 5 (col 12): rows 3,4 lit  (same as offset 0)
 */

/* Smile left corner */
static const uint8_t bmp_smile_corner_L[8] = {
    0b11111,  /* row 0 — top of arc at corner                        */
    0b11111,  /* row 1                                               */
    0b10000,  /* row 2 — left edge stroke only                       */
    0b10000,
    0b10000,
    0b10000,
    0b10000,
    0b10000   /* row 7                                               */
};

/* Smile right corner (mirror) */
static const uint8_t bmp_smile_corner_R[8] = {
    0b11111,
    0b11111,
    0b00001,
    0b00001,
    0b00001,
    0b00001,
    0b00001,
    0b00001
};

/* Smile centre segments — 6 variants for cols 7-12 */
static const uint8_t bmp_smile_c0[8] = { 0,0,0,0b11111,0b11111,0,0,0 }; /* col 7  */
static const uint8_t bmp_smile_c1[8] = { 0,0,0,0,0,0b11111,0b11111,0 }; /* col 8  */
static const uint8_t bmp_smile_c2[8] = { 0,0,0,0,0,0,0,0b11111       }; /* col 9  */
static const uint8_t bmp_smile_c3[8] = { 0,0,0,0,0,0,0,0b11111       }; /* col 10 */
static const uint8_t bmp_smile_c4[8] = { 0,0,0,0,0,0b11111,0b11111,0 }; /* col 11 */
static const uint8_t bmp_smile_c5[8] = { 0,0,0,0b11111,0b11111,0,0,0 }; /* col 12 */

static const uint8_t * const smile_centre_bmps[6] = {
    bmp_smile_c0, bmp_smile_c1, bmp_smile_c2,
    bmp_smile_c3, bmp_smile_c4, bmp_smile_c5
};

/* ------------------------------------------------------------------ */
/*  Animation frame table                                               */
/* ------------------------------------------------------------------ */
typedef struct {
    uint8_t eye_L_outer;   /* CGRAM index for left  eye outer half   */
    uint8_t eye_L_inner;   /* CGRAM index for left  eye inner half   */
    uint8_t eye_R_inner;   /* CGRAM index for right eye inner half   */
    uint8_t eye_R_outer;   /* CGRAM index for right eye outer half   */
} FaceFrame;

/*
 * For squint we reuse CG_EYE_SQ for all four slots.
 * For wink  we reuse CG_EYE_WK for the right eye slots only.
 */
static const FaceFrame anim_frames[ANIM_LEN] = {
    /* 0  idle */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_OR, CG_EYE_ORO },
    /* 1  idle */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_OR, CG_EYE_ORO },
    /* 2  idle */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_OR, CG_EYE_ORO },
    /* 3  idle */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_OR, CG_EYE_ORO },
    /* 4  left squints  */ { CG_EYE_SQ, CG_EYE_SQ, CG_EYE_OR,  CG_EYE_ORO },
    /* 5  both squint   */ { CG_EYE_SQ, CG_EYE_SQ, CG_EYE_SQ,  CG_EYE_SQ  },
    /* 6  both squint   */ { CG_EYE_SQ, CG_EYE_SQ, CG_EYE_SQ,  CG_EYE_SQ  },
    /* 7  return        */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_OR,  CG_EYE_ORO },
    /* 8  idle          */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_OR,  CG_EYE_ORO },
    /* 9  wink          */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_WK,  CG_EYE_WK  },
    /* 10 wink held     */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_WK,  CG_EYE_WK  },
    /* 11 reopen        */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_OR,  CG_EYE_ORO },
    /* 12 idle          */ { CG_EYE_OL, CG_EYE_OI, CG_EYE_OR,  CG_EYE_ORO },
};

/* ------------------------------------------------------------------ */
/*  Internal helpers                                                    */
/* ------------------------------------------------------------------ */
static void load_cgram(uint8_t index, const uint8_t *bitmap)
{
    uint8_t i;
    lcd_sendCommand(0x40u | (uint8_t)(index << 3u));
    for (i = 0u; i < 8u; i++) {
        lcd_putc((char)bitmap[i]);
    }
    lcd_sendCommand(0x80u); /* return to DDRAM */
}

static void draw_eyes(const FaceFrame *fr)
{
    lcd_setCursorPos(EYE_L_COL_OUTER, EYE_ROW);
    lcd_putc((char)fr->eye_L_outer);
    lcd_setCursorPos(EYE_L_COL_INNER, EYE_ROW);
    lcd_putc((char)fr->eye_L_inner);

    lcd_setCursorPos(EYE_R_COL_INNER, EYE_ROW);
    lcd_putc((char)fr->eye_R_inner);
    lcd_setCursorPos(EYE_R_COL_OUTER, EYE_ROW);
    lcd_putc((char)fr->eye_R_outer);
}

/*
 * draw_smile() reloads CGRAM slots 6 and 7 for each mouth cell.
 * Called once during face_init() — cost is paid only at startup.
 */
static void draw_smile(void)
{
    uint8_t i;

    /* Left corner */
    load_cgram(CG_SMILE_CO, bmp_smile_corner_L);
    lcd_setCursorPos(SMILE_COL_START, SMILE_ROW);
    lcd_putc((char)CG_SMILE_CO);

    /* Centre segments cols 7-12 */
    for (i = 0u; i < 6u; i++) {
        load_cgram(CG_SMILE_CE, smile_centre_bmps[i]);
        lcd_setCursorPos((uint8_t)(SMILE_COL_START + 1u + i), SMILE_ROW);
        lcd_putc((char)CG_SMILE_CE);
    }

    /* Right corner */
    load_cgram(CG_SMILE_CO, bmp_smile_corner_R);
    lcd_setCursorPos((uint8_t)(SMILE_COL_START + 7u), SMILE_ROW);
    lcd_putc((char)CG_SMILE_CO);

    /* Restore CG_SMILE_CO to left corner so CGRAM is consistent */
    load_cgram(CG_SMILE_CO, bmp_smile_corner_L);
}

/* ------------------------------------------------------------------ */
/*  Public API                                                          */
/* ------------------------------------------------------------------ */

/*
 * face_init()
 * Call once after lcd_init(). Loads all CGRAM characters and draws
 * the static smile. Must be called before face_run() or face_tick().
 */
void face_init(void)
{
    lcd_clear();

    load_cgram(CG_EYE_OL,  bmp_eye_ol);
    load_cgram(CG_EYE_OI,  bmp_eye_oi);
    load_cgram(CG_EYE_OR,  bmp_eye_or);
    load_cgram(CG_EYE_ORO, bmp_eye_oro);
    load_cgram(CG_EYE_SQ,  bmp_eye_sq);
    load_cgram(CG_EYE_WK,  bmp_eye_wk);

    draw_smile();

    /* Draw first frame */
    draw_eyes(&anim_frames[0]);
}

/*
 * face_tick()
 * Advances the animation by one frame and redraws the eyes.
 * Call this every FACE_TICK_MS milliseconds from your own loop,
 * or just call face_run() to let it loop forever internally.
 */
void face_tick(void)
{
    static uint8_t tick = 0u;
    draw_eyes(&anim_frames[tick]);
    tick++;
    if (tick >= ANIM_LEN) {
        tick = 0u;
    }
}

/*
 * face_run()
 * Blocking infinite loop. Calls face_tick() every FACE_TICK_MS ms.
 * Does not return.
 */
void face_run(void)
{
    while (1) {
        face_tick();
        timer_waitMillis(TICK_MS);
    }
}
