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

const int DL_X = 17;
const int DL_Y = 5;
const char *DAY_LABELS[7] = {
  "0111101000101000110000010001011001011100100010101010000101000101001111110001110010001",  // SUN
  "1000100111001000111011010001011001101010100010101011000101000101001110001001110010001",  // MON
  "1111101000101111100100010001010000001000100010111000010001000101000000100001110011111",  // TUE
  "1000101111101110010001010000010010100010111000100101010101000001001001010011111011100",  // WED
  "1111101000101000100100010001010001001000111110100010010001000101000100100010001001110",  // THU
  "1111101110001111110000010010000100100000100100001001110001110000010010000010010011111",  // FRI
  "0111100010001111110000001010000100011100100010001000000101111100010011110010001000100"  // SAT
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
  
const int PIX_X = 30;
const int PIX_Y = 30;
const int DL_PIX_X =25;
const int DL_PIX_Y =25;

const uint16_t COLORLIST[5] = {
  0x0000,  // Black
  0xF800, // Red
  0x07E0, // green
  0x001F, // BLUE
  0xFFFF  // WHITE
};
const uint16_t BLACK = 0x0000;
const uint16_t WHITE = 0xFFFF;

#define WIDTH 720 
#define HEIGHT 480
#define BYTES_PER_PIXEL 2
#define KERNING 2
#define COLOR_HOLD 600
#define CLOCK_OFFSET_X  71
#define CLOCK_OFFSET_Y  36
#define DAY_LABEL_OFFSET_X  147
#define DAY_LABEL_OFFSET_Y  298

void draw_digit(char digit, int x_offset, int y_offset, uint16_t color, uint16_t *fb_ptr, int line_length) {
  int pix_color = color;
  int bg_color = 0 ;
   if (digit < '0' || digit > '9') return;
   const char *bitmap = digits[digit - '0'];
   for (int y = 0; y < CY; y++) {
       for (int x = 0; x < CX; x++) {
	   if (bitmap[y * CX + x] == '1') {
	     pix_color=color;
	   }else{
	     pix_color=bg_color;
	   }
	   
	   for (int dy = 0; dy < PIX_Y; dy++) {
	     for (int dx = 0; dx < PIX_X; dx++) {
	       int pixel_x = x_offset + x * PIX_X + dx;
	       int pixel_y = y_offset + y * PIX_Y + dy;
	       if (pixel_x < WIDTH && pixel_y < HEIGHT) {
		 fb_ptr[pixel_y * (line_length / BYTES_PER_PIXEL) + pixel_x] = pix_color;
	       }
	
	     }
	   }
       }
   }
}



void draw_day_label(int day_number, int x_offset, int y_offset, uint16_t color, uint16_t *fb_ptr, int line_length) {
  int pix_color = color;
  int bg_color = 0 ;
  if (day_number >= 7 || day_number <  0) return;
   const char *bitmap = DAY_LABELS[day_number];
   for (int y = 0; y < DL_Y; y++) {
       for (int x = 0; x < DL_X ; x++) {
	   if (bitmap[y * DL_X + x] == '1') {
	     pix_color=color;
	   }else{
	     pix_color=bg_color;
	   }
	   
	   for (int dy = 0; dy < DL_PIX_Y; dy++) {
	     for (int dx = 0; dx < DL_PIX_X; dx++) {
	       int pixel_x = x_offset + x * DL_PIX_X + dx;
	       int pixel_y = y_offset + y * DL_PIX_Y + dy;
	       if (pixel_x < WIDTH && pixel_y < HEIGHT) {
		 fb_ptr[pixel_y * (line_length / BYTES_PER_PIXEL) + pixel_x] = pix_color;
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
  char last_hour_tens = 'a';
  char last_hour_ones = 'a';
  char last_min_tens = 'a';
  char last_min_ones = 'a';
  int last_day = -1;
  int last_colon = 0;
  
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
       int currentcolor = 4;
       time_t interval_time = 0;
       int display_number=0;
       clear_screen(fb_ptr, screen_size);
       int day = -1;
       
   while (1) {

       time_t t = time(NULL);
       struct tm tm = *localtime(&t);
       char buffer[9];
       snprintf(buffer, sizeof(buffer), "%02d:%02d",tm.tm_hour,tm.tm_min);

       /*day++;
	 //testing the day fonts
       if(day >=7 || day <0)
	 {
	   day = 0;
	   
	 }
       */
	
       day = tm.tm_wday;
       //int month = tm.tm_mon;
       //int day = tm.tm_mday;
       
       snprintf(buffer, sizeof(buffer), "%02d:%02d",tm.tm_hour,tm.tm_min);    
       /*	        snprintf(buffer, sizeof(buffer), "%02d : %02d", tm.tm_hour, tm.tm_min);    */
       if( interval_time == 0 ){
	       interval_time = t+1;
       }
       if( t >= interval_time ){
	 interval_time = t + 1;
	 colorcountdown--;
	 if( colorcountdown == 0 ){
	   colorcountdown = COLOR_HOLD;
	   //currentcolor++;
	   display_number++;

	   }
       }
       currentcolor = 4;
       // if(currentcolor >= 5 || currentcolor < 1){
       //	 currentcolor = 1;
       //       }
      //clear_screen(fb_ptr, screen_size);

       //       int x_offset =  WIDTH / 2 - PIX_X*CX/2; // Centered X
       // int y_offset = HEIGHT / 2 - PIX_Y*CY/2; // Centered Y

       int x_offset = CLOCK_OFFSET_X;
       int y_offset = CLOCK_OFFSET_Y;
       int x_day_offset = DAY_LABEL_OFFSET_X;
       int y_day_offset = DAY_LABEL_OFFSET_Y;
       
       if( display_number > 9 ) {
	 display_number = 0;
       }
       int testnum = '0';
       if(last_hour_tens != testnum ){
	 last_hour_tens = testnum  ;
	 draw_digit(buffer[0], x_offset, y_offset,COLORLIST[currentcolor] ,
		  fb_ptr, finfo.line_length);
       }
       if( last_hour_ones != buffer[1]){
	 last_hour_ones = buffer[1];
	 draw_digit(buffer[1], x_offset+PIX_X+3*PIX_X, y_offset,
		  COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
       }
       if( last_colon == 0 ){
	 last_colon = 4;
       }else{
	 last_colon = 0;
       }
	   
       draw_colon(buffer[2], x_offset+2*PIX_X+6*PIX_X, y_offset,
		  COLORLIST[last_colon] , fb_ptr, finfo.line_length);

       if( last_min_tens != buffer[3]){
	 last_min_tens = buffer[3];
	 draw_digit(buffer[3], x_offset+3*PIX_X+9*PIX_X, y_offset,
		    COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
       }
       if(last_min_ones != buffer[4]){
	 last_min_ones = buffer[4];
	 draw_digit(buffer[4], x_offset+4*PIX_X+12*PIX_X,
		    y_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);}
		 
       if( last_day != day ){
	 last_day = day;
	 draw_day_label(day, x_day_offset,
		    y_day_offset,COLORLIST[currentcolor] , fb_ptr, finfo.line_length);
       }
       
       sleep(1);
   }
   
   }else {

      printf(" child process (PID: %d) started digitalclock service .\n", pid);
      exit(0);
   }

    munmap(fb_ptr, screen_size);
    close(fb_fd);
    return 0;
}

