/*
 * LICENSE available at the bottom
 */

// GENERATE A LARGE AND COMPLEX CSV FILE
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>

#define NUM_COLUMNS 500
#define NUM_ROWS (20 * 1000)
#define MAX_WORD_SIZE 300
#define MIN_WORD_SIZE 4 // If it becomes "quoted" then it erases two. So 2 is minimum

#define CHARACTERS "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789"

uint32_t g_seed;

uint32_t inline our_rand() {
    g_seed = (214013*g_seed+2531011);
    return (g_seed>>16)&0x7FFF;
}

int main(int argc, char *argv[]) {
    int newline_len;
    char *newline;
#if IS_WINDOWS
    newline = "\r\n";
    newline_len = 2;
#else
    newline = "\n";
    newline_len = 1;
#endif
    char word[MAX_WORD_SIZE];
    int word_len;

    FILE *fh = fopen("generated.csv", "wb");
    if (!fh) {
        printf("Unable to create or open generated.csv\n");
        return EXIT_FAILURE;
    }

    setvbuf(fh, NULL, _IOFBF, 1024 * 1024 * 1024);

    srand(time(NULL));

    for (unsigned long long row = 0; row < NUM_ROWS; ++row) {
        for (unsigned long long column = 0; column < NUM_COLUMNS; ++column) {
            int quoted = our_rand() % 2;
            word_len = (our_rand() % (MAX_WORD_SIZE - MIN_WORD_SIZE)) + MIN_WORD_SIZE;

            for (int i = 0; i < word_len; ++i) {
                word[i] = CHARACTERS[our_rand() % (sizeof(CHARACTERS) - 1)];
            }

            if (quoted) {
                word[0] = '\"';
                word[word_len - 1] = '\"';
            }

            fwrite(word, 1, word_len, fh);
            if (column + 1 != NUM_COLUMNS) {
                fwrite(",", 1, 1, fh);
            }
        }
        fwrite(newline, 1, newline_len, fh);

        if (row % 10000 == 0) {
            printf("%llu rows written\n", row);
        }
    }

    fclose(fh);

    return EXIT_SUCCESS;
}

/*
------------------------------------------------------------------------------
This software is available under 2 licenses -- choose whichever you prefer.
------------------------------------------------------------------------------
ALTERNATIVE A - MIT License
Copyright (c) 2017 Ian Thomassen Jacobsen
Permission is hereby granted, free of charge, to any person obtaining a copy of 
this software and associated documentation files (the "Software"), to deal in 
the Software without restriction, including without limitation the rights to 
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies 
of the Software, and to permit persons to whom the Software is furnished to do 
so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all 
copies or substantial portions of the Software.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER 
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, 
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE 
SOFTWARE.
------------------------------------------------------------------------------
ALTERNATIVE B - Public Domain (www.unlicense.org)
This is free and unencumbered software released into the public domain.
Anyone is free to copy, modify, publish, use, compile, sell, or distribute this 
software, either in source code form or as a compiled binary, for any purpose, 
commercial or non-commercial, and by any means.
In jurisdictions that recognize copyright laws, the author or authors of this 
software dedicate any and all copyright interest in the software to the public 
domain. We make this dedication for the benefit of the public at large and to 
the detriment of our heirs and successors. We intend this dedication to be an 
overt act of relinquishment in perpetuity of all present and future rights to 
this software under copyright law.
THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR 
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, 
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE 
AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN 
ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION 
WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
------------------------------------------------------------------------------
*/
