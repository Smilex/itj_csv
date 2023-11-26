/*
 * LICENSE available at the bottom
 */

#ifndef ITJ_CSV_NO_STD
#include <stdlib.h>
#include <stdio.h>
#endif

#if !(ITJ_CSV_32BIT || ITJ_CSV_64BIT)
#if defined(__x86_64__) || defined(_M_X64) || defined(__ppc64__)
#define ITJ_CSV_64BIT
#else
#define ITJ_CSV_32BIT
#endif
#endif

typedef unsigned char itj_csv_u8;
typedef unsigned int itj_csv_u32;
typedef int itj_csv_s32;
typedef itj_csv_u8 itj_csv_bool;
#ifdef ITJ_CSV_64BIT
typedef long long itj_csv_s64;
typedef unsigned long long itj_csv_u64;
typedef itj_csv_s64 itj_csv_smax;
typedef itj_csv_u64 itj_csv_umax;
#else
typedef itj_csv_s32 itj_csv_smax;
typedef itj_csv_u32 itj_csv_umax;
#endif

#define ITJ_CSV_TRUE 1
#define ITJ_CSV_FALSE 0

#define ITJ_CSV_FAILED 0

#ifndef ITJ_CSV_NO_STD
#define ITJ_CSV_ALLOC(user_ptr, num_bytes) calloc(1, num_bytes)
#define ITJ_CSV_FREE(user_ptr, ptr) free(ptr)
#endif

#ifndef ITJ_CSV_MEMCPY
#include <string.h>
#define ITJ_CSV_MEMCPY(dst, src, size) memcpy(dst, src, size)
#endif

#define ITJ_CSV_DELIM_COMMA ','
#define ITJ_CSV_DELIM_COLON ';'

typedef enum itj_csv_state_name {
    ITJ_CSV_STATE_NORMAL,
    ITJ_CSV_STATE_NEWLINE,
    ITJ_CSV_STATE_EOF,
} itj_csv_state_name_t;

typedef struct itj_csv_string {
    itj_csv_u8 *base;
    itj_csv_u32 len;
} itj_csv_string_t;

typedef struct itj_csv_value {
    struct itj_csv_string data;
    itj_csv_bool is_end_of_line;
    itj_csv_bool need_data;
    itj_csv_umax idx;
} itj_csv_value_t;

struct itj_csv_profiler {
    __int64 num_iter;
    __int64 main_loop_avg;
    __int64 was_in_quotes_avg;
    __int64 got_doubles_avg;
};

typedef struct itj_csv {
    struct itj_csv_profiler profiler;
    itj_csv_u8 delimiter;
    itj_csv_umax read_iter;
    itj_csv_umax prev_read_iter;
    itj_csv_u8 *read_base;
    itj_csv_umax read_max;
    itj_csv_umax read_used;
    itj_csv_umax idx;
    void *user_mem_ptr;
#ifndef ITJ_CSV_NO_STD
    FILE *fh;
#endif
} itj_csv_t;

void itj_csv_ignore_newlines(struct itj_csv *csv) {
    itj_csv_umax i;
    itj_csv_umax max = csv->read_used;
    for (i = csv->read_iter; i < max; ++i) {
        itj_csv_u8 ch = csv->read_base[i];

        if (ch == '\0') {
            break;
        }

        if (ch == '\r') {
            if (i + 1 < max) {
                itj_csv_u8 ch_next = csv->read_base[i + 1];
                if (ch_next == '\n') {
                    i = i + 1;
                }
            }
        } else if (ch == '\n') {
            i = i + 1;
        } else {
            break;
        }
    }

    csv->read_iter = i;
}

itj_csv_umax itj_csv_contract_double_quotes(itj_csv_u8 *start, itj_csv_umax max) {
    itj_csv_umax new_len = max;
    for (itj_csv_umax i = 0; i < new_len; ++i) {
        itj_csv_u8 ch = start[i];

        if (ch == '\"') {
            for (itj_csv_umax j = i + 1; j < max - 1; ++j) {
                start[j] = start[j + 1];
            }
            --new_len;
        }
    }

    return new_len;
}

struct itj_csv_value itj_csv_parse_quotes(struct itj_csv *csv, itj_csv_umax i) {
    csv->prev_read_iter = csv->read_iter;
    struct itj_csv_value rv;
    i += 1; // Expecting " to be the first character
    rv.data.base = &csv->read_base[i];
    rv.data.len = 0;
    rv.is_end_of_line = ITJ_CSV_FALSE;
    rv.need_data = ITJ_CSV_FALSE;
    rv.idx = csv->idx;

    itj_csv_bool got_doubles = ITJ_CSV_FALSE;
    itj_csv_umax max = csv->read_used;
    for (; i < max; ++i) {
        itj_csv_u8 ch = csv->read_base[i];
        if (ch == '"') {
            if (i + 1 == max) {
                rv.need_data = ITJ_CSV_TRUE;
                return rv;
            }
            itj_csv_u8 ch_next = csv->read_base[i + 1];
            if (ch_next == '"') {
                got_doubles = ITJ_CSV_TRUE;
                rv.data.len += 2;
                i += 1;
            } else {
                i += 1;
                break;
            }
        } else {
            rv.data.len += 1;
        }
    }

    if (i >= max) {
        rv.need_data = ITJ_CSV_TRUE;
        return rv;
    }

    for (; i < max; ++i) {
        itj_csv_u8 ch = csv->read_base[i];
        if (ch == '\n') {
            rv.is_end_of_line = ITJ_CSV_TRUE;
            i += 1;
            break;
        }
        if (ch == csv->delimiter) {
            ++i;
            break;
        }
    }

    if (i >= max) {
        // This is a bad case. Because we know the next token, but have to revert to be consistent
        rv.need_data = ITJ_CSV_TRUE;
        return rv;
    }

    if (got_doubles) {
        rv.data.len = itj_csv_contract_double_quotes(rv.data.base, rv.data.len);
    }

    csv->read_iter = i;
    return rv;
}

struct itj_csv_value itj_csv_parse_value(struct itj_csv *csv, itj_csv_umax i) {
    csv->prev_read_iter = csv->read_iter;
    struct itj_csv_value rv;
    rv.data.base = &csv->read_base[i];
    rv.data.len = 0;
    rv.is_end_of_line = ITJ_CSV_FALSE;
    rv.need_data = ITJ_CSV_FALSE;
    rv.idx = csv->idx;

    itj_csv_umax max = csv->read_used;
    for (; i < max; ++i) {
        itj_csv_u8 ch = csv->read_base[i];
        if (ch == '"') {
            return itj_csv_parse_quotes(csv, i);
        } else if (ch == csv->delimiter) {
            i += 1;
            csv->read_iter = i;
            return rv;
        } else if (ch == '\r') {
            if (i + 1 == max) {
                rv.need_data = ITJ_CSV_TRUE;
                return rv;
            }
            itj_csv_u8 ch_next = csv->read_base[i + 1];
            if (ch_next == '\n') {
                i += 2;
                rv.is_end_of_line = ITJ_CSV_TRUE;
                csv->read_iter = i;
                return rv;
            } else {
                rv.data.len += 1;
            }
        } else if (ch == '\n') {
            rv.is_end_of_line = ITJ_CSV_TRUE;
            i += 1;
            csv->read_iter = i;
            return rv;
        } else {
            rv.data.len += 1;
        }
    }

    rv.need_data = ITJ_CSV_TRUE;
    return rv;
}

struct itj_csv_value itj_csv_get_next_value(struct itj_csv *csv) {
    struct itj_csv_value rv = itj_csv_parse_value(csv, csv->read_iter);

    if (!rv.need_data) {
        csv->idx += 1;
    }

    return rv;
}

#ifndef ITJ_CSV_NO_STD

void itj_csv_close_fh(struct itj_csv *csv) {
    if (csv->fh) {
        fclose(csv->fh);
    }
}

itj_csv_umax itj_csv_pump_stdio(struct itj_csv *csv) {
    itj_csv_umax total_read = 0;
    itj_csv_umax diff = 0;
    itj_csv_smax ret;

    if (csv->read_iter != 0) {
        if (csv->read_iter == csv->prev_read_iter) {
            if (csv->read_used > csv->read_iter) {
                diff = csv->read_used - csv->read_iter;
                if (diff <= csv->read_max) {
                    ITJ_CSV_MEMCPY(csv->read_base, csv->read_base + csv->read_iter, diff);
                } else {
                    return 0;
                }
            }
        }
    }

    csv->read_iter = 0;
    csv->prev_read_iter = 0;

    do {
        ret = fread(csv->read_base + diff + total_read, 1, csv->read_max - diff - total_read, csv->fh);
        if (ret < 0) {
            return 0;
        }

        total_read += ret;
    } while (ret != 0 && total_read < csv->read_max - diff);

    csv->read_used = total_read + diff;

    return total_read;
}

itj_csv_bool itj_csv_open_fp(struct itj_csv *csv_out, FILE *fh, void *mem_buf, itj_csv_umax mem_buf_size, itj_csv_u8 delimiter, void *user_mem_ptr) {
    csv_out->read_iter = 0;
    csv_out->read_used = 0;
    csv_out->read_base = (itj_csv_u8 *)mem_buf;
    csv_out->read_max = mem_buf_size;
    csv_out->delimiter = delimiter;
    csv_out->fh = fh;
    csv_out->user_mem_ptr = user_mem_ptr;
    csv_out->idx = 0;

    csv_out->profiler.main_loop_avg = 0;
    csv_out->profiler.was_in_quotes_avg = 0;
    csv_out->profiler.got_doubles_avg = 0;
    csv_out->profiler.num_iter = 0;

    return ITJ_CSV_TRUE;
}

itj_csv_bool itj_csv_open(struct itj_csv *csv_out, const char *filepath, itj_csv_u32 filepath_len, void *mem_buf, itj_csv_umax mem_buf_size, itj_csv_u8 delimiter, void *user_mem_ptr) {
    char *null_terminated = ITJ_CSV_ALLOC(user_mem_ptr, filepath_len + 1);

    ITJ_CSV_MEMCPY(null_terminated, filepath, filepath_len);
    null_terminated[filepath_len] = '\0';

    FILE *fh = fopen(null_terminated, "rb");
    if (!fh) {
        return ITJ_CSV_FALSE;
    }

    itj_csv_bool rv = itj_csv_open_fp(csv_out, fh, mem_buf, mem_buf_size, delimiter, user_mem_ptr);

    return rv;
}



#endif // ITJ_CSV_NO_STD
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
