// SPDX-License-Identifier: GPL-3.0-or-later

#include "mos/x86/devices/text_mode_console.h"

#include "lib/string.h"
#include "mos/device/console.h"
#include "mos/mos_global.h"
#include "mos/x86/devices/port.h"
#include "mos/x86/x86_platform.h"

#define VIDEO_WIDTH  80
#define VIDEO_HEIGHT 25

typedef struct
{
    char character;
    u8 color;
} __packed video_cell_t;

typedef struct
{
    video_cell_t cells[VIDEO_HEIGHT][VIDEO_WIDTH];
} __packed video_buffer_t;

static const size_t VIDEO_LINE_SIZE = VIDEO_WIDTH * sizeof(video_cell_t);

static video_buffer_t *video_buffer = (video_buffer_t *) BIOS_VADDR(X86_VIDEO_DEVICE_PADDR);
static u8 cursor_x = 0;
static u8 cursor_y = 0;
static standard_color_t foreground_color = White;
static standard_color_t background_color = Black;

static void screen_scroll(void)
{
    memmove(video_buffer, video_buffer->cells[1], (VIDEO_HEIGHT - 1) * VIDEO_LINE_SIZE);
    memset((void *) (video_buffer->cells[VIDEO_HEIGHT - 1]), 0, VIDEO_LINE_SIZE);
}

static bool screen_get_size(console_t *console, u32 *width, u32 *height)
{
    MOS_UNUSED(console);
    if (width)
        *width = VIDEO_WIDTH;
    if (height)
        *height = VIDEO_HEIGHT;
    return width || height;
}

static bool screen_get_cursor_pos(console_t *console, u32 *x, u32 *y)
{
    MOS_UNUSED(console);
    if (!x && !y)
        return false;
    if (x)
        *x = cursor_x;
    if (y)
        *y = cursor_y;
    return true;
}

static bool screen_set_cursor_pos(console_t *console, u32 x, u32 y)
{
    MOS_UNUSED(console);
    if (x >= VIDEO_WIDTH || y >= VIDEO_HEIGHT)
        return false;

    cursor_x = x;
    cursor_y = y;

    u16 pos = y * VIDEO_WIDTH + x;

    port_outb(0x3D4, 0x0F);
    port_outb(0x3D5, (u8) (pos & 0xFF));
    port_outb(0x3D4, 0x0E);
    port_outb(0x3D5, (u8) ((pos >> 8) & 0xFF));

    return true;
}

static void screen_print_char(console_t *console, char c)
{
    if (c == '\n')
    {
        cursor_x = 0;
        cursor_y++;
    }
    else
    {
        video_buffer->cells[cursor_y][cursor_x].character = c;
        video_buffer->cells[cursor_y][cursor_x].color = background_color << 4 | foreground_color;
        cursor_x++;
    }
    if (cursor_x >= VIDEO_WIDTH)
    {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VIDEO_HEIGHT)
    {
        screen_scroll();
        cursor_y--;
    }
    screen_set_cursor_pos(console, cursor_x, cursor_y);
}

static bool screen_enable_cursur(console_t *console, bool enable)
{
    MOS_UNUSED(console);
    if (enable)
    {
        u8 start_scanline = 13, end_scanline = 15;
        port_outb(0x3D4, 0x0A);
        port_outb(0x3D5, (port_inb(0x3D5) & 0xC0) | start_scanline);

        port_outb(0x3D4, 0x0B);
        port_outb(0x3D5, (port_inb(0x3D5) & 0xE0) | end_scanline);
    }
    else
    {
        port_outb(0x3D4, 0x0A);
        port_outb(0x3D5, 0x20);
    }
    return true;
}

static bool screen_get_color(console_t *console, standard_color_t *fg, standard_color_t *bg)
{
    MOS_UNUSED(console);
    if (fg)
        *fg = foreground_color;
    if (bg)
        *bg = background_color;
    return fg || bg;
}

static bool screen_set_color(console_t *console, standard_color_t fg, standard_color_t bg)
{
    MOS_UNUSED(console);
    foreground_color = fg;
    background_color = bg;
    return true;
}

static int screen_print_string(console_t *console, const char *str, size_t limit)
{
    MOS_UNUSED(console);
    size_t i = 0;
    for (i = 0; (str[i] != '\0') && (i < limit); i++)
        screen_print_char(console, str[i]);
    return i;
}

static bool screen_clear(console_t *console)
{
    MOS_UNUSED(console);
    int i = 0;
    for (i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
        screen_print_char(console, ' ');
    cursor_x = 0;
    cursor_y = 0;
    return true;
}

console_t vga_text_mode_console = {
    .name = "x86_vga_text_mode_console",
    .list_node = LIST_NODE_INIT(vga_text_mode_console),
    .caps = CONSOLE_CAP_COLOR | CONSOLE_CAP_CLEAR | CONSOLE_CAP_CURSOR_HIDE | CONSOLE_CAP_CURSOR_MOVE,
    .set_cursor = screen_enable_cursur,
    .get_size = screen_get_size,
    .move_cursor = screen_set_cursor_pos,
    .get_cursor = screen_get_cursor_pos,
    .get_color = screen_get_color,
    .set_color = screen_set_color,
    .write_impl = screen_print_string,
    .clear = screen_clear,
};
