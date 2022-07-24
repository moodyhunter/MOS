// SPDX-License-Identifier: GPL-3.0-or-later

#include "drivers/screen.h"

#include "attributes.h"
#include "drivers/port.h"
#include "stdlib.h"
#include "string.h"

#define VIDEO_DEVICE_ADDRESS 0xB8000
#define VIDEO_WIDTH 80
#define VIDEO_HEIGHT 25

typedef struct
{
    char character;
    u8 color;
} __attr_packed video_cell_t;

typedef struct
{
    video_cell_t cells[VIDEO_HEIGHT][VIDEO_WIDTH];
} __attr_packed video_buffer_t;

static video_buffer_t *video_buffer = (video_buffer_t *) VIDEO_DEVICE_ADDRESS;
static u8 cursor_x = 0;
static u8 cursor_y = 0;
static VGATextModeColor foreground_color = White;
static VGATextModeColor background_color = Black;

bool screen_init()
{
    screen_clear();
    screen_set_cursor_pos(0, 0);
    screen_enable_cursur(13, 15);
    return true;
}

int screen_clear()
{
    int i = 0;
    char *video_ptr = (char *) VIDEO_DEVICE_ADDRESS;
    for (i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
    {
        *video_ptr = ' ';
        video_ptr++;
        *video_ptr = 0x07;
        video_ptr++;
    }
    cursor_x = 0;
    cursor_y = 0;
    return 0;
}

void screen_get_size(u32 *width, u32 *height)
{
    if (width)
        *width = VIDEO_WIDTH;
    if (height)
        *height = VIDEO_HEIGHT;
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

    uint16_t pos = y * VIDEO_WIDTH + x;

    port_outb(0x3D4, 0x0F);
    port_outb(0x3D5, (uint8_t) (pos & 0xFF));
    port_outb(0x3D4, 0x0E);
    port_outb(0x3D5, (uint8_t) ((pos >> 8) & 0xFF));

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
        screen_print_char_at(c, cursor_x, cursor_y);
        cursor_x++;
    }
    if (cursor_x >= VIDEO_WIDTH)
    {
        cursor_x = 0;
        cursor_y++;
    }
    if (cursor_y >= VIDEO_HEIGHT)
    {
        cursor_y = 0;
        screen_clear();
    }
}

bool screen_print_char_at(char c, u32 x, u32 y)
{
    if (x >= VIDEO_WIDTH || y >= VIDEO_HEIGHT)
        return false;
    video_buffer->cells[y][x].character = c;
    video_buffer->cells[y][x].color = background_color << 4 | foreground_color;
    return true;
}

int screen_print_string(const char *str)
{
    int i = 0;
    for (i = 0; str[i] != '\0'; i++)
        screen_print_char(str[i]);
    return i;
}

int screen_print_string_at(const char *str, u32 x, u32 y)
{
    int i = 0;
    for (int i = 0; str[i] != '\0'; i++)
        screen_print_char_at(str[i], x + i, y);
    return i;
}

void screen_set_color(VGATextModeColor fg, VGATextModeColor bg)
{
    foreground_color = fg;
    background_color = bg;
}

void screen_enable_cursur(u8 start_scanline, u8 end_scanline)
{
    port_outb(0x3D4, 0x0A);
    port_outb(0x3D5, (port_inb(0x3D5) & 0xC0) | start_scanline);

    port_outb(0x3D4, 0x0B);
    port_outb(0x3D5, (port_inb(0x3D5) & 0xE0) | end_scanline);
}

void screen_disable_cursor()
{
    port_outb(0x3D4, 0x0A);
    port_outb(0x3D5, 0x20);
}

void screen_scroll(void)
{
    int i = 0;
    char *video_ptr = (char *) VIDEO_DEVICE_ADDRESS;
    for (i = 0; i < VIDEO_WIDTH * VIDEO_HEIGHT; i++)
    {
        *video_ptr = ' ';
        video_ptr++;
        *video_ptr = 0x07;
        video_ptr++;
    }
    cursor_x = 0;
    cursor_y = 0;
}
