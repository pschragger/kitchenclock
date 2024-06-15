/*
//sudo apt-get install libpng-dev
//
//gcc -o png_slideshow.exe png_slideshow.c  -lpng
//  sudo /home/kitcheclock/src/png_slideshow.exe  /home/piclock/images/image1.png /home/piclock/images/image2.png /home/piclock/images/image3.png */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include <png.h>
#include <time.h>

void display_png(const char *filename, int fbfd, struct fb_var_screeninfo vinfo, struct fb_fix_screeninfo finfo);
void delay(int number_of_seconds);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image1.png> <image2.png> ...\n", argv[0]);
        return 1;
    }

    // Open the framebuffer device file
    int fbfd = open("/dev/fb0", O_RDWR);
    if (fbfd == -1) {
        perror("Error: cannot open framebuffer device");
        return 1;
    }

    // Get fixed screen information
    struct fb_fix_screeninfo finfo;
    if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo) == -1) {
        perror("Error reading fixed information");
        return 1;
    }

    // Get variable screen information
    struct fb_var_screeninfo vinfo;
    if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo) == -1) {
        perror("Error reading variable information");
        return 1;
    }

    // Loop through images
    while (1) {
        for (int i = 1; i < argc; i++) {
            display_png(argv[i], fbfd, vinfo, finfo);
            delay(30); // Delay for 30 seconds
        }
    }

    close(fbfd);
    return 0;
}

void display_png(const char *filename, int fbfd, struct fb_var_screeninfo vinfo, struct fb_fix_screeninfo finfo) {
    // Open the PNG file
    FILE *fp = fopen(filename, "rb");
    if (!fp) {
        perror("Error opening PNG file");
        return;
    }

    // Read PNG file header
    png_byte header[8];
    fread(header, 1, 8, fp);
    if (png_sig_cmp(header, 0, 8)) {
        fprintf(stderr, "Error: not a PNG file\n");
        fclose(fp);
        return;
    }

    // Initialize PNG structures
    png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    if (!png) {
        fclose(fp);
        return;
    }

    png_infop info = png_create_info_struct(png);
    if (!info) {
        png_destroy_read_struct(&png, NULL, NULL);
        fclose(fp);
        return;
    }

    if (setjmp(png_jmpbuf(png))) {
        png_destroy_read_struct(&png, &info, NULL);
        fclose(fp);
        return;
    }

    png_init_io(png, fp);
    png_set_sig_bytes(png, 8);
    png_read_info(png, info);

    int width = png_get_image_width(png, info);
    int height = png_get_image_height(png, info);
    png_byte color_type = png_get_color_type(png, info);
    png_byte bit_depth = png_get_bit_depth(png, info);

    if (bit_depth == 16) png_set_strip_16(png);
    if (color_type == PNG_COLOR_TYPE_PALETTE) png_set_palette_to_rgb(png);
    if (color_type == PNG_COLOR_TYPE_GRAY && bit_depth < 8) png_set_expand_gray_1_2_4_to_8(png);
    if (png_get_valid(png, info, PNG_INFO_tRNS)) png_set_tRNS_to_alpha(png);
    if (color_type == PNG_COLOR_TYPE_RGB || color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_PALETTE) png_set_filler(png, 0xFF, PNG_FILLER_AFTER);
    if (color_type == PNG_COLOR_TYPE_GRAY || color_type == PNG_COLOR_TYPE_GRAY_ALPHA) png_set_gray_to_rgb(png);

    png_read_update_info(png, info);

    png_bytep *row_pointers = (png_bytep *)malloc(sizeof(png_bytep) * height);
    for (int y = 0; y < height; y++) {
        row_pointers[y] = (png_byte *)malloc(png_get_rowbytes(png, info));
    }

    png_read_image(png, row_pointers);

    // Calculate location
    long screensize = vinfo.yres_virtual * finfo.line_length;
    char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((intptr_t)fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        return;
    }

    // Write to framebuffer
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            long location = y * vinfo.xres_virtual * vinfo.bits_per_pixel / 8 + x * vinfo.bits_per_pixel / 8;
            if (vinfo.bits_per_pixel == 32) {
                *(fbp + location) = row_pointers[y][x * 4 + 2];     // Blue
                *(fbp + location + 1) = row_pointers[y][x * 4 + 1]; // Green
                *(fbp + location + 2) = row_pointers[y][x * 4];     // Red
                *(fbp + location + 3) = row_pointers[y][x * 4 + 3]; // Alpha
            } else if (vinfo.bits_per_pixel == 16) {
                int b = row_pointers[y][x * 4 + 2] >> 3;
                int g = row_pointers[y][x * 4 + 1] >> 2;
                int r = row_pointers[y][x * 4] >> 3;
                unsigned short int t = r << 11 | g << 5 | b;
                *((unsigned short int *)(fbp + location)) = t;
            }
        }
    }

    // Cleanup
    munmap(fbp, screensize);
    for (int y = 0; y < height; y++) {
        free(row_pointers[y]);
    }
    free(row_pointers);
    png_destroy_read_struct(&png, &info, NULL);
    fclose(fp);
}

void delay(int number_of_seconds) {
    int milli_seconds = 1000 * number_of_seconds;
    clock_t start_time = clock();
    while (clock() < start_time + milli_seconds);
}
