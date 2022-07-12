// SPDX-License-Identifier: GPL-3.0-or-later

#include "drivers/screen.h"

#include "stdlib.h"

#define VIDEO_DEVICE_ADDRESS 0xB8000
#define VIDEO_WIDTH 80
#define VIDEO_HEIGHT 25

typedef struct
{
    char character;
    u8 color;
} __attribute__((packed)) video_cell_t;

typedef struct
{
    video_cell_t cells[VIDEO_HEIGHT][VIDEO_WIDTH];
} __attribute__((packed)) video_buffer_t;

static video_buffer_t *video_buffer = (video_buffer_t *) VIDEO_DEVICE_ADDRESS;
static u8 cursor_x = 0;
static u8 cursor_y = 0;

int screen_init()
{
    screen_clear();
    return 0;
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

int screen_move_cursor(u32 x, u32 y)
{
    cursor_x = x;
    cursor_y = y;
    return 0;
}

int screen_get_width()
{
    return VIDEO_WIDTH;
}

int screen_get_height()
{
    return VIDEO_HEIGHT;
}

int screen_print_char_at(u32 x, u32 y, char c, TextModeColor fg, TextModeColor bg)
{
    if (x < 0 || y < 0 || x >= VIDEO_WIDTH || y >= VIDEO_HEIGHT)
        return -1;
    video_buffer->cells[y][x].character = c;
    video_buffer->cells[y][x].color = bg << 4 | fg;
    return 0;
}

int screen_print_string_at(u32 x, u32 y, const char *str, TextModeColor fg, TextModeColor bg)
{
    size_t str_len = strlen(str);
    for (u32 i = 0; i < str_len; i++)
        screen_print_char_at(x + i, y, str[i], fg, bg);
    return 0;
}

int screen_print_string(const char *str)
{
    return screen_print_string_colored(str, White, Black);
}

int screen_print_string_colored(const char *str, TextModeColor fg, TextModeColor bg)
{
    int r = 0;
    char *video = (char *) VIDEO_DEVICE_ADDRESS;
    for (; *str; str++, r++)
    {
        char c = *str;
        *video = c;
        *(video + 1) = bg << 4 | fg;

        video += 2;
    }

    return r;
}

void screen_cursur_enable(u8 cursor_start, u8 cursor_end)
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, (inb(0x3D5) & 0xC0) | cursor_start);

    outb(0x3D4, 0x0B);
    outb(0x3D5, (inb(0x3D5) & 0xE0) | cursor_end);
}

void screen_cursor_disable()
{
    outb(0x3D4, 0x0A);
    outb(0x3D5, 0x20);
}
