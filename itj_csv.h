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

typedef struct itj_csv_value {
    itj_csv_u8 *base;
    itj_csv_u32 len;
    itj_csv_bool is_end_of_line;
    itj_csv_bool is_end_of_data;
    itj_csv_bool need_data;
    itj_csv_umax idx;
} itj_csv_value_t;

typedef struct itj_csv_header {
    itj_csv_u32 num_columns;
    itj_csv_u32 max;
    itj_csv_u8 *base;
    itj_csv_u8 **names;
    itj_csv_u32 *lens;
} itj_csv_header_t;

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

itj_csv_bool itj_csv_parse_header(struct itj_csv *csv, struct itj_csv_header *header) {
    header->num_columns = 0;

    itj_csv_bool in_quotes = ITJ_CSV_FALSE;
    itj_csv_u8 delim = csv->delimiter;

    itj_csv_umax i = csv->read_iter;
    itj_csv_umax max = csv->read_used;
    for (; i < max; ++i) {
        itj_csv_u8 ch = csv->read_base[i];

        if (ch == '\0') {
            break;
        }

        if (in_quotes) {
            if (ch == '"') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next != '"') {
                        in_quotes = ITJ_CSV_FALSE;
                    } else {
                        i += 1;
                    }
                } else {
                    in_quotes = ITJ_CSV_FALSE;
                }
            }
        } else {
            if (ch == delim) {
                ++header->num_columns;
            } else if (ch == '"') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next != '"') {
                        in_quotes = ITJ_CSV_TRUE;
                    } else {
                        i += 1;
                    }
                } else {
                    in_quotes = ITJ_CSV_TRUE;
                }
            } else if (ch == '\r') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next == '\n') {
                        break;
                    }
                }
            } else if (ch == '\n') {
                break;
            }
        }
    }

    ++header->num_columns;

    header->max = i;
    header->base = (itj_csv_u8 *)ITJ_CSV_ALLOC(header->user_mem_ptr, header->max);
    if (!header->base) {
        return ITJ_CSV_FALSE;
    }


    header->names = (itj_csv_u8 **)ITJ_CSV_ALLOC(header->user_mem_ptr, header->num_columns * sizeof(*header->names));
    if (!header->names) {
        return ITJ_CSV_FALSE;
    }
    header->lens = (itj_csv_u32 *)ITJ_CSV_ALLOC(header->user_mem_ptr, header->num_columns * sizeof(*header->lens));
    if (!header->lens) {
        return ITJ_CSV_FALSE;
    }

    itj_csv_bool got_doubles = ITJ_CSV_FALSE;

    itj_csv_u32 column_iter = 0;
    itj_csv_u32 len = 0;

    i = csv->read_iter;
    ITJ_CSV_MEMCPY(header->base, &csv->read_base[i], header->max);
    itj_csv_u8 *ptr = header->base;
    max = csv->read_used;
    for (; i < max; ++i) {
        itj_csv_u8 ch = csv->read_base[i];

        if (ch == '\0') {
            break;
        }

        if (in_quotes) {
            if (ch == '"') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next != '"') {
                        in_quotes = ITJ_CSV_FALSE;
                    } else {
                        i += 1;
                        len += 2;
                        got_doubles = ITJ_CSV_TRUE;
                    }
                } else {
                    in_quotes = ITJ_CSV_FALSE;
                }
            } else {
                len++;
            }
        } else {
            if (ch == delim) {
                if (got_doubles) {
                    len = itj_csv_contract_double_quotes(ptr, len);
                    got_doubles = ITJ_CSV_FALSE;
                }
                header->names[column_iter] = ptr;
                header->lens[column_iter] = len;
                column_iter++;

                if (column_iter > header->num_columns) {
                    break;
                }

                len = 0;
                ptr = &header->base[i + 1];
            } else if (ch == '"') {
                in_quotes = ITJ_CSV_TRUE;
                len = 0;
                ptr = &header->base[i + 1];
            } else if (ch == '\r') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next == '\n') {
                        i = i + 2;
                        if (got_doubles) {
                            len = itj_csv_contract_double_quotes(ptr, len);
                            got_doubles = ITJ_CSV_FALSE;
                        }
                        header->names[column_iter] = ptr;
                        header->lens[column_iter] = len;
                        break;
                    }
                }
            } else if (ch == '\n') {
                i = i + 1;
                if (got_doubles) {
                    len = itj_csv_contract_double_quotes(ptr, len);
                    got_doubles = ITJ_CSV_FALSE;
                }
                header->names[column_iter] = ptr;
                header->lens[column_iter] = len;
                break;
            } else {
                len++;
            }
        }
    }

    csv->read_iter = i;

    return ITJ_CSV_TRUE;
}



struct itj_csv_value itj_csv_get_next_value(struct itj_csv *csv) {
    csv->profiler.num_iter += 1;

    csv->prev_read_iter = csv->read_iter;

    struct itj_csv_value rv;
    rv.base = NULL;
    rv.len = 0;
    rv.is_end_of_line = ITJ_CSV_FALSE;
    rv.is_end_of_data = ITJ_CSV_FALSE;
    rv.need_data = ITJ_CSV_FALSE;
    rv.idx = csv->idx;

    itj_csv_u8 delim = csv->delimiter;
    itj_csv_bool was_in_quotes = ITJ_CSV_FALSE;
    itj_csv_bool in_quotes = ITJ_CSV_FALSE;
    itj_csv_u32 len = 0;
    itj_csv_umax i = csv->read_iter;
    itj_csv_u8 *ptr = &csv->read_base[i];
    itj_csv_bool got_doubles = ITJ_CSV_FALSE;

    __int64 cycles_main_loop_start = __rdtsc();

    itj_csv_umax max = csv->read_used;
    for (; i < max; ++i) {
        itj_csv_u8 ch = csv->read_base[i];

        if (in_quotes) {
            if (ch == '"') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next != '"') {
                        in_quotes = ITJ_CSV_FALSE;
                        was_in_quotes = ITJ_CSV_TRUE;
                    } else {
                        i += 1;
                        len += 2;
                        got_doubles = ITJ_CSV_TRUE;
                    }
                } else {
                    in_quotes = ITJ_CSV_FALSE;
                    was_in_quotes = ITJ_CSV_TRUE;
                }
            } else {
                len++;
            }
        } else {
            if (ch == delim) {
                if (!was_in_quotes) {
                    i += 1;
                }
                break;
            } else if (ch == '"') {
                in_quotes = ITJ_CSV_TRUE;
                ptr = &csv->read_base[i + 1];
                len = 0;
            } else if (ch == '\r') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next == '\n') {
                        i = i + 2;
                        rv.is_end_of_line = ITJ_CSV_TRUE;
                        break;
                    }
                }
            } else if (ch == '\n') {
                i = i + 1;
                rv.is_end_of_line = ITJ_CSV_TRUE;
                break;
            } else if (!was_in_quotes) {
                len++;
            }
        }
    }

    __int64 cycles_main_loop_end = __rdtsc();
    __int64 cycles_main_loop_diff = cycles_main_loop_end - cycles_main_loop_start;

    csv->profiler.main_loop_avg += (cycles_main_loop_diff - csv->profiler.main_loop_avg) / csv->profiler.num_iter;


    rv.base = ptr;
    rv.len = len;


    if (was_in_quotes) {
        __int64 cycles_was_in_quotes_start = __rdtsc();
        for (; i < max; ++i) {
            itj_csv_u8 ch = csv->read_base[i];
            if (ch == '\0') {
                break;
            }
            if (ch == '\n') {
                rv.is_end_of_line = ITJ_CSV_TRUE;
                ++i;
                break;
            }
            if (ch == delim) {
                ++i;
                break;
            }
        }

        __int64 cycles_was_in_quotes_end = __rdtsc();
        __int64 cycles_was_in_quotes_diff = cycles_was_in_quotes_end - cycles_was_in_quotes_start;
        csv->profiler.was_in_quotes_avg += (cycles_was_in_quotes_diff - csv->profiler.was_in_quotes_avg) / csv->profiler.num_iter;
    }

    if (i == max) {
        rv.need_data = ITJ_CSV_TRUE;
        return rv;
    }


    csv->idx += 1;
    csv->read_iter = i;

    if (got_doubles) {
        __int64 cycles_got_doubles_start = __rdtsc();
        rv.len = itj_csv_contract_double_quotes(rv.base, rv.len);
        __int64 cycles_got_doubles_end = __rdtsc();
        __int64 cycles_got_doubles_diff = cycles_got_doubles_end - cycles_got_doubles_start;

        csv->profiler.got_doubles_avg += (cycles_got_doubles_diff - csv->profiler.got_doubles_avg) / csv->profiler.num_iter;
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
                if (diff < csv->read_max) {
                    ITJ_CSV_MEMCPY(csv->read_base, csv->read_base + csv->read_iter, diff);
                }
            }
        }
    }

    csv->read_iter = 0;

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
