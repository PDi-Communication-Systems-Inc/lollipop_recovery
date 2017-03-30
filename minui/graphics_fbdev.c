/*
 * Copyright (C) 2014 The Android Open Source Project
 * Copyright (C) 2015 Freescale Semiconductor, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>

#include <fcntl.h>
#include <stdio.h>

#include <sys/cdefs.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/types.h>

#include <linux/fb.h>
#include <linux/kd.h>

#include "minui.h"
#include "graphics.h"

static gr_surface fbdev_init(minui_backend*);
static gr_surface fbdev_flip(minui_backend*);
static void fbdev_blank(minui_backend*, bool);
static void fbdev_exit(minui_backend*);

static GRSurface gr_framebuffer[2];
static bool double_buffered;
static GRSurface* gr_draw = NULL;
static int displayed_buffer;

static struct fb_var_screeninfo vi;
struct fb_fix_screeninfo fi;
static int fb_fd = -1;

static minui_backend my_backend = {
    .init = fbdev_init,
    .flip = fbdev_flip,
    .blank = fbdev_blank,
    .exit = fbdev_exit,
};

minui_backend* open_fbdev() {
    return &my_backend;
}

static void fbdev_blank(minui_backend* backend __unused, bool blank)
{
    int ret;

    ret = ioctl(fb_fd, FBIOBLANK, blank ? FB_BLANK_POWERDOWN : FB_BLANK_UNBLANK);
    if (ret < 0)
        perror("ioctl(): blank");
}

static void set_displayed_framebuffer(unsigned n)
{
    if (n > 1 || !double_buffered) return;

    vi.yres_virtual = gr_framebuffer[0].height * 2;
    vi.yoffset = n * gr_framebuffer[0].height;
    vi.bits_per_pixel = gr_framebuffer[0].pixel_bytes * 8;
    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        perror("active fb swap failed");
    }
    displayed_buffer = n;
}

static void dump_variable_screeninfo(){
     printf("fb0 reports:\n"
            "  vi.bits_per_pixel = %d\n"
            "  vi.red.offset   = %3d   .length = %3d\n"
            "  vi.green.offset = %3d   .length = %3d\n"
            "  vi.blue.offset  = %3d   .length = %3d\n"
            "  vi.transp.offset = %3d  .length = %3d\n"
            "  vi.pixclock = %6d\n"
            "  vi.xres=%4d vi.yres=%4d\n"
            "  vi.xres_virutal=%4d vi.yres_virtual=%4d\n"
            "  vi.xoffset=%4d vi.yoffset=%4d\n"
            "  vi.nonstd=%3d\n"
            "  vi.activate=%3d\n"
            "  vi.height=%4d mm \n vi.width=%4d mm \n"
            "  vi.left_margin=%4d vi.right_margin=%4d\n"
            "  vi.upper_margin=%4d vi.lower_margin=%4d\n"
            "  vi.hsync_len=%4d vi.vsync_len=%4d\n"
            "  vi.vmode=%3d\n"
            "  vi.rotate=%3d\n",
            vi.bits_per_pixel,
            vi.red.offset, vi.red.length,
            vi.green.offset, vi.green.length,
            vi.blue.offset, vi.blue.length,
            vi.transp.offset, vi.transp.length,
            vi.pixclock,
            vi.xres, vi.yres,
            vi.xres_virtual, vi.yres_virtual,
            vi.xoffset, vi.yoffset,
            vi.nonstd,
            vi.activate,
            vi.height, vi.width,
            vi.left_margin, vi.right_margin,
            vi.upper_margin, vi.lower_margin,
            vi.hsync_len, vi.vsync_len,
            vi.vmode,
            vi.rotate);

       printf("fb0 reports the following fixed info:\n"
            "   id=%s\n"
            "   smem_start=0x%lx\n"
            "   smem_len=%9d\n"
            "   type=%3d\n"
            "   type_aux=%3d\n"
            "   visual=%3d\n"
            "   xpanstep=%3d ypanstep=%3d\n"
            "   ywrapstep=%3d\n"
            "   line_Length=%3d\n"
            "   mmio_start=0x%lx\n"
            "   mmio_len=%9d\n"
            "   accel=%6d\n",
            fi.id,
            fi.smem_start,
            fi.smem_len,
            fi.type,
            fi.type_aux,
            fi.visual,
            fi.xpanstep, fi.ypanstep,
            fi.ywrapstep,
            fi.line_length,
            fi.mmio_start,
            fi.mmio_len,
            fi.accel);
}

static void setup_variable_screeninfo() {

    // for P19 LVDS
#if defined(RECOVERY_BGRA)
    printf("Defining RECOVERY_RGB565\n");
    vi.red.offset     = 11;
    vi.red.length     = 5;
    vi.green.offset   = 5;
    vi.green.length   = 6;
    vi.blue.offset    = 0;
    vi.blue.length    = 5;
    vi.transp.offset  = 0;
    vi.transp.length  = 0;
    vi.bits_per_pixel = 16;
    vi.xres_virtual = vi.xres;
    vi.yres_virtual = vi.yres * 2;
    vi.activate = 0;
    vi.pixclock = 12843;
#elif defined(RECOVERY_RGBX)
    // for P14T2 HDMI
    printf("Defining RECOVERY_RGB8888\n");
    vi.red.offset     = 0;
    vi.red.length     = 8;
    vi.green.offset   = 8;
    vi.green.length   = 8;
    vi.blue.offset    = 16;
    vi.blue.length    = 8;
    vi.transp.offset  = 24;
    vi.transp.length  = 8;
    vi.bits_per_pixel = 32;
    vi.xres_virtual = vi.xres;
    vi.yres_virtual = vi.yres * 2;
    vi.activate = FB_ACTIVATE_NOW;
#else
    printf("Defining else...\n");
    vi.grayscale = 1;
    vi.bits_per_pixel = 8;
    vi.xres_virtual = vi.xres;
    vi.yres_virtual = vi.yres * 2;
    vi.activate = FB_ACTIVATE_NOW;
#endif

    if (ioctl(fb_fd, FBIOPUT_VSCREENINFO, &vi) < 0) {
        perror("setting variable screen info failed");
    }
    else {
        dump_variable_screeninfo();
    }

   return;
}

static gr_surface fbdev_init(minui_backend* backend) {
    void *bits;

    fb_fd = open("/dev/graphics/fb0", O_RDWR);
    if (fb_fd < 0) {
        perror("cannot open fb0");
        return NULL;
    }

    if (ioctl(fb_fd, FBIOGET_FSCREENINFO, &fi) < 0) {
        perror("failed to get fb0 info");
        close(fb_fd);
        return NULL;
    }   

    if (ioctl(fb_fd, FBIOGET_VSCREENINFO, &vi) < 0) {
        perror("failed to get fb0 info");
        close(fb_fd);
        return NULL;
    }

    // We print this out for informational purposes only, but
    // throughout we assume that the framebuffer device uses an RGBX
    // pixel format.  This is the case for every development device I
    // have access to.  For some of those devices (eg, hammerhead aka
    // Nexus 5), FBIOGET_VSCREENINFO *reports* that it wants a
    // different format (XBGR) but actually produces the correct
    // results on the display when you write RGBX.
    //
    // If you have a device that actually *needs* another pixel format
    // (ie, BGRX, or 565), patches welcome...

    setup_variable_screeninfo();
    dump_variable_screeninfo();

    bits = mmap(0, fi.smem_len, PROT_READ | PROT_WRITE, MAP_SHARED, fb_fd, 0);
    if (bits == MAP_FAILED) {
        perror("failed to mmap framebuffer");
        close(fb_fd);
        return NULL;
    }

    memset(bits, 0, fi.smem_len);

    gr_framebuffer[0].width = vi.xres;
    gr_framebuffer[0].height = vi.yres;
    gr_framebuffer[0].row_bytes = fi.line_length;
    gr_framebuffer[0].pixel_bytes = vi.bits_per_pixel / 8;
    gr_framebuffer[0].data = bits;
    memset(gr_framebuffer[0].data, 0, gr_framebuffer[0].height * gr_framebuffer[0].row_bytes);

    /* check if we can use double buffering */
    if (vi.yres * fi.line_length * 2 <= fi.smem_len) {
        printf("Using double buffering\n");
        double_buffered = true;

        memcpy(gr_framebuffer+1, gr_framebuffer, sizeof(GRSurface));
        gr_framebuffer[1].data = gr_framebuffer[0].data +
            gr_framebuffer[0].height * gr_framebuffer[0].row_bytes;

        gr_draw = gr_framebuffer+1;

    } else {
        printf("Not using double buffering, flipping instead\n");
        double_buffered = false;

        // Without double-buffering, we allocate RAM for a buffer to
        // draw in, and then "flipping" the buffer consists of a
        // memcpy from the buffer we allocated to the framebuffer.

        gr_draw = (GRSurface*) malloc(sizeof(GRSurface));
        memcpy(gr_draw, gr_framebuffer, sizeof(GRSurface));
        gr_draw->data = (unsigned char*) malloc(gr_draw->height * gr_draw->row_bytes);
        if (!gr_draw->data) {
            perror("failed to allocate in-memory surface");
            return NULL;
        }
    }

    memset(gr_draw->data, 0, gr_draw->height * gr_draw->row_bytes);
    set_displayed_framebuffer(0);

    printf("framebuffer: %d (%d x %d)\n", fb_fd, gr_draw->width, gr_draw->height);

    fbdev_blank(backend, true);
    fbdev_blank(backend, false);

    return gr_draw;
}

static gr_surface fbdev_flip(minui_backend* backend __unused) {
    if (double_buffered) {
        // Change gr_draw to point to the buffer currently displayed,
        // then flip the driver so we're displaying the other buffer
        // instead.
        gr_draw = gr_framebuffer + displayed_buffer;
        set_displayed_framebuffer(1-displayed_buffer);
    } else {
        // Copy from the in-memory surface to the framebuffer.
        memcpy(gr_framebuffer[0].data, gr_draw->data,
               gr_draw->height * gr_draw->row_bytes);
    }

    setup_variable_screeninfo();
    return gr_draw;
}

static void fbdev_exit(minui_backend* backend __unused) {
    close(fb_fd);
    fb_fd = -1;

    if (!double_buffered && gr_draw) {
        free(gr_draw->data);
        free(gr_draw);
    }
    gr_draw = NULL;
}
