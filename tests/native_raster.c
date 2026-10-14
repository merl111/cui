/* Check actual native text/icon pixels, not just successful render calls. */
#include "cui.h"
#include "cui_draw.h"
#include <assert.h>
#include <stdio.h>

static unsigned char pixels[160 * 48 * 4];

static void visible_pixels(cui_surface *surface, int translucent)
{
    int width, height, visible = 0, saturated = 0;
    cui_surface_read(surface, pixels, sizeof(pixels), &width, &height);
    assert(width == 160 && height == 48);
    for (int i = 0; i < width * height; ++i) {
        unsigned char *p = pixels + i * 4;
        if (p[3] > 64) {
            ++visible;
            if (translucent) {
                /* Subpixel antialiasing can desaturate edge pixels. Interior
                 * pixels must still retain red rather than multiply alpha twice. */
                if (p[0] > 220 && p[1] < 8 && p[2] < 8 && p[3] > 100)
                    ++saturated;
                assert(p[3] <= 129);
            }
        }
    }
    assert(visible > 20);
    if (translucent) assert(saturated > 20);
}

int main(void)
{
    cui_app *app = cui_app_create();
    assert(app);
    cui_surface *surface = cui_surface_create(160, 48, 1);
    assert(surface);
    cui_draw_command commands[] = {
        {.op = CUI_DRAW_CLEAR},
        {.op = CUI_DRAW_TEXT, .p = {4, 4, 152, 20, 600},
         .text = "Readable text", .font = "sans", .color = 0xff000080u}
    };
    assert(cui_surface_render(surface, commands, 2));
    visible_pixels(surface, 1);
    cui_icon_asset *icon = cui_icon_symbol(CUI_SYMBOL_HOME);
    assert(icon);
    commands[1] = (cui_draw_command){.op = CUI_DRAW_ICON,
        .p = {8, 8, 32, 32}, .icon = icon, .color = 0xff000080u};
    assert(cui_surface_render(surface, commands, 2));
    visible_pixels(surface, 1);
    cui_icon_release(icon);
    commands[1] = (cui_draw_command){.op = CUI_DRAW_TEXT,
        .p = {4, 4, 152, 24, 400}, .text = "👍", .color = 0xffffffffu};
    assert(cui_surface_render(surface, commands, 2));
    visible_pixels(surface, 0);
    cui_surface_release(surface);
    cui_app_destroy(app);
    puts("Native text, translucent icons and emoji have visible pixels.");
    return 0;
}
