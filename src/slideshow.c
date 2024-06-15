/*  AUTHOR: Paul Schragger 
    INITIAL CODE WRITTEN BY: Chatgpt using prompt 
    "create a c program to write a list of jpg images to a pi framebuffer in a loop with 30 seconds delay" */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <jpeglib.h>
#include <time.h>

void display_image(const char *filename, int fbfd, struct fb_var_screeninfo vinfo, struct fb_fix_screeninfo finfo);
void delay(int number_of_seconds);

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s <image1.jpg> <image2.jpg> ...\n", argv[0]);
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
            display_image(argv[i], fbfd, vinfo, finfo);
            delay(30); // Delay for 30 seconds
        }
    }

    close(fbfd);
    return 0;
}

void display_image(const char *filename, int fbfd, struct fb_var_screeninfo vinfo, struct fb_fix_screeninfo finfo) {
    // Open the jpeg file
    FILE *infile = fopen(filename, "rb");
    if (!infile) {
        perror("Error opening jpeg file");
        return;
    }

    // Setup decompression
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_stdio_src(&cinfo, infile);
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    int row_stride = cinfo.output_width * cinfo.output_components;
    unsigned char *buffer = (unsigned char *)malloc(row_stride);

    // Calculate location
    long screensize = vinfo.yres_virtual * finfo.line_length;
    char *fbp = (char *)mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
    if ((intptr_t)fbp == -1) {
        perror("Error: failed to map framebuffer device to memory");
        return;
    }

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, &buffer, 1);

        // Write to framebuffer
        for (int x = 0; x < cinfo.output_width; x++) {
            long location = (cinfo.output_scanline - 1) * vinfo.xres_virtual * vinfo.bits_per_pixel / 8 + x * vinfo.bits_per_pixel / 8;
            if (vinfo.bits_per_pixel == 32) {
                *(fbp + location) = buffer[x * 3 + 2];       // Blue
                *(fbp + location + 1) = buffer[x * 3 + 1];   // Green
                *(fbp + location + 2) = buffer[x * 3];       // Red
                *(fbp + location + 3) = 0;                   // Transparency
            } else if (vinfo.bits_per_pixel == 16) {
                int b = buffer[x * 3 + 2] >> 3;
                int g = buffer[x * 3 + 1] >> 2;
                int r = buffer[x * 3] >> 3;
                unsigned short int t = r << 11 | g << 5 | b;
                *((unsigned short int *)(fbp + location)) = t;
            }
        }
    }

    // Cleanup
    munmap(fbp, screensize);
    free(buffer);
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    fclose(infile);
}

void delay(int number_of_seconds) {
    int milli_seconds = 1000 * number_of_seconds;
    clock_t start_time = clock();
    while (clock() < start_time + milli_seconds);
}
