// SPDX-License-Identifier: GPL-3.0-or-later

#include <mos/device/dm_types.h>
#include <mos/x86/devices/port.h>
#include <string.h>

#define VIDEO_WIDTH  80
#define VIDEO_HEIGHT 25

typedef struct
{
    struct
    {
        char character;
        u8 color;
    } __packed cells[VIDEO_HEIGHT][VIDEO_WIDTH];
} __packed video_buffer_t;

static const size_t VIDEO_LINE_SIZE = VIDEO_WIDTH * (sizeof(char) + sizeof(u8));

static video_buffer_t *video_buffer = NULL;
static u8 cursor_x = 0;
static u8 cursor_y = 0;
static standard_color_t foreground_color = White;
static standard_color_t background_color = Black;

static void screen_scroll(void)
{
    memmove(video_buffer, video_buffer->cells[1], (VIDEO_HEIGHT - 1) * VIDEO_LINE_SIZE);
    memset((void *) (video_buffer->cells[VIDEO_HEIGHT - 1]), 0, VIDEO_LINE_SIZE);
}

bool screen_get_size(u32 *width, u32 *height)
{
    if (width)
        *width = VIDEO_WIDTH;
    if (height)
        *height = VIDEO_HEIGHT;
    return width || height;
}

bool screen_get_cursor_pos(u32 *x, u32 *y)
{
    if (!x && !y)
        return false;
    if (x)
        *x = cursor_x;
    if (y)
        *y = cursor_y;
    return true;
}

bool screen_set_cursor_pos(u32 x, u32 y)
{
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

void screen_print_char(char c)
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
    screen_set_cursor_pos(cursor_x, cursor_y);
}

bool screen_enable_cursur(bool enable)
{
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

bool screen_get_color(standard_color_t *fg, standard_color_t *bg)
{
    if (fg)
        *fg = foreground_color;
    if (bg)
        *bg = background_color;
    return fg || bg;
}

bool screen_set_color(standard_color_t fg, standard_color_t bg)
{
    foreground_color = fg;
    background_color = bg;
    return true;
}

int screen_print_string(const char *str, size_t limit)
{
    size_t i = 0;
    for (i = 0; (str[i] != '\0') && (i < limit); i++)
        screen_print_char(str[i]);
    return i;
}

bool screen_clear(void)
{
    int i = 0;
    for (i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
        screen_print_char(' ');
    cursor_x = 0;
    cursor_y = 0;
    return true;
}

void x86_vga_text_mode_console_init(ptr_t video_buffer_addr)
{
    video_buffer = (video_buffer_t *) video_buffer_addr;
}
