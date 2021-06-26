#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <fcntl.h>
#include <linux/fb.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

int main(){
    int fb_fd = 0;
    // Open the file for reading and writing
    fb_fd = open("/dev/fb0", O_RDWR);
    if (fb_fd == -1) {
        perror("Error: cannot open framebuffer device");
        exit(1);
    }
    printf("The framebuffer device was opened successfully.\n");

    struct fb_fix_screeninfo finfo;
    struct fb_var_screeninfo vinfo; 

    //Get variable screen information
    ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

    //Get fixed screen information
    ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);

    printf("vinfo.bits_per-pixel: %d\n",vinfo.bits_per_pixel);

    close(fb_fd);
    return(0);
}
