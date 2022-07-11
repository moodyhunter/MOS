// SPDX-License-Identifier: GPL-3.0-or-later

const char *const VIDEO_MEMORY = (char *) 0xB8000;

void print_string(const char *str)
{
    char *video = (char *) VIDEO_MEMORY;
    for (; *str; str++)
    {
        char c = *str;
        *video = c;
        *(video + 1) = 0xca;

        video += 2;
    }
}
