// SPDX-License-Identifier: GPL-3.0-or-later

#include "readline/libreadline.h"

#include "mos/moslib_global.h"
#include "string.h"

#include <mos/syscall/usermode.h>
#include <stdio.h>
#include <stdlib.h>

#define LINE_BUFFER_SIZE 1024

char *readline(const char *prompt)
{
    printf("%s", prompt);

    char *line_buffer = malloc(LINE_BUFFER_SIZE);
    if (!line_buffer)
        return NULL;

    memzero(line_buffer, LINE_BUFFER_SIZE);

    int insertion_point = 0; // index of the character that will be overwritten
    int line_length = 0;     // not including the null terminator

    while (true)
    {
        char c = getchar();

        switch (c)
        {
            case '\r':
            case '\n':
                line_buffer[line_length] = '\0';
                printf("\n");
                return line_buffer;
            case '\f':
                printf("\033[2J\033[H"); // clear screen
                printf("%s", prompt);
                break;
            case '\b':
                if (insertion_point > 0)
                {
                    // if at the end of the line, we can just move the insertion point
                    if (insertion_point == line_length)
                    {
                        insertion_point--;
                        line_length--;
                        printf("\b \b");
                    }
                    else
                    {
                        // move the rest of the line to the right
                        for (int i = line_length; i > insertion_point; i--)
                        {
                            line_buffer[i] = line_buffer[i - 1];
                            printf("%c", line_buffer[i]);
                        }

                        // move the cursor forward
                        for (int i = insertion_point; i < line_length; i++)
                            printf("\033[C");

                        // clear the last character
                        printf(" \b");

                        // move the insertion point back
                        insertion_point--;
                        line_length--;
                    }
                }
                break;
            case 0x7f: // backspace
                if (insertion_point > 0)
                {
                    // if at the end of the line, we can just move the insertion point
                    if (insertion_point == line_length)
                    {
                        insertion_point--;
                        line_length--;
                        printf("\b \b");
                    }
                    else
                    {
                        // move the rest of the line to the right
                        for (int i = line_length; i > insertion_point; i--)
                        {
                            line_buffer[i] = line_buffer[i - 1];
                            printf("%c", line_buffer[i]);
                        }

                        // move the cursor forward
                        for (int i = insertion_point; i < line_length; i++)
                            printf("\033[C");

                        // clear the last character
                        printf(" \b");

                        // move the insertion point back
                        insertion_point--;
                        line_length--;
                    }
                }
                break;

            case 0x1b: // ESC
                c = getchar();
                if (c == '[') // is an escape sequence
                {
                    c = getchar();
                    if (c == 'D')
                    {
                        if (insertion_point > 0)
                        {
                            insertion_point--;
                            printf("\b"); // move cursor left
                        }
                    }
                    else if (c == 'C')
                    {
                        if (insertion_point < line_length)
                        {
                            insertion_point++;
                            printf("\033[C"); // move cursor right
                        }
                    }
                    else if (c == '3')
                    {
                        c = getchar();
                        if (c == '~')
                        {
                            if (insertion_point < line_length)
                            {
                                // move the rest of the line to the left
                                for (int i = insertion_point; i < line_length; i++)
                                {
                                    line_buffer[i] = line_buffer[i + 1];
                                    printf("%c", line_buffer[i]);
                                }

                                // clear the last character
                                printf(" \b");

                                // move the cursor back
                                for (int i = insertion_point; i < line_length; i++)
                                    printf("\b");

                                // move the insertion point back
                                line_length--;
                            }
                        }
                    }
                }
                else
                {
                    // ???
                }
                break;
            default:
                if (line_length < LINE_BUFFER_SIZE - 1)
                {
                    line_buffer[insertion_point] = c;
                    insertion_point++;
                    line_length++;
                    printf("%c", c);
                }
                else
                {
                    // buffer is full
                    fatal_abort("readline: line too long");
                    break;
                }

                // if we're not at the end of the line, we need to move the rest of the line
                if (insertion_point < line_length)
                {
                    // move the rest of the line to the right
                    for (int i = line_length; i > insertion_point; i--)
                    {
                        line_buffer[i] = line_buffer[i - 1];
                    }

                    // print the rest of the line
                    for (int i = insertion_point; i < line_length; i++)
                    {
                        printf("%c", line_buffer[i]);
                    }

                    // move the cursor back to the insertion point
                    for (int i = insertion_point; i < line_length; i++)
                    {
                        printf("\b");
                    }
                }

                break;
        }
    }

    return "";
}
