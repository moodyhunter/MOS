
// SPDX-License-Identifier: MIT
// SPDX-FileCopyrightText: © 2014 Maurits van der Schee

/* Console version of the game "2048" for MOS */

// Adapted from the u-boot project
// https://github.com/u-boot/u-boot/commit/a554ee7edee8e10b38c6899ad8556daf58ca3afe

#include <mos/types.h>
#include <stdio.h>
#include <string.h>

#define SIZE 4
static u32 score;

// a very pseudo random number generator
should_inline u32 rand(void)
{
    u32 result;
#if defined(__x86_64__)
    __asm__ volatile("rdtsc" : "=a"(result) : : "edx");
#elif defined(__riscv)
    __asm__ volatile("rdcycle %0" : "=r"(result));
#else
#error "unsupported architecture"
#endif
    result ^= (ptr_t) &result;
    return result;
}

static void getColor(u32 value, char *color, size_t length)
{
    u8 original[] = { 8, 255, 1, 255, 2, 255, 3, 255, 4, 255, 5, 255, 6, 255, 7, 255, 9, 0, 10, 0, 11, 0, 12, 0, 13, 0, 14, 0, 255, 0, 255, 0 };
    u8 *scheme = original;
    u8 *background = scheme + 0;
    u8 *foreground = scheme + 1;

    if (value > 0)
    {
        while (value >>= 1)
        {
            if (background + 2 < scheme + sizeof(original))
            {
                background += 2;
                foreground += 2;
            }
        }
    }
    snprintf(color, length, "\033[38;5;%d;48;5;%dm", *foreground, *background);
}

static void drawBoard(u16 board[SIZE][SIZE])
{
    int x, y;
    char color[40], reset[] = "\033[0m";

    printf("\033[H");
    printf("2048.c %17d pts\n\n", score);

    for (y = 0; y < SIZE; y++)
    {
        for (x = 0; x < SIZE; x++)
        {
            getColor(board[x][y], color, 40);
            printf("%s", color);
            printf("       ");
            printf("%s", reset);
        }
        printf("\n");
        for (x = 0; x < SIZE; x++)
        {
            getColor(board[x][y], color, 40);
            printf("%s", color);
            if (board[x][y] != 0)
            {
                char s[8];
                s8 t;

                snprintf(s, 8, "%u", board[x][y]);
                t = 7 - strlen(s);
                printf("%*s%s%*s", t - t / 2, "", s, t / 2, "");
            }
            else
            {
                printf("   ·   ");
            }
            printf("%s", reset);
        }
        printf("\n");
        for (x = 0; x < SIZE; x++)
        {
            getColor(board[x][y], color, 40);
            printf("%s", color);
            printf("       ");
            printf("%s", reset);
        }
        printf("\n");
    }
    printf("\n");
    printf("        ←, ↑, →, ↓ or q        \n");
    printf("\033[A");
}

static s8 findTarget(u16 array[SIZE], int x, int stop)
{
    int t;

    /* if the position is already on the first, don't evaluate */
    if (x == 0)
        return x;
    for (t = x - 1; t >= 0; t--)
    {
        if (array[t])
        {
            if (array[t] != array[x])
            {
                /* merge is not possible, take next position */
                return t + 1;
            }
            return t;
        }

        /* we should not slide further, return this one */
        if (t == stop)
            return t;
    }
    /* we did not find a */
    return x;
}

static bool slideArray(u16 array[SIZE])
{
    bool success = false;
    int x, t, stop = 0;

    for (x = 0; x < SIZE; x++)
    {
        if (array[x] != 0)
        {
            t = findTarget(array, x, stop);
            /*
             * if target is not original position, then move or
             * merge
             */
            if (t != x)
            {
                /*
                 * if target is not zero, set stop to avoid
                 * double merge
                 */
                if (array[t])
                {
                    score += array[t] + array[x];
                    stop = t + 1;
                }
                array[t] += array[x];
                array[x] = 0;
                success = true;
            }
        }
    }
    return success;
}

static void rotateBoard(u16 board[SIZE][SIZE])
{
    s8 i, j, n = SIZE;
    int tmp;

    for (i = 0; i < n / 2; i++)
    {
        for (j = i; j < n - i - 1; j++)
        {
            tmp = board[i][j];
            board[i][j] = board[j][n - i - 1];
            board[j][n - i - 1] = board[n - i - 1][n - j - 1];
            board[n - i - 1][n - j - 1] = board[n - j - 1][i];
            board[n - j - 1][i] = tmp;
        }
    }
}

static bool moveUp(u16 board[SIZE][SIZE])
{
    bool success = false;
    int x;

    for (x = 0; x < SIZE; x++)
        success |= slideArray(board[x]);

    return success;
}

static bool moveLeft(u16 board[SIZE][SIZE])
{
    bool success;

    rotateBoard(board);
    success = moveUp(board);
    rotateBoard(board);
    rotateBoard(board);
    rotateBoard(board);
    return success;
}

static bool moveDown(u16 board[SIZE][SIZE])
{
    bool success;

    rotateBoard(board);
    rotateBoard(board);
    success = moveUp(board);
    rotateBoard(board);
    rotateBoard(board);
    return success;
}

static bool moveRight(u16 board[SIZE][SIZE])
{
    bool success;

    rotateBoard(board);
    rotateBoard(board);
    rotateBoard(board);
    success = moveUp(board);
    rotateBoard(board);
    return success;
}

static bool findPairDown(u16 board[SIZE][SIZE])
{
    bool success = false;
    int x, y;

    for (x = 0; x < SIZE; x++)
    {
        for (y = 0; y < SIZE - 1; y++)
        {
            if (board[x][y] == board[x][y + 1])
                return true;
        }
    }

    return success;
}

static s16 countEmpty(u16 board[SIZE][SIZE])
{
    int x, y;
    int count = 0;

    for (x = 0; x < SIZE; x++)
    {
        for (y = 0; y < SIZE; y++)
        {
            if (board[x][y] == 0)
                count++;
        }
    }
    return count;
}

static bool gameEnded(u16 board[SIZE][SIZE])
{
    bool ended = true;

    if (countEmpty(board) > 0)
        return false;
    if (findPairDown(board))
        return false;
    rotateBoard(board);
    if (findPairDown(board))
        ended = false;
    rotateBoard(board);
    rotateBoard(board);
    rotateBoard(board);

    return ended;
}

static void addRandom(u16 board[SIZE][SIZE])
{
    int x, y;
    int r, len = 0;
    u16 n, list[SIZE * SIZE][2];

    for (x = 0; x < SIZE; x++)
    {
        for (y = 0; y < SIZE; y++)
        {
            if (board[x][y] == 0)
            {
                list[len][0] = x;
                list[len][1] = y;
                len++;
            }
        }
    }

    if (len > 0)
    {
        r = rand() % len;
        x = list[r][0];
        y = list[r][1];
        n = ((rand() % 10) / 9 + 1) * 2;
        board[x][y] = n;
    }
}

int main(int argc, char *argv[])
{
    MOS_UNUSED(argc);
    MOS_UNUSED(argv);

    u16 board[SIZE][SIZE];
    bool success;

    score = 0;

    printf("\033[?25l\033[2J\033[H");

    memset(board, 0, sizeof(board));
    addRandom(board);
    addRandom(board);
    drawBoard(board);

    while (true)
    {
        char c = getchar();
        if (c == 'q')
        {
            printf("            QUIT            \n");
            break;
        }

        if (c != '\033')
            continue;

        c = getchar();
        if (c != '[')
            continue;

        c = getchar(); // A = up, B = down, C = right, D = left
        switch (c)
        {
            case 'A': success = moveUp(board); break;
            case 'B': success = moveDown(board); break;
            case 'C': success = moveRight(board); break;
            case 'D': success = moveLeft(board); break;
            default: success = false;
        }

        if (success)
        {
            drawBoard(board);
            addRandom(board);
            drawBoard(board);
            if (gameEnded(board))
            {
                printf("         GAME OVER          \n");
                break;
            }
        }
    }

    printf("\033[?25h");

    return 0;
}
