#ifndef HWC_H
#define HWC_H

#define to_ctx(dev) ((hwc_context_t *)dev)

struct fb_ctx_t {
    int id;
    int available;
    int fd;
    int vsyncfd;
    int vsync_on;
    int vsync_stop;
    int vthread_running;
    int fake_vsync;
    pthread_t vthread;
    hwc_procs_t const *hwc_procs;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
};

struct hwc_context_t {
    hwc_composer_device_1_t device;
    const hwc_procs_t* proc;
    int fb0;
    struct fb_var_screeninfo vinfo;
    struct fb_fix_screeninfo finfo;
    float xdpi = ((float)(vinfo.xres) * 25.4f) / (float)(vinfo.width);
    float ydpi = ((float)(vinfo.yres) * 25.4f) / (float)(vinfo.height);
    fb_ctx_t disp[3];
};

#endif //HWC_H
