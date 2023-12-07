/*
 * LICENSE available at the bottom
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#define ITJ_CSV_IMPLEMENTATION_AVX2
#include "itj_csv.h"

#define KB(x) (x * 1024)
#define SIZE_OF_NULL_TERMINATOR 1
#define SIZE_OF_DIR_SEPERATOR 1

#ifdef IS_WINDOWS
#define DIR_SEPERATOR "\\"
#else
#define DIR_SEPERATOR "/"
#endif

#define DATE_START 1960
#define DATE_END 2022

struct string {
    char *base;
    itj_csv_umax len;
};

struct entry {
    struct string country_name;
    struct string country_code;
    struct string indicator_name;
    struct string indicator_code;

    float dates[DATE_END - DATE_START + 1];
};

struct context {
    itj_csv_umax entries_max, entries_used;
    struct entry *entries;
} *g_ctx = NULL;

itj_csv_bool string_alloc(struct string *out, char *base, itj_csv_umax len) {
    if (len == 0) {
        out->len = 0;
        out->base = NULL;
    }
    out->len = len;
    out->base = (char *)calloc(1, len + 1);
    if (!out->base) {
        return ITJ_CSV_FALSE;
    }

    memcpy(out->base, base, len);
    out->base[len] = '\0';

    return ITJ_CSV_TRUE;
}

int main(int argc, char *argv[]) {
    g_ctx = (struct context *)calloc(1, sizeof(*g_ctx));
    if (!g_ctx) {
        printf("Unable to allocate memory\n");
        return EXIT_FAILURE;
    }

    g_ctx->entries_max = 1024;
    g_ctx->entries_used = 0;
    g_ctx->entries = (struct entry *)calloc(1, g_ctx->entries_max * sizeof(*g_ctx->entries));

    const char *exe_path = argv[0];
    if (!exe_path) {
        printf("Unable to run because argv[0] does not point to the path to the executable\n");
        return EXIT_FAILURE;
    }

    itj_csv_umax exe_dir_len = strlen(exe_path);
    if (exe_dir_len == 0) {
        printf("Unable to run because argv[0] points to a zero length string\n");
        return EXIT_FAILURE;
    }

    {
        itj_csv_smax i;
        for (i = exe_dir_len - 1; i >= 0; i--) {
            if (exe_path[i] == '\\' || exe_path[i] == '/') {
                break;
            }
        }

        if (i <= 0) {
            exe_dir_len = 0;
        } else {
            exe_dir_len = i;
        }
    }

    char *exe_dir = NULL;
    if (exe_dir_len > 0) {
        exe_dir = (char *)calloc(1, exe_dir_len + SIZE_OF_NULL_TERMINATOR);
        if (!exe_dir) {
            printf("Unable to allocate memory for the executable directory path. The executable string is '%s'\n", exe_path);
            return EXIT_FAILURE;
        }

        memcpy(exe_dir, exe_path, exe_dir_len);
        exe_dir[exe_dir_len] = '\0';
    }

    const char *csv_path = "API_EG.ELC.ACCS.ZS_DS2_en_csv_v2_5995100.csv";
    itj_csv_umax csv_path_len = strlen(csv_path);
    if (exe_dir_len > 0) {
        char *path = (char *)calloc(1, exe_dir_len + SIZE_OF_DIR_SEPERATOR + csv_path_len + SIZE_OF_NULL_TERMINATOR);
        if (!path) {
            printf("Unable to allocate memory for the example csv path, '%s'\n", csv_path);
            return EXIT_FAILURE;
        }

        csv_path_len = snprintf(path, exe_dir_len + SIZE_OF_DIR_SEPERATOR + csv_path_len + SIZE_OF_NULL_TERMINATOR, "%s" DIR_SEPERATOR "%s", exe_dir, csv_path);
        csv_path = path;
    }

    itj_csv_umax buffer_max = KB(16);
    void *buffer = calloc(1, buffer_max);
    if (!buffer) {
        printf("Unable to allocate memory for parsing buffer\n");
        return EXIT_FAILURE;
    }

    struct itj_csv csv;
    if (!itj_csv_open(&csv, csv_path, csv_path_len, buffer, buffer_max, ITJ_CSV_DELIM_COMMA, NULL)) {
        printf("Failed to initalize itj_csv struct to file, '%s'\n", csv_path);
        return EXIT_FAILURE;
    }

    itj_csv_umax pump_ret = itj_csv_pump_stdio(&csv);
    if (pump_ret == 0) {
        printf("Unable to read '%s'\n", csv_path);
        return EXIT_FAILURE;
    }


    // The CSV file, downloaded from the World Bank website, is non-standard, so we move around in the file first
    struct itj_csv_value value;
    value = itj_csv_get_next_value(&csv); // Skip Data Source
    value = itj_csv_get_next_value(&csv); // Skip Data Source Value
    itj_csv_ignore_newlines(&csv);
    value = itj_csv_get_next_value(&csv); // Skip Last Updated Date
    value = itj_csv_get_next_value(&csv); // Skip Last Updated Date Value
    itj_csv_ignore_newlines(&csv);

    itj_csv_u32 column_offset = 4;
    itj_csv_s32 num_columns = -1;
    for (;;) {
        value = itj_csv_get_next_value(&csv);
        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                return EXIT_FAILURE;
            } else {
                continue;
            }
        }

        if (value.is_end_of_line) {
            num_columns = (value.idx + 1) - column_offset;
            break;
        }
    }
    struct entry ent = {0};
    for (;;) {
        value = itj_csv_get_next_value_avx2(&csv);
        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                break;
            } else {
                continue;
            }
        }

        itj_csv_umax column = (value.idx - column_offset) % num_columns;


        if (column == 0) {
            if (!string_alloc(&ent.country_name, value.data.base, value.data.len)) {
                printf("Unable to allocate memory for string\n");
                return EXIT_FAILURE;
            }
        } else if (column == 1) {
            if (!string_alloc(&ent.country_code, value.data.base, value.data.len)) {
                printf("Unable to allocate memory for string\n");
                return EXIT_FAILURE;
            }
        } else if (column == 2) {
            if (!string_alloc(&ent.indicator_name, value.data.base, value.data.len)) {
                printf("Unable to allocate memory for string\n");
                return EXIT_FAILURE;
            }
        } else if (column == 3) {
            if (!string_alloc(&ent.indicator_code, value.data.base, value.data.len)) {
                printf("Unable to allocate memory for string\n");
                return EXIT_FAILURE;
            }
        } else if (column > 3 && column <= 4 + (DATE_END - DATE_START)) {
            float val = NAN;
            if (value.data.len > 0) {
                char *start = (char *)value.data.base;
                char *expected_end = start + value.data.len;
                char *end;

                val = strtof(start, &end);
            }

            itj_csv_umax idx = (column - 4);
            ent.dates[idx] = val;
        }

        if (value.is_end_of_line) {
            if (g_ctx->entries_used == g_ctx->entries_max) {
                g_ctx->entries_max += 512;
                void *new_base = realloc(g_ctx->entries, g_ctx->entries_max * sizeof(*g_ctx->entries));
                if (!new_base) {
                    printf("Unable to allocate memory for entire csv file in csv parsing loop\n");
                    return EXIT_FAILURE;
                }

                g_ctx->entries = (struct entry *)new_base;
            }

            g_ctx->entries[g_ctx->entries_used] = ent;
            ++g_ctx->entries_used;

            memset(&ent, 0, sizeof(ent));
        }
    }

    itj_csv_close_fh(&csv);

    char buf[128];
    for (;;) {
        printf("Type the Country Code to get its statistics: ");
        scanf("%s", buf);
        printf("\n");

        for (itj_csv_umax i = 0; buf[i]; ++i) {
            buf[i] = toupper(buf[i]);
        }

        if (strcmp(buf, "QUIT") == 0) {
            return EXIT_SUCCESS;
        }

        itj_csv_umax len = strlen(buf);
        itj_csv_bool is_first = ITJ_CSV_TRUE;
        for (itj_csv_umax i = 0; i < g_ctx->entries_used; ++i) {
            struct entry ent = g_ctx->entries[i];
            struct string country_code = ent.country_code;

            if (country_code.len == len) {
                if (memcmp(country_code.base, buf, len) == 0) {
                    printf("Country: %s\n", ent.country_name.base);
                    for (itj_csv_umax j = 0; j < DATE_END - DATE_START; ++j) {
                        itj_csv_umax date = DATE_START + j;
                        float val = ent.dates[j];
                        if (!isnan(val) && is_first) {
                            is_first = ITJ_CSV_FALSE;
                            printf("No data available from 1960 to %lld\n", date);
                        }

                        if (!isnan(val)) {
                            printf("%lld: %f\n", date, val);
                        }
                    }
                }
            }
        }

        printf("\n");
    }


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
