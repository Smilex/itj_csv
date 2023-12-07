/*
 * LICENSE available at the bottom
 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <time.h>

#include <immintrin.h>

#ifdef IS_WINDOWS
#include <windows.h>
#endif

#define ITJ_CSV_IMPLEMENTATION_AVX2
#include "itj_csv.h"

#define KB(x) (x * 1024)
#define MB(x) (KB(x) * 1024)
#define SIZE_OF_NULL_TERMINATOR 1
#define SIZE_OF_DIR_SEPERATOR 1
#define NUM_BUFFERED_CYCLES (10 * 1000 * 1000)

#ifdef IS_WINDOWS
#define DIR_SEPERATOR "\\"
#else
#define DIR_SEPERATOR "/"
#endif

#ifdef IS_WINDOWS
double g_freq;

double get_time_ms() {
    LARGE_INTEGER li;
    QueryPerformanceCounter(&li);
    return ((double)li.QuadPart)/g_freq;
}
#else
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1000000.0;
}
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

void print_total(itj_csv_umax bytes_read, double time_start, double time_end) {
    double time_diff_ms = time_end - time_start;
    double time_diff_sec = time_diff_ms / 1000.0;

    sitrep("Read %llu total bytes. %.3f MB per second\n", bytes_read, ((double)bytes_read / (1024.0 * 1024.0)) / time_diff_sec);

}

void test_correctness(struct itj_csv_value value, itj_csv_smax num_columns, itj_csv_smax *num_columns_out, itj_csv_u32 num_lines, itj_csv_u32 *num_lines_out) {
    itj_csv_umax column; 
    if (num_columns == -1) {
        column = value.idx;
    } else {
        column = value.idx % num_columns;
    }

    if (column == 0) {
        if (num_lines == 0) {
            test_print("Is header[0] == column1");
            test_print_result(compare_strings(value.data.base, value.data.len, "column1", 7));
        } else if (num_lines == 1) {
            test_print("Is the first column of the first row == row1_column1");
            test_print_result(compare_strings(value.data.base, value.data.len, "row1_column1", 12));
        } else if (num_lines == 2) {
            test_print("Is the first column of the second row == row2_column1");
            test_print_result(compare_strings(value.data.base, value.data.len, "row2_column1", 12));
        }
    } else if (column == 1) {
        if (num_lines == 0) {
            test_print("Is header[1] == column2");
            test_print_result(compare_strings(value.data.base, value.data.len, "column2", 7));
        } else if (num_lines == 1) {
            test_print("Is the second column of the first row == row1_column2");
            test_print_result(compare_strings(value.data.base, value.data.len, "row1_column2", 12));
        } else if (num_lines == 2) {
            test_print("Is the second column of the second row == row2_column2");
            test_print_result(compare_strings(value.data.base, value.data.len, "row2_column2", 12));
        }
    } else if (column == 2) {
        if (num_lines == 0) {
            test_print("Is header[2] == column\\r\\n3");
            test_print_result(compare_strings(value.data.base, value.data.len, "column\r\n3", 9));
        } else if (num_lines == 1) {
            test_print("Is the third column of the first row == row1_column\\r\\n3");
            test_print_result(compare_strings(value.data.base, value.data.len, "row1_column\r\n3", 14));
        } else if (num_lines == 2) {
            test_print("Is the third column of the second row == row2_column\\r\\n3");
            test_print_result(compare_strings(value.data.base, value.data.len, "row2_column\r\n3", 14));
        }
    } else if (column == 3) {
        if (num_lines == 0) {
            test_print("Is header[3] == column\"4");
            test_print_result(compare_strings(value.data.base, value.data.len, "column\"4", 8));
        } else if (num_lines == 1) {
            test_print("Is the fourth column of the first row == row1_column\"4");
            test_print_result(compare_strings(value.data.base, value.data.len, "row1_column\"4", 13));
        } else if (num_lines == 2) {
            test_print("Is the fourth column of the second row == row2_column\"4");
            test_print_result(compare_strings(value.data.base, value.data.len, "row2_column\"4", 13));
        }
    } else if (column == 4) {
        if (num_lines == 0) {
            test_print("Is header[4] == columnð");
            test_print_result(compare_strings(value.data.base, value.data.len, "columnð", 8));
        } else if (num_lines == 1) {
            test_print("Is the fifth column of the first row == row1_columnð");
            test_print_result(compare_strings(value.data.base, value.data.len, "row1_columnð", 13));
        } else if (num_lines == 2) {
            test_print("Is the fifth column of the second row == row2_columnð");
            test_print_result(compare_strings(value.data.base, value.data.len, "row2_columnð", 13));
        }
    }

    if (value.is_end_of_line) {
        if (num_columns == -1) {
            *num_columns_out = value.idx + 1;
        }

        *num_lines_out = num_lines + 1;
    }
}

int main(int argc, char *argv[]) {
    printf("Press any key to start\n");
    getc(stdin);

#ifdef IS_WINDOWS
    {
        LARGE_INTEGER large_integer;
        QueryPerformanceFrequency(&large_integer);
        g_freq = ((double)large_integer.QuadPart) / 1000.0;
    }
#endif

    g_sitrep_filepath = (char *)calloc(1, KB(10));
    if (!g_sitrep_filepath) {
        printf("Unable to allocate memory for sitrep filepath\n");
        return EXIT_FAILURE;
    }
    snprintf(g_sitrep_filepath, KB(10), "itj_csv_test_%lld.log", time(NULL));

    itj_csv_smax *buffered_cycles = (itj_csv_smax *)calloc(1, NUM_BUFFERED_CYCLES * sizeof(*buffered_cycles));
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

    printf("Running standard correctness tests\n");
    g_did_a_test_fail = ITJ_CSV_FALSE;

    itj_csv_umax buffer_max = MB(2);
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

    itj_csv_bool end = ITJ_CSV_FALSE;
    itj_csv_u32 num_lines = 0;
    itj_csv_smax num_columns = -1;
    itj_csv_bool first = ITJ_CSV_TRUE;
    for (;;) {
        struct itj_csv_value value = itj_csv_get_next_value(&csv);
        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                break;
            } else {
                continue;
            }
        }

        test_correctness(value, num_columns, &num_columns, num_lines, &num_lines);

        if (value.is_end_of_line && first) {
            test_print("Expecting 5 columns in header");
            test_print_result(value.idx == 4);
            first = ITJ_CSV_FALSE;
        }

    }

    itj_csv_close_fh(&csv);

    if (g_did_a_test_fail) {
        sitrep("NOT ALL CORRECTNESS TESTS COMPLETED SUCCESSFULLY!\n");
    } else {
        sitrep("ALL CORRECTNESS TESTS COMPLETED SUCCESSFULL\n");
    }

    printf("Running AVX2 correctness tests\n");
    g_did_a_test_fail = ITJ_CSV_FALSE;

    if (!itj_csv_open(&csv, correctness_with_header_csv_path, correctness_with_header_csv_path_len, buffer, buffer_max, ITJ_CSV_DELIM_COMMA, NULL)) {
        printf("Failed to initalize itj_csv struct to file, '%s'\n", correctness_with_header_csv_path);
        return EXIT_FAILURE;
    }

    pump_ret = itj_csv_pump_stdio(&csv);
    if (pump_ret == 0) {
        printf("Unable to read from '%s'\n", correctness_with_header_csv_path);
        return EXIT_FAILURE;
    }

    num_lines = 0;
    num_columns = -1;
    first = ITJ_CSV_TRUE;
    for (;;) {
        struct itj_csv_value value = itj_csv_get_next_value_avx2(&csv);
        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                break;
            } else {
                continue;
            }
        }

        test_correctness(value, num_columns, &num_columns, num_lines, &num_lines);

        if (value.is_end_of_line && first) {
            test_print("Expecting 5 columns in header");
            test_print_result(value.idx == 4);
            first = ITJ_CSV_FALSE;
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

    double time_start_of_reference = get_time_ms();

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

    double time_end_of_reference = get_time_ms();

    fclose(fh);

    print_total(bytes_read, time_start_of_reference, time_end_of_reference);

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

    num_columns = -1;
    num_lines = 0;
    itj_csv_smax num_values = 0;
    double time_start_of_standard = get_time_ms();
    for (;;) {
        struct itj_csv_value value = itj_csv_get_next_value_avx2(&csv);

#if 0
        itj_csv_umax column;
        if (num_columns == -1) {
            column = value.idx;
        } else {
            column = value.idx % num_columns;
        }
#endif

        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                break;
            } else {
                bytes_read += pump_ret;
                continue;
            }
        }

        if (value.is_end_of_line) {
            ++num_lines;
            if (num_columns == -1) {
                num_columns = value.idx + 1;
            }
        }

        ++num_values;

    }
    double time_end_of_standard = get_time_ms();


    sitrep("The following numbers are for the non-SIMD version\n");

    print_total(bytes_read, time_start_of_standard, time_end_of_standard);
    itj_csv_close_fh(&csv);


    sitrep("The number of iterations %llu\n", num_values);
    sitrep("The number of lines %llu\n", num_lines);
    sitrep("The number of columns %llu\n", num_columns);

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

    num_columns = -1;
    num_lines = 0;
    num_values = 0;
    double time_start_of_avx = get_time_ms();
    for (;;) {
        struct itj_csv_value value = itj_csv_get_next_value_avx(&csv);

#if 0
        itj_csv_umax column;
        if (num_columns == -1) {
            column = value.idx;
        } else {
            column = value.idx % num_columns;
        }
#endif

        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                break;
            } else {
                bytes_read += pump_ret;
                continue;
            }
        }

        if (value.is_end_of_line) {
            ++num_lines;
            if (num_columns == -1) {
                num_columns = value.idx + 1;
            }
        }

        ++num_values;

    }
    double time_end_of_avx = get_time_ms();


    sitrep("The following numbers are for the AVX version\n");

    print_total(bytes_read, time_start_of_avx, time_end_of_avx);
    itj_csv_close_fh(&csv);

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

    num_columns = -1;
    num_lines = 0;
    num_values = 0;
    double time_start_of_avx2 = get_time_ms();
    for (;;) {
        struct itj_csv_value value = itj_csv_get_next_value_avx2(&csv);

#if 0
        itj_csv_umax column;
        if (num_columns == -1) {
            column = value.idx;
        } else {
            column = value.idx % num_columns;
        }
#endif

        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                break;
            } else {
                bytes_read += pump_ret;
                continue;
            }
        }

        if (value.is_end_of_line) {
            ++num_lines;
            if (num_columns == -1) {
                num_columns = value.idx + 1;
            }
        }

        ++num_values;

    }
    double time_end_of_avx2 = get_time_ms();


    sitrep("The following numbers are for the AVX2 version\n");

    print_total(bytes_read, time_start_of_avx2, time_end_of_avx2);
    itj_csv_close_fh(&csv);

    fh = fopen(generated_csv_path, "rb");
    if (!fh) {
        printf("Unable to open %s for the second reference reading\n", generated_csv_path);
        return EXIT_FAILURE;
    }

    double time_start_of_reference2 = get_time_ms();

    bytes_read = 0;
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

    double time_end_of_reference2 = get_time_ms();

    sitrep("Reading generated csv file without any work as reference, a final time\n");
    print_total(bytes_read, time_start_of_reference2, time_end_of_reference2);

    fclose(fh);

    printf("Finished. Press any key to quit\n");
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
