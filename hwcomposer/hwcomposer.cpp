/*
 * Copyright (c) 2017 Jonathan Dennis [Meticulus] theonejohnnyd@gmail.com
 * Copyright (c) 2021 The LineageOS Project
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <hardware/hardware.h>
#include <utils/ThreadDefs.h>
#include <utils/Timers.h>
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <malloc.h>
#include <sys/resource.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <sys/ioctl.h>
#define LOG_TAG "hwcomposer"
#include <cutils/log.h>
#include <cutils/atomic.h>
#include <utils/Trace.h>

#include <hardware/hwcomposer.h>

#include <EGL/egl.h>

#include "hisi_dss.h"
#include "hwcomposer.h"
/*****************************************************************************/
#define FAKE_VSYNC

#define NSEC_PER_SEC    1000000000L
#define NSEC_PER_MSEC   1000000L

#define FB0_FILE "/dev/graphics/fb0"
#define FB1_FILE "/dev/graphics/fb1"
#define FB2_FILE "/dev/graphics/fb2"
#define TIMESTAMP_FILE "/sys/devices/virtual/graphics/fb0/vsync_timestamp"

//#define DEBUG

#ifdef DEBUG
#define DEBUG_LOG(x...) ALOGD(x)
#else
#define DEBUG_LOG(x...) do {} while(0)
#endif

static void write_string(const char * path, const char * value) {
    int fd = open(path, O_WRONLY);
	if(!fd) { ALOGE("Unable to open to %s", path); return;}

	unsigned long bytes_written = write(fd, value, strlen(value));

	if (bytes_written < 1 || bytes_written < strlen(value)) {
		ALOGE("Unable to write to %s : %lu",path, bytes_written);
	}

    close(fd);
}

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device);

static struct hw_module_methods_t hwc_module_methods = {
    .open = hwc_device_open
};

hwc_module_t HAL_MODULE_INFO_SYM = {
    .common = {
        .tag = HARDWARE_MODULE_TAG,
        .version_major = 1,
        .version_minor = 0,
        .id = HWC_HARDWARE_MODULE_ID,
        .name = "Kirin Hardware Composer",
        .author = "Meticulus",
        .methods = &hwc_module_methods,
    }
};

/*****************************************************************************/

uint32_t hwcApiVersion(const hwc_composer_device_1_t* hwc) {
    uint32_t hwcVersion = hwc->common.version;
    return hwcVersion & HARDWARE_API_VERSION_2_MAJ_MIN_MASK;
}

uint32_t hwcHeaderVersion(const hwc_composer_device_1_t* hwc) {
    uint32_t hwcVersion = hwc->common.version;
    return hwcVersion & HARDWARE_API_VERSION_2_HEADER_MASK;
}

bool hwcHasApiVersion(const hwc_composer_device_1_t* hwc,
        uint32_t version) {
    return hwcApiVersion(hwc) >= (version & HARDWARE_API_VERSION_2_MAJ_MIN_MASK);
}

static void dump_layer(hwc_layer_1_t const* l) {
    DEBUG_LOG("\ttype=%d, flags=%08x, handle=%p, tr=%02x, blend=%04x, {%d,%d,%d,%d}, {%d,%d,%d,%d}",
            l->compositionType, l->flags, l->handle, l->transform, l->blending,
            l->sourceCrop.left,
            l->sourceCrop.top,
            l->sourceCrop.right,
            l->sourceCrop.bottom,
            l->displayFrame.left,
            l->displayFrame.top,
            l->displayFrame.right,
            l->displayFrame.bottom);
}

static int hwc_prepare(hwc_composer_device_1_t *,
        size_t numDisplays, hwc_display_contents_1_t** displays) {
    for(size_t j=0; j<numDisplays; j++) {
	if (displays && (displays[j]->flags & HWC_GEOMETRY_CHANGED)) {
	    for (size_t i=0 ; i<displays[j]->numHwLayers ; i++) {
		displays[j]->hwLayers[i].compositionType = HWC_FRAMEBUFFER;
            }
	}
    }
    return 0;
}

static int hwc_set(hwc_composer_device_1_t *,
        size_t numDisplays, hwc_display_contents_1_t** displays)
{
    /*for (size_t i=0 ; i<list->numHwLayers ; i++) {
        dump_layer(&list->hwLayers[i]);
    }*/
    for(size_t j=0; j<numDisplays; j++) {
	EGLBoolean sucess = eglSwapBuffers((EGLDisplay)displays[j]->dpy,
		(EGLSurface)displays[j]->sur);
	if (!sucess) {
	    return HWC_EGL_ERROR;
	}
    }
    return 0;
}

static void * vsync_thread(void * arg) {
   setpriority(PRIO_PROCESS, 0, HAL_PRIORITY_URGENT_DISPLAY +
                android::PRIORITY_MORE_FAVORABLE);
   fb_ctx_t *fb = (fb_ctx_t *) arg;
   DEBUG_LOG("vsync thread enter id = %d fake = %d", fb->id, fb->fake_vsync);
   fb->vthread_running = 1;
   int retval = -1;
   int64_t timestamp = 0;
   char read_result[20];
   if(fb->fake_vsync) {
	while(true) {
	    if(!fb->vsync_on) {
		DEBUG_LOG("vsync thread sleeping id = %d fake = 1", fb->id);
		while(!fb->vsync_on && !fb->vsync_stop)
		    pthread_cond_wait(&fb->cond,&fb->mutex);
		DEBUG_LOG("vsync thread woke up id = %d fake = 1", fb->id);
		if(fb->vsync_stop)
		    break;
		if(!fb->vsync_on)
		    continue;
	    }
	    timestamp = systemTime();
	    fb->hwc_procs->vsync(fb->hwc_procs,fb->id,timestamp);
	    usleep(16666);
	}
   } else {
	while(true) {
	    if(!fb->vsync_on) {
		DEBUG_LOG("vsync thread sleeping id = %d fake = 0", fb->id);
		while(!fb->vsync_on && !fb->vsync_stop)
		    pthread_cond_wait(&fb->cond,&fb->mutex);
		DEBUG_LOG("vsync thread woke up id = %d fake = 0", fb->id);
		if(fb->vsync_stop)
		    break;
		if(!fb->vsync_on)
		    continue;
	    }
	    if(pread(fb->vsyncfd,read_result,20,0)) {
		timestamp = atol(read_result);
		fb->hwc_procs->vsync(fb->hwc_procs, fb->id, timestamp);
		usleep(16666);
	    } else { goto error; }
       }
   }

   retval = 0;
error:
   fb->vthread_running = 0;
   DEBUG_LOG("vsync thread exit");
   return NULL;

}

static void signal_vsync_thread(fb_ctx_t *fb) {
    if(fb->vthread_running) {
	pthread_mutex_lock(&fb->mutex);
	pthread_cond_signal(&fb->cond);
	pthread_mutex_unlock(&fb->mutex);
    }
}

static void start_vsync_thread(fb_ctx_t *fb) {

    if(!fb->fake_vsync)
	ioctl(fb->fd,HISIFB_VSYNC_CTRL, &fb->vsync_on);

    if(!fb->vthread_running) {
	pthread_create(&fb->vthread,NULL,&vsync_thread, fb);
    } else {
	signal_vsync_thread(fb);
    }
}

static int hwc_event_control (struct hwc_composer_device_1* dev, int disp,
            int event, int enabled) {
    if(event == HWC_EVENT_VSYNC) {
	struct hwc_context_t *context = (hwc_context_t *)dev;

	    if(!context->disp[disp].available)
		return -EINVAL;
	    context->disp[disp].vsync_on = enabled;
	    if(!context->disp[disp].vthread_running && enabled)
		start_vsync_thread(&context->disp[disp]);
    }
    return 0;

}

static int hwc_getDisplayConfigs(struct hwc_composer_device_1* dev, int disp,
    uint32_t* configs, size_t* numConfigs) {

    int ret = 0;
    hwc_context_t* ctx __unused = to_ctx(dev);

    if (*numConfigs == 0)
	return 0;

    switch(disp) {
	case HWC_DISPLAY_PRIMARY:
	    configs[0] = 0;
	    *numConfigs = 1;

	ret = 0;
	break;
	case HWC_DISPLAY_EXTERNAL:
	    ret = -1;
	break;
    }
    return ret;
}

static int hwc_getDisplayAttributes(struct hwc_composer_device_1* dev, int disp __unused,
    uint32_t config __unused, const uint32_t* attributes, int32_t* values) {

    hwc_context_t* ctx = to_ctx(dev);

    static const uint32_t DISPLAY_ATTRIBUTES[] = {
	HWC_DISPLAY_VSYNC_PERIOD,
	HWC_DISPLAY_WIDTH,
	HWC_DISPLAY_HEIGHT,
	HWC_DISPLAY_DPI_X,
	HWC_DISPLAY_DPI_Y,
	HWC_DISPLAY_NO_ATTRIBUTE,
	};

    const int NUM_DISPLAY_ATTRIBUTES = (sizeof(DISPLAY_ATTRIBUTES) / sizeof(DISPLAY_ATTRIBUTES)[0]);

    for (size_t i = 0; i < NUM_DISPLAY_ATTRIBUTES -1; ++i) {
	switch (attributes[i]) {
	case HWC_DISPLAY_VSYNC_PERIOD:
	    values[i] = ctx->vinfo.vsync_len;
	    break;
	case HWC_DISPLAY_WIDTH:
	    values[i] = ctx->vinfo.xres;
	    break;
	case HWC_DISPLAY_HEIGHT:
	    values[i] = ctx->vinfo.yres;
	    break;
	case HWC_DISPLAY_DPI_X:
	    values[i] = (int32_t) (ctx->xdpi * 1000);
	    break;
	case HWC_DISPLAY_DPI_Y:
	    values[i] = (int32_t) (ctx->ydpi * 1000);
	    break;
	default:
	    ALOGE("Unknown display attr: %d", attributes[i]);
	    return  -EINVAL;
	}
    }
    return 0;
}

int hwc_getActiveConfig(struct hwc_composer_device_1* dev, int disp)
{
    struct kirin_hwc_composer_device_1_t *pdev =
                   (struct kirin_hwc_composer_device_1_t *)dev;
    if (disp == HWC_DISPLAY_PRIMARY)
        return 0;
    else if (disp == HWC_DISPLAY_EXTERNAL) {
        if (pdev->hdmi_hpd) {
                return  pdev->externalDisplay->getActiveConfig();
        } else {
            ALOGE("%s::External device is not connected", __func__);
            return -1;
        }
    } else if (disp == HWC_DISPLAY_VIRTUAL)
        return 0;
    else {
        ALOGE("%s:: unknown display type %u", __func__, disp);
        return -EINVAL;
    }
}

int hwc_setActiveConfig(struct hwc_composer_device_1* dev, int disp, int index)
{
    struct kirin_hwc_composer_device_1_t *pdev =
                   (struct kirin_hwc_composer_device_1_t *)dev;
    ALOGI("%s:: disp(%d), index(%d)", __func__, disp, index);
    if (disp == HWC_DISPLAY_PRIMARY) {
        if (index != 0) {
            ALOGE("%s::Primary display doen't support index(%d)", __func__, index);
            return -1;
        }
        return 0;
    }
    else if (disp == HWC_DISPLAY_EXTERNAL) {
        if (pdev->hdmi_hpd) {
            if (hwcHasApiVersion((hwc_composer_device_1_t*)dev, HWC_DEVICE_API_VERSION_1_4)) {
                return pdev->externalDisplay->setActiveConfig(index);
            } else {
                if (index != 0) {
                    ALOGE("%s::External display doen't support index(%d)", __func__, index);
                    return -1;
                } else {
                    return 0;
                }
            }
        } else {
            ALOGE("%s::External device is not connected", __func__);
            return -1;
        }
    } else if (disp == HWC_DISPLAY_VIRTUAL)
        return 0;

    return -1;
}

int hwc_setCursorPositionAsync(struct hwc_composer_device_1 *dev __unused, int disp __unused, int x_pos __unused, int y_pos __unused)
{
    return 0;
}

static int hwc_setPowerMode(struct hwc_composer_device_1* dev, int disp __unused, int mode)
{
    ATRACE_CALL();
    hwc_context_t* ctx = to_ctx(dev);
    int ret = 0, value = 0;
    ALOGD("%s: Setting mode %d", __func__, mode);
    switch(mode) {
	case HWC_POWER_MODE_OFF:
	    value = FB_BLANK_POWERDOWN;
	    break;
	case HWC_POWER_MODE_DOZE:
	case HWC_POWER_MODE_DOZE_SUSPEND:
	    value = FB_BLANK_VSYNC_SUSPEND;
	    break;
	case HWC_POWER_MODE_NORMAL:
	    value = FB_BLANK_UNBLANK;
	    break;
    }
    if(ioctl(ctx->fb0, FBIOBLANK, value) < 0) {
	ALOGE("%s: ioctl FBIOBLANK failed: %s %d", __func__, strerror(errno), value);
	return -errno;
    }
 return ret;
}

static void hwc_register_procs(struct hwc_composer_device_1* dev,
            hwc_procs_t const* procs) {
    struct hwc_context_t *context = (hwc_context_t *)dev;

    context->disp[HWC_DISPLAY_PRIMARY].hwc_procs = procs;
    context->disp[HWC_DISPLAY_EXTERNAL].hwc_procs = procs;
    context->disp[HWC_DISPLAY_VIRTUAL].hwc_procs = procs;
    DEBUG_LOG("procs registered");
}

static int hwc_query(struct hwc_composer_device_1* dev, int what, int *value)
{
    hwc_context_t *ctx __unused = to_ctx(dev);
    int refreshRate = 60;
    switch (what) {
    case HWC_BACKGROUND_LAYER_SUPPORTED:
	value[0] = 1;
	break;
    case HWC_VSYNC_PERIOD:
	*value = 1000000000 / refreshRate;
	break;
    case HWC_DISPLAY_TYPES_SUPPORTED:
	*value = HWC_DISPLAY_PRIMARY_BIT;//Should be fine by now
	break;
    default:
	return -EINVAL; //Unsupported
    }
    return 0;
}

static int hwc_device_close(struct hw_device_t *dev)
{
    struct hwc_context_t* ctx = (struct hwc_context_t*)dev;
    if (ctx) {
        free(ctx);
    }
    return 0;
}

/*****************************************************************************/

static int hwc_device_open(const struct hw_module_t* module, const char* name,
        struct hw_device_t** device)
{
    int status = 0;
    if (!strcmp(name, HWC_HARDWARE_COMPOSER)) {
        struct hwc_context_t *dev;
        dev = (hwc_context_t*)malloc(sizeof(*dev));

        /* initialize our state here */
        memset(dev, 0, sizeof(*dev));

        /* initialize the procs */
        dev->device.common.tag = HARDWARE_DEVICE_TAG;
        dev->device.common.version = HWC_DEVICE_API_VERSION_1_4;
        dev->device.common.module = const_cast<hw_module_t*>(module);
        dev->device.common.close = hwc_device_close;

        dev->device.prepare = hwc_prepare;
        dev->device.set = hwc_set;
//        dev->device.blank = hwc_blank;
        dev->device.eventControl = hwc_event_control;
        dev->device.registerProcs = hwc_register_procs;
        dev->device.getDisplayConfigs = hwc_getDisplayConfigs;
        dev->device.getDisplayAttributes = hwc_getDisplayAttributes;
        dev->device.getActiveConfig = hwc_getActiveConfig;
        dev->device.setActiveConfig = hwc_setActiveConfig;
        dev->device.setCursorPositionAsync = hwc_setCursorPositionAsync;
        dev->device.setPowerMode = hwc_setPowerMode;
        dev->device.query = hwc_query;

	/* init primary display */
	dev->disp[HWC_DISPLAY_PRIMARY].vthread_running = 0;
	dev->disp[HWC_DISPLAY_PRIMARY].vsync_on = 0;
	dev->disp[HWC_DISPLAY_PRIMARY].vsync_stop = 0;
	dev->disp[HWC_DISPLAY_PRIMARY].fake_vsync = 0;
	dev->disp[HWC_DISPLAY_PRIMARY].id = HWC_DISPLAY_PRIMARY;
        dev->disp[HWC_DISPLAY_PRIMARY].mutex = PTHREAD_MUTEX_INITIALIZER;
        dev->disp[HWC_DISPLAY_PRIMARY].cond = PTHREAD_COND_INITIALIZER;
	dev->disp[HWC_DISPLAY_PRIMARY].fd = open (FB0_FILE, O_WRONLY);
	if(dev->disp[HWC_DISPLAY_PRIMARY].fd < 0) {
	    ALOGE("Could not open fb0 file!");
	    status = -EINVAL;
   	}
	dev->disp[HWC_DISPLAY_PRIMARY].available = 1;
	dev->disp[HWC_DISPLAY_PRIMARY].vsyncfd = open(TIMESTAMP_FILE, O_RDONLY);
	if(dev->disp[HWC_DISPLAY_PRIMARY].vsyncfd < 0) {
	    ALOGW("Using fake vsync on primary...");
	    dev->disp[HWC_DISPLAY_PRIMARY].fake_vsync = 1;
	}
	/* init external physical display */
	dev->disp[HWC_DISPLAY_EXTERNAL].available = 1;
	dev->disp[HWC_DISPLAY_EXTERNAL].id = HWC_DISPLAY_EXTERNAL;
        dev->disp[HWC_DISPLAY_EXTERNAL].mutex = PTHREAD_MUTEX_INITIALIZER;
        dev->disp[HWC_DISPLAY_EXTERNAL].cond = PTHREAD_COND_INITIALIZER;
	dev->disp[HWC_DISPLAY_EXTERNAL].fd = open (FB1_FILE, O_WRONLY);
	if(dev->disp[HWC_DISPLAY_EXTERNAL].fd < 0) {
	    ALOGW("Could not open fb1 file!");
	    ALOGW("External physicals display will be unavailable.");
	    dev->disp[HWC_DISPLAY_EXTERNAL].available = 0;
	}
	dev->disp[HWC_DISPLAY_EXTERNAL].fake_vsync = 1;
	dev->disp[HWC_DISPLAY_EXTERNAL].vsync_on = 0;
	dev->disp[HWC_DISPLAY_EXTERNAL].vsync_stop = 0;
	/* init virtual displays */
	dev->disp[HWC_DISPLAY_VIRTUAL].available = 1;
	dev->disp[HWC_DISPLAY_VIRTUAL].id = HWC_DISPLAY_VIRTUAL;
        dev->disp[HWC_DISPLAY_VIRTUAL].mutex = PTHREAD_MUTEX_INITIALIZER;
        dev->disp[HWC_DISPLAY_VIRTUAL].cond = PTHREAD_COND_INITIALIZER;
	dev->disp[HWC_DISPLAY_VIRTUAL].fd = open (FB2_FILE, O_WRONLY);
	if(dev->disp[HWC_DISPLAY_VIRTUAL].fd < 0) {
	    ALOGE("Could not open fb2 file!");
	    ALOGW("Virtual displays will be unavailable.");
	    dev->disp[HWC_DISPLAY_VIRTUAL].available = 0;
   	}
	dev->disp[HWC_DISPLAY_VIRTUAL].fake_vsync = 1;
	dev->disp[HWC_DISPLAY_VIRTUAL].vsync_stop = 0;
	dev->disp[HWC_DISPLAY_VIRTUAL].vsync_on = 0;
        *device = &dev->device.common;
    }
    return status;
}
