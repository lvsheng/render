#include <linux/fb.h>
#include <stdio.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

//#include <linux/kd.h>
//#include <linux/vt.h>

//inline uint32_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo)
uint16_t pixel_color(uint8_t r, uint8_t g, uint8_t b, struct fb_var_screeninfo *vinfo)
{
	return (r<<vinfo->red.offset) | (g<<vinfo->green.offset) | (b<<vinfo->blue.offset);
}

int main()
{
	struct fb_fix_screeninfo finfo;
	struct fb_var_screeninfo vinfo;

    //int tty_fd = open("/dev/tty0", O_RDWR);
	//ioctl(tty_fd,KDSETMODE,KD_GRAPHICS);
	//ioctl(tty_fd,KDSETMODE,KD_TEXT);

	int fb_fd = open("/dev/fb0",O_RDWR);

	//Get variable screen information
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);
	vinfo.grayscale=0;
	vinfo.bits_per_pixel=32;
	ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vinfo);
	ioctl(fb_fd, FBIOGET_VSCREENINFO, &vinfo);

	ioctl(fb_fd, FBIOGET_FSCREENINFO, &finfo);
    printf("bits_per_pixel: %d.\n", vinfo.bits_per_pixel); // 16。设置失败

	long screensize = vinfo.yres_virtual * finfo.line_length;

	uint8_t *fbp = mmap(0, screensize, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, (off_t)0);

	int x,y;

	for (x=0;x<vinfo.xres;x++)
		for (y=0;y<vinfo.yres;y++)
		{
            // printf("x: %d, y: %d\n", x, y);
			long location = (x+vinfo.xoffset) * (vinfo.bits_per_pixel/8) + (y+vinfo.yoffset) * finfo.line_length;
			//*((uint32_t*)(fbp + location)) = pixel_color(0xFF,0x00,0xFF, &vinfo);
			*((uint16_t*)(fbp + location)) = pixel_color(0xFF,0x00,0xFF, &vinfo);
		}

	return 0;
}
