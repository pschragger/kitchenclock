#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <time.h>
#include <string.h>
#include <stdint.h>

// Define the font (simple 3x5 representation for demo purposes)
const int CY = 5;
const int CX = 3;
const char *digits[10] = {
   "111101101101111",  // 0
   "010010010010010",  // 1
   "111001111100111",  // 2
   "111001111001111",  // 3
   "101101111001001",  // 4
   "111100111001111",  // 5
   "111100111101111",  // 6
   "111001001001001",  // 7
   "111101111101111",  // 8
   "111101111001111"   // 9
};

const char *colon  = "000010000010000";
  
static char DISPLAY_DIGIT[10] = {
  '0',
  '1',
  '2',
  '3',
  '4',
  '5',
  '6',
  '7',
  '8',
  '9'
};
  
const int PIX_X = 20;
const int PIX_Y = 20;
const uint16_t COLORLIST[4] = {
       0xF800, // Red
       0x07E0, // green
       0x001F, // BLUE
       0xFFFF  // WHITE
};
const uint16_t BLACK = 0x0000;
const uint16_t WHITE = 0xFFFF;

#define WIDTH 700 
#define HEIGHT 480
#define BYTES_PER_PIXEL 2
#define KERNING 2
#define COLOR_HOLD 10

void draw_digit(char digit, int x_offset, int y_offset, uint16_t color, uint16_t *fb_ptr, int line_length) {
   if (digit < '0' || digit > '9') return;
   const char *bitmap = digits[digit - '0'];
   for (int y = 0; y < CY; y++) {
       for (int x = 0; x < CX; x++) {
	   if (bitmap[y * CX + x] == '1') {
	       for (int dy = 0; dy < PIX_Y; dy++) {
		   for (int dx = 0; dx < PIX_X; dx++) {
		     int pixel_x = x_offset + x * PIX_X + dx;
		       int pixel_y = y_offset + y * PIX_Y + dy;
		       if (pixel_x < WIDTH && pixel_y < HEIGHT) {
			   fb_ptr[pixel_y * (line_length / BYTES_PER_PIXEL) + pixel_x] = color;
		       }
		   }
	       }
	   }
       }
   }
}
void draw_colon(char digit, int x_offset, int y_offset, uint16_t color, uint16_t *fb_ptr, int line_length) {
  if (digit != ':' ) return;
  const char *bitmap = colon;
  for (int y = 0; y < CY; y++) {
       for (int x = 0; x < CX; x++) {
	   if (bitmap[y * CX + x] == '1') {
	       for (int dy = 0; dy < PIX_Y; dy++) {
		   for (int dx = 0; dx < PIX_X; dx++) {
		     int pixel_x = x_offset + x * PIX_X + dx;
		       int pixel_y = y_offset + y * PIX_Y + dy;
		       if (pixel_x < WIDTH && pixel_y < HEIGHT) {
			   fb_ptr[pixel_y * (line_length / BYTES_PER_PIXEL) + pixel_x] = color;
		       }
		   }
	       }
	   }
       }
   }
}
void clear_screen(uint16_t *fb_ptr, int screen_size) {
   memset(fb_ptr, 0, screen_size);
}

int main() {

   int fb_fd = open("/dev/fb0", O_RDWR);
   if (fb_fd == -1) {
       perror("Error opening /dev/fb0");
       return 1;
   }

   struct fb_var_screeninfo vinfo;
   struct fb_fix_screeninfo finfo;

   if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo)) {
       perror("Error reading variable information");
       close(fb_fd);
       return 1;
   }

   if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo)) {
       perror("Error reading fixed information");
       close(fb_fd);
       return 1;
   }

   long screen_size = vinfo.yres_virtual * finfo.line_length;

   uint16_t *fb_ptr = (uint16_t *)mmap(0, screen_size, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
   int fb_line_length = 1 * finfo.line_length;

   if (fb_ptr == MAP_FAILED) {
       perror("Error mapping framebuffer device to memory");
       close(fb_fd);
       return 1;
   }
   pid_t pid = fork();
   if( pid < 0) {
	perror("digitalclock fork failed");
	exit(1);
   }
   else if(pid==0){
       int colorcountdown = COLOR_HOLD;
       int currentcolor = 2;
       time_t interval_time = 0;
       int display_number=0;
   while (1) {

       time_t t = time(NULL);
       struct tm tm = *localtime(&t);
       char buffer[9];
       snprintf(buffer, sizeof(buffer), "%02d:%02d:%02d",tm.tm_hour,tm.tm_min,tm.tm_sec);    
 /*	        snprintf(buffer, sizeof(buffer), "%02d : %02d", tm.tm_hour, tm.tm_min);    */
       if( interval_time == 0 ){
	       interval_time = t+1;
       }
       if( t >= interval_time ){
	 interval_time = t + 1;
	 colorcountdown--;
	 if( colorcountdown == 0 ){
	   colorcountdown = COLOR_HOLD;
	   currentcolor++;
	   display_number++;

	   }
       }
      
       if(currentcolor >= 4 || currentcolor < 1){
	 currentcolor = 1;
       }
       clear_screen(fb_ptr, screen_size);

       int x_offset = WIDTH / 2 - PIX_X*CX/2; // Centered X
       int y_offset = HEIGHT / 2 - PIX_Y*CY/2; // Centered Y
        x_offset =  CX*1.5 ;  
       if( display_number > 9 ) {
	 display_number = 0;
   }
       for(int i = 0;i<2 ;i++){
	 
	 draw_digit(buffer[0], x_offset, y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
	 draw_digit(buffer[1], x_offset+PIX_X+3*PIX_X, y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
	 draw_colon(buffer[2], x_offset+2*PIX_X+6*PIX_X, y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
	 draw_digit(buffer[3], x_offset+3*PIX_X+9*PIX_X, y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
	 draw_digit(buffer[4], x_offset+4*PIX_X+12*PIX_X, y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
	 draw_colon(buffer[5], x_offset+5*PIX_X+15*PIX_X, y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
	 draw_digit(buffer[6], x_offset+6*PIX_X+18*PIX_X, y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
	 draw_digit(buffer[7], x_offset+7*PIX_X+21*PIX_X, y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
		 
       }
	sleep(1);
   }
} else {

   printf(" child process (PID: %d) started digitalclock service .\n", pid);
   exit(0);
}
    munmap(fb_ptr, screen_size);
    close(fb_fd);
    return 0;
}

