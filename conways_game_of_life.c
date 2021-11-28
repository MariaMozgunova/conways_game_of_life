#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>
#include <math.h>
#include <stddef.h>
#include <sys/stat.h>  // mkdir()
#include <sys/types.h>

uint8_t ALL_DEAD = 255;

uint32_t WIDTH;
uint32_t HEIGHT;
uint32_t ROW_SIZE;
uint32_t PIXEL_ARRAY_OFFSET;
int BITS_PER_PIXEL = 1;  // monochrome picture
int PADDING_BITS;

char* INPUT_FILE = 0;
char* DIR_NAME = 0;
int MAX_ITER = -1;
int DUMP_FREQ = 1;

void parse_args(int count, char** args) {    
    for (int i = 1; i < count; i += 2) {
        if (strcmp(args[i], "--input") == 0) {
            INPUT_FILE = args[i + 1];
        } else if (strcmp(args[i], "--output") == 0) {
            DIR_NAME = args[i + 1];
        } else if (strcmp(args[i], "--max_iter") == 0) {
            MAX_ITER = atoi(args[i + 1]);
        } else if (strcmp(args[i], "--dump_freq") == 0) {
            DUMP_FREQ = atoi(args[i + 1]);
        }
    }
    
    if (INPUT_FILE == 0) {
        printf("No input file given. To specify input file use '--input <file_name>'.");
        exit(1);
    } else if (DIR_NAME == 0) {
        printf("No output directory given. To specify output directory use '--output <directory_name>'.");
        exit(1);
    }

    struct stat st;

    if (stat(DIR_NAME, &st) == -1) {
        printf("Creating directory %s...\n", DIR_NAME);
        
        if (mkdir(DIR_NAME) == -1) {
            printf("Can't create a directory %s", DIR_NAME);
            exit(1);
        } else {
            printf("Successfully created directory %s\n", DIR_NAME);
        }
    } else if ((st.st_mode & S_IFMT) != S_IFDIR) {
        printf("%s is not a directory", DIR_NAME);
        exit(1);
    }

    if (stat(INPUT_FILE, &st) == -1) {
        printf("Input file %s doesn't exist", INPUT_FILE);
        exit(1);
    }
}

void reverse_incomplete_byte(uint8_t row[]) {
    row[1 + WIDTH / 8] ^= (1 << (PADDING_BITS % 8)) - 1;
}

void reverse_padding(uint8_t row[]) {
    if (PADDING_BITS % 8) {
        reverse_incomplete_byte(row);
    }
    
    for (int i = (WIDTH + 7) / 8 + 1; i < ROW_SIZE + 1; ++i) {
        row[i] ^= ALL_DEAD;
    }
}

void print_row(uint8_t row[]) {
    for (int i = 0; i < ROW_SIZE + 2; ++i) {
        printf("%d ", row[i]);
    }

    printf("\n");
}

void fread_row(FILE* in, uint8_t row[]) {
    row[0] = ALL_DEAD;
    row[ROW_SIZE + 1] = ALL_DEAD;

    for (int i = 1; i < ROW_SIZE + 1; ++i) {
        row[i] = fgetc(in);
    }

    reverse_padding(row);
}

void fill_with_1s(uint8_t row[]) {
    for (int i = 0; i < ROW_SIZE + 2; ++i) {
        row[i] = ALL_DEAD;
    }
}

uint32_t parse_number(FILE* in) {
    int bytes = 4;
    uint8_t little_endian_number[bytes];

    for (int i = 0; i < bytes; ++i) {
        little_endian_number[i] = fgetc(in);
    }

    uint32_t res = 0;

    for (int i = 3; i >= 0; --i) {
        res = (res << 8) + little_endian_number[i];
    }

    return res;
}

void fread_bmp_info(char input_file[]) {
    FILE* in = fopen(input_file, "rb");
    fseek(in, 10, SEEK_SET);
    PIXEL_ARRAY_OFFSET = parse_number(in);
    fseek(in, 18, SEEK_SET);
    WIDTH = parse_number(in);
    fseek(in, 22, SEEK_SET);
    HEIGHT = parse_number(in);
    ROW_SIZE = ((BITS_PER_PIXEL * WIDTH + 31) / 32) * 4;
    PADDING_BITS = ROW_SIZE * 8 - WIDTH; 
    fclose(in);
}

void fcopy_bmp_info(char intput_file[], FILE* out) {
    FILE* in = fopen(intput_file, "rb");

    for (int i = 0; i < PIXEL_ARRAY_OFFSET; ++i) {
        fprintf(out, "%c", fgetc(in));
    }
    
    fclose(in);
}

int read_bit(uint8_t* bytes, int k) {
    return (bytes[k / 8] & (1 << (7 - (k % 8)))) != 0;
}

void alive_bit(uint8_t* bytes, int k) {
    bytes[k / 8] ^= 1 << (7 - (k % 8));
}

int count_neighbors(uint8_t prev[], uint8_t cur[], uint8_t next[], int k) {
    return 8 - (read_bit(prev, k - 1) + read_bit(prev, k) + read_bit(prev, k + 1) +
           read_bit(cur, k - 1) + read_bit(cur, k + 1) +
           read_bit(next, k - 1) + read_bit(next, k) + read_bit(next, k + 1));
}

void make_new_row(uint8_t prev[], uint8_t cur[], uint8_t next[], uint8_t new[]) {
    for (int k = 8; k < WIDTH + 8; ++k) {
        int live_neighbors = count_neighbors(prev, cur, next, k);
        int bit = read_bit(cur, k);

        if (live_neighbors == 3 || !bit && live_neighbors == 2) {
            alive_bit(new, k);
        }
    }
}

void make_path(char res[], char dir[], int i) {
    char file_name[30];
    itoa(i, file_name, 10);
    strcat(file_name, ".bmp");
    strcpy(res, dir);
    strcat(res, "/");
    strcat(res, file_name);
}

void next_iteration(uint8_t pixel_array[][ROW_SIZE + 2], uint8_t res[][ROW_SIZE + 2]) {
    for (int i = 1; i < HEIGHT + 1; ++i) {
        fill_with_1s(res[i]);
        make_new_row(pixel_array[i - 1], pixel_array[i], pixel_array[i + 1], res[i]);
    }
}

void initialize_pixel_array(uint8_t pixel_array[][ROW_SIZE + 2]) {
    fill_with_1s(pixel_array[0]);
    fill_with_1s(pixel_array[HEIGHT + 1]);
    
    FILE* in = fopen(INPUT_FILE, "rb");
    fseek(in, PIXEL_ARRAY_OFFSET, SEEK_SET);

    for (int i = 1; i < HEIGHT + 1; ++i) {
        fread_row(in, pixel_array[i]);
    }
    
    fclose(in);
}

void fwrite_pixel_array(int i, uint8_t pixel_array[][ROW_SIZE + 2]) {
    char output_file[50];
    make_path(output_file, DIR_NAME, i);
    FILE* out = fopen(output_file, "wb");
    fcopy_bmp_info(INPUT_FILE, out);

    for (int i = 1; i < HEIGHT + 1; ++i) {
        reverse_padding(pixel_array[i]);
        fwrite(pixel_array[i] + 1, 1, ROW_SIZE, out);
        reverse_padding(pixel_array[i]);
    }

    fclose(out);
}

int main(int argc, char* argv[]) {
    parse_args(argc, argv);

    fread_bmp_info(INPUT_FILE);
    uint8_t pixel_array[HEIGHT + 2][ROW_SIZE + 2];
    uint8_t res[HEIGHT + 2][ROW_SIZE + 2];
    fill_with_1s(res[0]);
    fill_with_1s(res[HEIGHT + 1]);
    initialize_pixel_array(pixel_array);

    for (int i = 0; i < MAX_ITER; ++i) {
        if (i % 2) {
            next_iteration(res, pixel_array);
        } else {
            next_iteration(pixel_array, res);
        }

        if (i % DUMP_FREQ == 0) {
            if (i % 2) {
                fwrite_pixel_array(i, pixel_array);
            } else {
                fwrite_pixel_array(i, res);
            }
        }
    }

    return 0;
}