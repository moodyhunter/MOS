// SPDX-License-Identifier: GPL-3.0-or-later

#include "readline/libreadline.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define LINE_BUFFER_SIZE 1024

char *readline(const char *prompt)
{
    const size_t prompt_length = strlen(prompt);
    printf("%s", prompt);
    fflush(stdout);

    char *line_buffer = malloc(LINE_BUFFER_SIZE);
    if (!line_buffer)
        return NULL;

    memset(line_buffer, 0, LINE_BUFFER_SIZE);

    int insertion_point = 0; // index of the character that will be overwritten
    int line_length = 0;     // not including the null terminator

    while (true)
    {
        fflush(stdout);
        char c = getchar();

        switch (c)
        {
            case 1:
                insertion_point = 0;
                const size_t pos = prompt_length + 1 + insertion_point;
                printf("\033[%zuG", pos);
                break;
            case 5:
                insertion_point = line_length;
                const size_t pos2 = prompt_length + 1 + insertion_point;
                printf("\033[%zuG", pos2);
                break;
            case '\r':
            case '\n':
                line_buffer[line_length] = '\0';
                fflush(stdout);
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
                        printf("\b \b"); // move cursor back, clear last character, move cursor back again
                    }
                    else
                    {
                        // move the rest of the line to the left
                        insertion_point--;
                        line_length--;
                        printf("\b"); // move cursor back

                        for (int i = insertion_point; i < line_length; i++)
                        {
                            line_buffer[i] = line_buffer[i + 1];
                            printf("%c", line_buffer[i]);
                        }

                        // clear the last character
                        printf(" \b");

                        // move the cursor back to the insertion point
                        for (int i = insertion_point; i < line_length; i++)
                            printf("\b");
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
                            insertion_point--;
                        else
                            printf("\033[C"); // move the cursor back to the insertion point
                    }
                    else if (c == 'C')
                    {
                        if (insertion_point < line_length)
                            insertion_point++;
                        else
                            printf("\033[D"); // move the cursor back to the insertion point
                    }
                    else if (c == 'A')
                    {
                        printf("\033[B"); // move the cursor back to the insertion point
                    }
                    else if (c == 'B')
                    {
                        printf("\033[A"); // move the cursor back to the insertion point
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

                                line_length--;

                                // move the cursor back
                                for (int i = insertion_point; i < line_length; i++)
                                    printf("\b");
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
                if (line_length + 1 >= LINE_BUFFER_SIZE)
                {
                    // buffer is full
                    puts("readline: line too long");
                    abort();
                    break;
                }

                line_length++;

                // if we're not at the end of the line, we need to move the rest of the line
                if (insertion_point < line_length)
                {
                    // move the rest of the line to the right
                    for (int i = line_length; i > insertion_point; i--)
                    {
                        line_buffer[i] = line_buffer[i - 1];
                    }

                    // print the rest of the line
                    for (int i = insertion_point + 1; i < line_length; i++)
                    {
                        printf("%c", line_buffer[i]);
                    }

                    // move the cursor back to the insertion point
                    for (int i = insertion_point + 1; i < line_length; i++)
                    {
                        printf("\b");
                    }
                }

                line_buffer[insertion_point] = c;
                insertion_point++;
                break;
        }
    }

    fflush(stdout);
    return "";
}

char *get_line(fd_t fd)
{
    char *line_buffer = malloc(LINE_BUFFER_SIZE);
    if (!line_buffer)
        return NULL;

    memset(line_buffer, 0, LINE_BUFFER_SIZE);

    char c;
    int i = 0;
    while (i < LINE_BUFFER_SIZE - 1)
    {
        size_t readsz = read(fd, &c, 1);

        if (readsz == 0 && i == 0)
        {
            free(line_buffer);
            return NULL; // EOF
        }

        if (readsz != 1)
            break;

        if (c == '\n')
            break;

        line_buffer[i] = c;
        i++;
    }

    line_buffer[i] = '\0';
    return line_buffer;
}
