/*
 * LICENSE available at the bottom
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <immintrin.h>

#include <windows.h>

#define ITJ_CSV_IMPLEMENTATION
#include "itj_csv.h"

#define KB(x) (x * 1024)
#define SIZE_OF_NULL_TERMINATOR 1
#define SIZE_OF_DIR_SEPERATOR 1
#define NUM_BUFFERED_CYCLES (10 * 1000 * 1000)

#ifdef IS_WINDOWS
#define DIR_SEPERATOR "\\"
#else
#define DIR_SEPERATOR "/"
#endif

itj_csv_bool compare_strings(char *base1, itj_csv_umax len1, char *base2, itj_csv_umax len2) {
    if (len1 == len2) {
        if (memcmp(base1, base2, len1) == 0) {
            return ITJ_CSV_TRUE;
        }
    }

    return ITJ_CSV_FALSE;
}

itj_csv_bool g_did_a_test_fail = ITJ_CSV_FALSE;
char *g_sitrep_filepath;

void sitrep(char *fmt, ...) {
    char *buf = (char *)calloc(1, KB(512));
    if (!buf) {
        printf("Unable to allocate memory for sitrep\n");
        return;
    }

    va_list va;
    va_start(va, fmt);
    itj_csv_umax buf_len = vsnprintf(buf, KB(512), fmt, va);
    va_end(va);

    printf("%s", buf);

    FILE *fh = fopen(g_sitrep_filepath, "ab");
    if (fh) {
        fwrite(buf, 1, buf_len, fh);
        fclose(fh);
    }

    free(buf);
}

void test_print(char *str) {
    sitrep("%s... ", str);
}

void test_print_result(itj_csv_bool result) {
    if (result == ITJ_CSV_TRUE) {
        sitrep("OK\n");
    } else {
        sitrep("FAIL\n");
        g_did_a_test_fail = ITJ_CSV_TRUE;
    }
}

void print_total(itj_csv_umax bytes_read, double multiplier, __int64 cycles_start, __int64 cycles_end) {
    __int64 cycles_diff = cycles_end - cycles_start;
    double time_diff_ns = multiplier * cycles_diff;
    double time_diff_ms = time_diff_ns / 100000.0;
    double time_diff_sec = time_diff_ms / 1000.0;

    sitrep("It took %lld cycles to complete\n", cycles_diff);
    sitrep("Read %llu total bytes. %.3f MB per second\n", bytes_read, ((double)bytes_read / time_diff_sec) / (1024.0 * 1024.0));

}

int main(int argc, char *argv[]) {
    g_sitrep_filepath = (char *)calloc(1, KB(10));
    if (!g_sitrep_filepath) {
        printf("Unable to allocate memory for sitrep filepath\n");
        return EXIT_FAILURE;
    }
    snprintf(g_sitrep_filepath, KB(10), "itj_csv_test_%d.log", time(NULL));

    __int64 *buffered_cycles = (__int64 *)calloc(1, NUM_BUFFERED_CYCLES * sizeof(*buffered_cycles));
    if (!buffered_cycles) {
        printf("Unable to allocate memory for buffered cycles. It might work to lower NUM_BUFFERED_CYCLES\n");
        return EXIT_FAILURE;
    }

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

    const char *correctness_with_header_csv_path = "correctness_with_header.csv";
    itj_csv_umax correctness_with_header_csv_path_len = strlen(correctness_with_header_csv_path);
    if (exe_dir_len > 0) {
        char *path = (char *)calloc(1, exe_dir_len + correctness_with_header_csv_path_len + SIZE_OF_NULL_TERMINATOR);
        if (!path) {
            printf("Unable to allocate memory for the example csv path, '%s'\n", correctness_with_header_csv_path);
            return EXIT_FAILURE;
        }

        correctness_with_header_csv_path_len = snprintf(path, exe_dir_len + SIZE_OF_DIR_SEPERATOR + correctness_with_header_csv_path_len + SIZE_OF_NULL_TERMINATOR, "%s" DIR_SEPERATOR "%s", exe_dir, correctness_with_header_csv_path);
        correctness_with_header_csv_path = path;
    }

    const char *generated_csv_path = "generated.csv";
    itj_csv_umax generated_csv_path_len = strlen(generated_csv_path);
    if (exe_dir_len > 0) {
        char *path = (char *)calloc(1, exe_dir_len + generated_csv_path_len + SIZE_OF_NULL_TERMINATOR);
        if (!path) {
            printf("Unable to allocate memory for the generated csv path, '%s'\n", generated_csv_path);
            return EXIT_FAILURE;
        }

        generated_csv_path_len = snprintf(path, exe_dir_len + SIZE_OF_DIR_SEPERATOR + generated_csv_path_len + SIZE_OF_NULL_TERMINATOR, "%s" DIR_SEPERATOR "%s", exe_dir, generated_csv_path);
        generated_csv_path = path;
    }

    printf("Running itj_csv correctness tests\n");
    itj_csv_umax buffer_max = KB(512);
    void *buffer = calloc(1, buffer_max);
    if (!buffer) {
        printf("Unable to allocate memory for parsing buffer\n");
        return EXIT_FAILURE;
    }


    struct itj_csv csv;
    if (!itj_csv_open(&csv, correctness_with_header_csv_path, correctness_with_header_csv_path_len, buffer, buffer_max, ITJ_CSV_DELIM_COMMA, NULL)) {
        printf("Failed to initalize itj_csv struct to file, '%s'\n", correctness_with_header_csv_path);
        return EXIT_FAILURE;
    }

    itj_csv_umax pump_ret = itj_csv_pump_stdio(&csv);
    if (pump_ret == 0) {
        printf("Unable to read from '%s'\n", correctness_with_header_csv_path);
        return EXIT_FAILURE;
    }

    struct itj_csv_header header;
    if (!itj_csv_parse_header(&csv, &header)) {
        printf("Unable to parse header from '%s'\n", correctness_with_header_csv_path);
        return EXIT_FAILURE;
    }

    test_print("Expecting 5 columns in header");
    test_print_result(header.num_columns == 5);

    test_print("Is header[0] == column1");
    test_print_result(compare_strings(header.names[0], header.lens[0], "column1", 7));

    test_print("Is header[1] == column2");
    test_print_result(compare_strings(header.names[1], header.lens[1], "column2", 7));

    test_print("Is header[2] == column\\r\\n3");
    test_print_result(compare_strings(header.names[2], header.lens[2], "column\r\n3", 9));

    test_print("Is header[3] == column\"4");
    test_print_result(compare_strings(header.names[3], header.lens[3], "column\"4", 8));

    test_print("Is header[4] == columnð");
    test_print_result(compare_strings(header.names[4], header.lens[4], "columnð", 8));

    itj_csv_bool end = ITJ_CSV_FALSE;
    itj_csv_u32 num_lines = 0;
    for (;;) {
        struct itj_csv_value value = itj_csv_get_next_value(&csv);
        itj_csv_umax column = value.idx % header.num_columns;
        if (value.is_end_of_data) {
            break;
        }
        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                end = ITJ_CSV_TRUE;
            } else {
                continue;
            }
        }


        if (column == 0) {
            if (num_lines == 0) {
                test_print("Is the first column of the first row == row1_column1");
                test_print_result(compare_strings(value.base, value.len, "row1_column1", 12));
            } else if (num_lines == 1) {
                test_print("Is the first column of the second row == row2_column1");
                test_print_result(compare_strings(value.base, value.len, "row2_column1", 12));
            }
        } else if (column == 1) {
            if (num_lines == 0) {
                test_print("Is the second column of the first row == row1_column2");
                test_print_result(compare_strings(value.base, value.len, "row1_column2", 12));
            } else if (num_lines == 1) {
                test_print("Is the second column of the second row == row2_column2");
                test_print_result(compare_strings(value.base, value.len, "row2_column2", 12));
            }
        } else if (column == 2) {
            if (num_lines == 0) {
                test_print("Is the third column of the first row == row1_column\\r\\n3");
                test_print_result(compare_strings(value.base, value.len, "row1_column\r\n3", 14));
            } else if (num_lines == 1) {
                test_print("Is the third column of the second row == row2_column\\r\\n3");
                test_print_result(compare_strings(value.base, value.len, "row2_column\r\n3", 14));
            }
        } else if (column == 3) {
            if (num_lines == 0) {
                test_print("Is the fourth column of the first row == row1_column\"4");
                test_print_result(compare_strings(value.base, value.len, "row1_column\"4", 13));
            } else if (num_lines == 1) {
                test_print("Is the fourth column of the second row == row2_column\"4");
                test_print_result(compare_strings(value.base, value.len, "row2_column\"4", 13));
            }
        } else if (column == 4) {
            if (num_lines == 0) {
                test_print("Is the fifth column of the first row == row1_columnð");
                test_print_result(compare_strings(value.base, value.len, "row1_columnð", 13));
            } else if (num_lines == 1) {
                test_print("Is the fifth column of the second row == row2_columnð");
                test_print_result(compare_strings(value.base, value.len, "row2_columnð", 13));
            }
        }

        if (value.is_end_of_line) {
            ++num_lines;
        }

        if (end) {
            break;
        }
    }

    itj_csv_close_fh(&csv);

    if (g_did_a_test_fail) {
        sitrep("NOT ALL CORRECTNESS TESTS COMPLETED SUCCESSFULLY!\n");
    } else {
        sitrep("ALL CORRECTNESS TESTS COMPLETED SUCCESSFULL\n");
    }

    sitrep("\nRunning itj_csv speed tests\n");

    sitrep("Reading generated csv file without any work as reference\n");

    FILE *fh = fopen(generated_csv_path, "rb");
    if (!fh) {
        printf("Unable to open %s for reference reading\n", generated_csv_path);
        return EXIT_FAILURE;
    }

    __int64 cycles_start_of_reference = __rdtsc();
    LARGE_INTEGER large_integer;
    QueryPerformanceCounter(&large_integer);
    itj_csv_umax time_start_of_reference = large_integer.QuadPart;


    itj_csv_umax bytes_read = 0;
    for (;;) {
        itj_csv_smax bytes = fread(buffer, 1, buffer_max, fh);
        if (bytes < 0) {
            printf("Unable to read all of %s\n", generated_csv_path);
            return EXIT_FAILURE;
        }

        if (bytes == 0) {
            break;
        }

        bytes_read += bytes;
    }
    __int64 cycles_end_of_reference = __rdtsc();
    QueryPerformanceCounter(&large_integer);
    itj_csv_umax time_end_of_reference = large_integer.QuadPart;

    fclose(fh);

    QueryPerformanceFrequency(&large_integer);
    itj_csv_umax freq = large_integer.QuadPart;

    __int64 cycles_freq = (cycles_end_of_reference - cycles_start_of_reference) * freq / (time_end_of_reference - time_start_of_reference);
    double multiplier = 1000000000.0 / (double)cycles_freq;

    print_total(bytes_read, multiplier, cycles_start_of_reference, cycles_end_of_reference);

    if (!itj_csv_open(&csv, generated_csv_path, generated_csv_path_len, buffer, buffer_max, ITJ_CSV_DELIM_COMMA, NULL)) {
        printf("Failed to initalize itj_csv struct to file, '%s'\n", correctness_with_header_csv_path);
        printf("Remember to generate a file for profiling first!\n");
        return EXIT_FAILURE;
    }

    bytes_read = 0;
    pump_ret = itj_csv_pump_stdio(&csv);
    if (pump_ret == 0) {
        printf("Unable to read from '%s'\n", generated_csv_path);
        return EXIT_FAILURE;
    }
    bytes_read += pump_ret;

    itj_csv_s32 num_columns = -1;
    num_lines = 0;
    __int64 num_values = 0;
    end = ITJ_CSV_FALSE;
    __int64 cycles_start_of_standard = __rdtsc();
    for (;;) {
        __int64 cycles_start_of_loop = __rdtsc();
        struct itj_csv_value value = itj_csv_get_next_value(&csv);
        __int64 cycles_after_next_value = __rdtsc();

#if 0
        itj_csv_umax column;
        if (num_columns == -1) {
            column = value.idx;
        } else {
            column = value.idx % num_columns;
        }
#endif

        if (value.is_end_of_data) {
            end = ITJ_CSV_TRUE;
        }
        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                end = ITJ_CSV_TRUE;
            } else {
                bytes_read += pump_ret;
            }
        }

        __int64 cycles_after_work = __rdtsc();
        __int64 cycles_diff = cycles_after_work - cycles_start_of_loop;


        if (num_values < NUM_BUFFERED_CYCLES) {
            buffered_cycles[num_values] = cycles_diff;
        }
        num_values++;


#if 0
        if (value.is_end_of_line) {
            ++num_lines;
            if (num_columns == -1) {
                num_columns = value.idx + 1;
            }
        }
#endif

        if (end) {
            break;
        } else if (csv.read_used != csv.read_max) {
            break;
        }
    }
    __int64 cycles_end_of_standard = __rdtsc();

    itj_csv_close_fh(&csv);

    sitrep("The following numbers are for the non-SIMD version\n");

    print_total(bytes_read, multiplier, cycles_start_of_standard, cycles_end_of_standard);


    itj_csv_umax mean = 0;
    __int64 total = (num_values < NUM_BUFFERED_CYCLES ? num_values : NUM_BUFFERED_CYCLES);
    for (itj_csv_umax i = 0; i < total; ++i) {
        mean += buffered_cycles[i];
    }

    mean /= total;

    sitrep("The mean cycle amount for job is %llu\n", mean);
    sitrep("The mean in nanoseconds for job is %0.2f\n", (double)mean * multiplier);


    sitrep("The mean cycle amount for main loop is %llu\n", csv.profiler.main_loop_avg);
    sitrep("The mean cycle amount for was in quotes is %llu\n", csv.profiler.was_in_quotes_avg);
    sitrep("The mean cycle amount for got doubles is %llu\n", csv.profiler.got_doubles_avg);

    getc(stdin);

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
