// Maybe reviewing code and writing reviewable (as in, presentable) code is beneficial for improvment. Consider organising on discor
//
//
// Do I want a function to skip a line?
//
// Do I have to check for \r\n and not just \n?

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
typedef long itj_csv_s64;
typedef unsigned long itj_csv_u64;
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

enum itj_csv_state_name {
    ITJ_CSV_STATE_NORMAL,
    ITJ_CSV_STATE_NEWLINE,
    ITJ_CSV_STATE_EOF,
};

struct itj_csv_value {
    itj_csv_u8 *base;
    itj_csv_u32 len;
    itj_csv_bool is_end_of_line;
    itj_csv_bool is_end_of_data;
    itj_csv_bool need_data;
    itj_csv_umax idx;
};

struct itj_csv_header {
    itj_csv_u32 num_columns;
    itj_csv_u32 header_max;
    itj_csv_u8 *header_base;
    itj_csv_u8 **header_names;
    itj_csv_u32 *header_lens;
};

struct itj_csv {
    itj_csv_u8 delimiter;
    itj_csv_u32 column_iter;
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
};

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

    header->header_max = i;
    header->header_base = (itj_csv_u8 *)ITJ_CSV_ALLOC(header->user_mem_ptr, header->header_max);
    if (!header->header_base) {
        return ITJ_CSV_FALSE;
    }


    header->header_names = (itj_csv_u8 **)ITJ_CSV_ALLOC(header->user_mem_ptr, header->num_columns * sizeof(*header->header_names));
    if (!header->header_names) {
        return ITJ_CSV_FALSE;
    }
    header->header_lens = (itj_csv_u32 *)ITJ_CSV_ALLOC(header->user_mem_ptr, header->num_columns * sizeof(*header->header_lens));
    if (!header->header_lens) {
        return ITJ_CSV_FALSE;
    }

    itj_csv_u32 column_iter = 0;
    itj_csv_u32 len = 0;
    itj_csv_u8 *ptr = csv->read_base;

    i = csv->read_iter;
    ITJ_CSV_MEMCPY(header->header_base, &csv->read_base[i], header->header_max);
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
                    }
                } else {
                    in_quotes = ITJ_CSV_FALSE;
                }
            } else {
                len++;
            }
        } else {
            if (ch == delim) {
                header->header_names[column_iter] = ptr;
                header->header_lens[column_iter] = len;
                column_iter++;

                if (column_iter > header->num_columns) {
                    break;
                }

                len = 0;
                ptr = &csv->read_base[i + 1];
            } else if (ch == '"') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next != '"') {
                        in_quotes = ITJ_CSV_TRUE;
                        len = 0;
                        ptr = &csv->read_base[i + 1];
                    } else {
                        i += 1;
                    }
                } else {
                    in_quotes = ITJ_CSV_TRUE;
                    len = 0;
                    ptr = &csv->read_base[i + 1];
                }
            } else if (ch == '\r') {
                if (i + 1 < max) {
                    itj_csv_u8 ch_next = csv->read_base[i + 1];
                    if (ch_next == '\n') {
                        i = i + 2;
                        header->header_names[column_iter] = ptr;
                        header->header_lens[column_iter] = len;
                        break;
                    }
                }
            } else if (ch == '\n') {
                i = i + 1;
                header->header_names[column_iter] = ptr;
                header->header_lens[column_iter] = len;
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
    itj_csv_u8 *ptr = csv->read_base;

    itj_csv_umax i = csv->read_iter;

    itj_csv_umax max = csv->read_used;
    for (; i < max; ++i) {
        itj_csv_u8 ch = csv->read_base[i];

        if (ch == '\0') {
            rv.is_end_of_data = ITJ_CSV_TRUE;
            return rv;
        }

        if (in_quotes) {
            if (ch == '"') {
                in_quotes = ITJ_CSV_FALSE;
                was_in_quotes = ITJ_CSV_TRUE;
            } else {
                len++;
            }
        } else {
            if (ch == delim) {
                i += 1;
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
                        csv->read_iter = i;
                        return rv;
                    }
                }
            } else if (ch == '\n') {
                i = i + 1;
                rv.is_end_of_line = ITJ_CSV_TRUE;
                csv->read_iter = i;
                return rv;
            } else if (!was_in_quotes) {
                len++;
            }
        }
    }

    if (i == max) {
        rv.need_data = ITJ_CSV_TRUE;
        return rv;
    }

    csv->idx += 1;
    csv->read_iter = i;

    rv.base = ptr;
    rv.len = len;

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
    itj_csv_smax ret;

    if (csv->read_iter != 0) {
        if (csv->read_iter == csv->prev_read_iter) {
            if (csv->read_used > csv->read_iter) {
                itj_csv_umax diff = csv->read_used - csv->read_iter;
                if (diff < csv->read_max) {
                    ITJ_CSV_MEMCPY(csv->read_base, csv->read_base + csv->read_iter, diff);
                    total_read = diff;
                }
            }
        }
    }

    csv->read_iter = 0;

    do {
        ret = fread(csv->read_base + total_read, 1, csv->read_max - total_read, csv->fh);
        if (ret < 0) {
            return 0;
        }

        total_read += ret;
    } while (ret != 0 && total_read < csv->read_max);

    csv->read_used = total_read;
    
    return total_read;
}

itj_csv_bool itj_csv_open_fp(struct itj_csv *csv_out, FILE *fh, void *mem_buf, itj_csv_umax mem_buf_size, itj_csv_u8 delimiter, void *user_mem_ptr) {
    csv_out->read_iter = 0;
    csv_out->read_base = (itj_csv_u8 *)mem_buf;
    csv_out->read_max = mem_buf_size;
    csv_out->delimiter = delimiter;
    csv_out->fh = fh;
    csv_out->user_mem_ptr = user_mem_ptr;
    csv_out->idx = 0;

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
