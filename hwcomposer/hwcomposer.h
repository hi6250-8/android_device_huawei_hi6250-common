#ifndef HWC_H
#define HWC_H

#define to_ctx(dev) ((hwc_context_t *)dev)

uint32_t hwcApiVersion(const hwc_composer_device_1_t* hwc);
uint32_t hwcHeaderVersion(const hwc_composer_device_1_t* hwc);
bool hwcHasApiVersion(const hwc_composer_device_1_t* hwc, uint32_t version);

class KirinPrimaryDisplay;
class KirinExternalDisplay;
class KirinVirtualDisplay;

class KirinDisplay {
    public:
        /* Methods */
        KirinDisplay(int numMPPs);
        KirinDisplay(uint32_t type, struct kirin_hwc_composer_device_1_t *pdev);
        virtual ~KirinDisplay();
};

class KirinExternalDisplay : public KirinDisplay {
    public:
        /* Methods */
        KirinExternalDisplay(struct kirin_hwc_composer_device_1_t *pdev);
        ~KirinExternalDisplay();

        virtual int getActiveConfig();
        virtual int setActiveConfig(int index);
};

struct kirin_hwc_composer_device_1_t;

struct kirin_hwc_composer_device_1_t {
    hwc_composer_device_1_t base;
    KirinExternalDisplay    *externalDisplay;
    bool hdmi_hpd;
};

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
    fb_ctx_t disp[1];
};

#endif //HWC_H
