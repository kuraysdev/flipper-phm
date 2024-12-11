#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>

void draw_utf8_str(Canvas* canvas, uint8_t x, uint8_t y, const char* str) {
    FuriStringUTF8State state = FuriStringUTF8StateStarting;
    FuriStringUnicodeValue value = 0;

    for(; *str; str++) {
        furi_string_utf8_decode(*str, &state, &value);
        if(state == FuriStringUTF8StateError) furi_crash(NULL);

        if(state == FuriStringUTF8StateStarting) {
            canvas_draw_glyph(canvas, x, y, value);

            // Only one-byte glyphs supported by canvas_glyph_width
            x += value <= 0xFF ? canvas_glyph_width(canvas, value) :
                                 canvas_glyph_width(canvas, ' ');
        }
    }
}