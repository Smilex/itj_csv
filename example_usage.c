#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

#define ITJ_CSV_IMPLEMENTATION
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

    struct itj_csv_value value;
    value = itj_csv_get_next_value(&csv); // Skip Data Source
    value = itj_csv_get_next_value(&csv); // Skip Data Source Value
    itj_csv_ignore_newlines(&csv);
    value = itj_csv_get_next_value(&csv); // Skip Last Updated Date
    value = itj_csv_get_next_value(&csv); // Skip Last Updated Date Value
    itj_csv_ignore_newlines(&csv);

    struct itj_csv_header header;
    if (!itj_csv_parse_header(&csv, &header)) {
        printf("Unable to parse header from '%s'\n", csv_path);
        return EXIT_FAILURE;
    }

    itj_csv_u32 column_offset = 4;
    struct entry ent;
    for (;;) {
        itj_csv_umax column = (csv.idx - column_offset) % header.num_columns;

        value = itj_csv_get_next_value(&csv);
        if (value.is_end_of_data) {
            break;
        }
        if (value.need_data) {
            pump_ret = itj_csv_pump_stdio(&csv);
            if (pump_ret == 0) {
                break;
            }
            continue;
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
            continue;
        }




        if (column == 0) {
            if (!string_alloc(&ent.country_name, value.base, value.len)) {
                printf("Unable to allocate memory for string\n");
                return EXIT_FAILURE;
            }
        } else if (column == 1) {
            if (!string_alloc(&ent.country_code, value.base, value.len)) {
                printf("Unable to allocate memory for string\n");
                return EXIT_FAILURE;
            }
        } else if (column == 2) {
            if (!string_alloc(&ent.indicator_name, value.base, value.len)) {
                printf("Unable to allocate memory for string\n");
                return EXIT_FAILURE;
            }
        } else if (column == 3) {
            if (!string_alloc(&ent.indicator_code, value.base, value.len)) {
                printf("Unable to allocate memory for string\n");
                return EXIT_FAILURE;
            }
        } else if (column > 3 && column <= 4 + (DATE_END - DATE_START)) {
            float val = NAN;
            if (value.len > 0) {
                char *start = (char *)value.base;
                char *expected_end = start + value.len;
                char *end;

                val = strtof(start, &end);

                if (end != expected_end) {
                    printf("WARNING: floating point value didn't get parsed to the point we expected. Result: %f\n", val);
                }
            }

            itj_csv_umax idx = (column - 4);
            ent.dates[idx] = val;
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
                            printf("No data available from 1960 to %d\n", date);
                        }

                        if (!isnan(val)) {
                            printf("%d: %f\n", date, val);
                        }
                    }
                }
            }
        }

        printf("\n");
    }


    return EXIT_SUCCESS;
}
