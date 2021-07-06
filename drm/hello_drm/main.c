#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>

// 各drm方法与结构体声明可参见这里：
// https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h
#include <xf86drmMode.h>

static const bool logPlanes = true;

//int printModule(int argc, char **argv);

void printResource(int fd, drmModeRes *resource) {
    //if (resource == NULL) {
        // 外界传来的老的resource中信息不会及时更新（比如新申请了fb不会反映）
        resource = drmModeGetResources(fd);
        printf("  got resource: %p\n", resource);
        if (resource == NULL) {
            perror("[modesetting] Could not get DRM device resources. drmModeGetResources");
            return;
        }
    //} else {
    //    printf("printResource(fd, NULL)\n");
    //    printResource(fd, NULL);
    //    printf("~~~~~~~end printResource(fd, NULL)\n");
    //}

    printf("  printResource: fd:%d, resource:%p\n", fd, resource);
    // see: https://manpages.debian.org/testing/libdrm-dev/drmModeGetResources.3.en.html
    printf("  resource: count_fbs:%d, count_crtcs:%d, count_encoders:%d, count_connectors:%d, minW:%d, maxW:%d, minH:%d, maxH:%d\n", 
        resource->count_fbs, resource->count_crtcs, resource->count_encoders, resource->count_connectors, resource->min_width, resource->max_width, resource->min_height, resource->max_height
    );

    // todo: 似乎不能看到其他app申请的fb？(比如开着gui时，初始依然是0）
    printf("    fbs:");
    // allocated framebuffer objects:
    for (int i = 0; i < resource->count_fbs; ++i) {
        printf(" %d", resource->fbs[i]);
    }
    printf("\n");

    // log crtcs info
    for (int i = 0; i < resource->count_crtcs; i++) {
        // https://manpages.debian.org/unstable/libdrm-dev/drm-kms.7.en.html#Planes
        // an abstraction representing a part of the chip that contains a pointer to a scanout buffer
        drmModeCrtcPtr crtc = drmModeGetCrtc(fd, resource->crtcs[i]);
        printf("    crtc[%d]: buffer_id:%d, x:%d, y:%d, w:%d, h:%d, mode_valid:%d, mode->hdisplay:%d, mode->vdisplay:%d, gamma_size:%d\n", 
            i,
            // todo: crtc上似乎能看到其他应用的fb？但ubuntu树莓派双屏幕时，两个crtc的buffer_id相同？
            crtc->buffer_id, // a pointer to some video memory (abstracted as a frame-buffer object). 0 = disconnect
            crtc->x, crtc->y, crtc->width, crtc->height, crtc->mode_valid, crtc->mode.hdisplay, crtc->mode.vdisplay, crtc->gamma_size
        );
    }
}

// from: https://dri-devel.freedesktop.narkive.com/5synpVPO/patch-libdrm-0-2-print-plane-modifiers-in-modeprint
int printBlob(const char *name, void *data, uint32_t length) {
    if (!strcmp(name, "IN_FORMATS")) {
        struct drm_format_modifier_blob *m = data;
        uint32_t *fmts = (uint32_t *)(((char *)data) + m->formats_offset);
        struct drm_format_modifier *mod = (struct drm_format_modifier *)(((char *)data) + m->modifiers_offset);
        int i,j,k;
        // todo: modifiers是啥？？
        printf("            modifiers :\n");
        for (j = 0; j < (int)m->count_modifiers; j++) {
            printf("              0x%016" PRIx64, mod[j].modifier);
            for (i = 0; i < 64 && (i + mod[j].offset) < (int)m->count_formats; i++) {
                if (mod[j].formats & (1<<i)) {
                    k = i + mod[j].offset;
                    printf(" %c%c%c%c", fmts[k] >> 24, fmts[k] >> 16, fmts[k] >> 8, fmts[k] >> 0);
                }
            }
            printf("\n");
        }
    }
    return 0;
}

void printProperty(int fd, drmModePropertyPtr props, uint64_t value) {
    const char *name = NULL;
    drmModePropertyBlobPtr blob = drmModeGetPropertyBlob(fd, value);
    if (blob == NULL) {
        // perror("[modesetting] Could not get blob. drmModeGetPropertyBlob");
        return;
    }
    printf("            blob is %d length, %08X\n", blob->length, *(uint32_t *)blob->data);
    printBlob(props->name, blob->data, blob->length);
    drmModeFreePropertyBlob(blob);
}

void printPlanes(int fd) {
    if (!logPlanes) return;

    printf("  printPlanes: fd:%d\n", fd);
    drmModePlaneRes *plane_res = drmModeGetPlaneResources(fd);
    if (plane_res == NULL) {
        perror("[modesetting] Could not get DRM device planes resources. drmModeGetPlaneResources");
        return;
    }

    // see: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L336
    printf("  drmModePlaneRes count_planes:%d\n", plane_res->count_planes);
    for (int i = 0; i < plane_res->count_planes; i++) {
        drmModePlane *plane = drmModeGetPlane(fd, plane_res->planes[i]);
        if (plane == NULL) {
            perror("[modesetting] Could not get DRM device plane. drmModeGetPlane");
            return;
        }
        printf("    plane[%d]: crtc_x:%d, crtc_y:%d, x:%d, y:%d\n", i, plane->crtc_x, plane->crtc_y, plane->x, plane->y);

        drmModeObjectProperties *props = drmModeObjectGetProperties(fd, plane_res->planes[i], DRM_MODE_OBJECT_PLANE);
        if (props == NULL) {
            perror("[modesetting] Could not get DRM device planes' properties. drmModeObjectGetProperties");
            return;
        }

        for (int j = 0; j < props->count_props; j++) {
            uint32_t prop_id = props->props[j];
            uint64_t prop_value = props->prop_values[j];
            drmModePropertyPtr prop = drmModeGetProperty(fd, prop_id);
            if (prop == NULL) {
                perror("[modesetting] Could not get DRM device planes' properties' info. drmModeGetProperty");
                return;
            }
            // see: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L234
            //   type value: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L311
            printf("        props[%d]:%u %s:%lu\n", j, prop_id, prop->name, prop_value);
            printf("            drmModePropertyRes: count_values:%d, values:", prop->count_values);
            for (int k = 0; k < prop->count_values; k++) {
                printf(" %lu", prop->values[k]);
            }
            printf("\n");
            printProperty(fd, prop, prop_value);
        }
    } // for planes
}

// see: https://github.com/ardera/flutter-pi/blob/master/src/modesetting.c
// see also: https://blog.csdn.net/hexiaolong2009/article/details/83721242
// see also: https://github.com/dvdhrm/docs/tree/master/drm-howto
int main(int argc, char **argv) {
    printf("argc: %d, argv:", argc);
    for (int i = 0; i < argc; ++i) {
        printf(" %s", *(argv + i));
    }
    printf("\n\n");
    //if (argc > 1) {
    //    return printModule(argc, argv);
    //}

    int ok;
    drmDevicePtr devices[64];
    int num_devices = drmGetDevices2(0, devices, sizeof(devices)/sizeof(*devices));
    printf("num_devices:%d\n", num_devices);

    for (int i = 0; i < num_devices; i++) {
        // ## get device
        drmDevicePtr cur_device = devices[i];

        bool primary = cur_device->available_nodes & (1 << DRM_NODE_PRIMARY);
        printf("device%d: available_nodes:%d, primary:%d\n", i, cur_device->available_nodes, primary);
        if (!(cur_device->available_nodes & (1 << DRM_NODE_PRIMARY))) {
            // We need a primary node.
            continue;
        }

        // ## get path
        const char *path = cur_device->nodes[DRM_NODE_PRIMARY];
        printf("  path: %s\n", path);

        // ## get file descriptor
        int fd = open(path, O_RDWR);
        if (fd < 0) {
            perror("[modesetting] Could not open DRM device. open");
            continue;
        }
        printf("  got fd: %d\n", fd);
        //// request master[optional]
        //ok = drmSetMaster(fd);
        //if (ok < 0) {
        //    perror("[modesetting] Could not requests master controls for a DRM device. drmSetMaster");
        //    continue;
        //}

        // ## expose all planes (overlay, primary, and cursor) to userspace
        // see: https://dri.freedesktop.org/docs/drm/gpu/drm-uapi.html#c.DRM_CLIENT_CAP_UNIVERSAL_PLANES
        ok = drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
        if (ok < 0) {
            perror("[modesetting] Could not set DRM client universal planes capable. drmSetClientCap");
            continue;
        }

        // ## get resource
        drmModeRes *resource = drmModeGetResources(fd);
        printf("  got resource: %p\n", resource);
        if (resource == NULL) {
            perror("[modesetting] Could not get DRM device resources. drmModeGetResources");
            continue;
        }
        printf("------initial resource:------\n");
        printResource(fd, resource);
        printf("-----------------------------\n");

        for (int i_connector = 0; i_connector < resource->count_connectors; i_connector++) {
            // ## get connector
            drmModeConnector *cur_connector = drmModeGetConnector(fd, resource->connectors[i_connector]);
            if (cur_connector == NULL) {
                perror("[modesetting] Could not get DRM device connector. drmModeGetConnector");
                continue;
            }
            if (cur_connector->count_modes == 0) {
                printf("[modesetting] cur_connector->count_modes is 0.\n");
                continue;
            }
            //drmModeModeInfoPtr selected_mode = &cur_connector->modes[0];
            drmModeModeInfoPtr selected_mode = &cur_connector->modes[cur_connector->count_modes - 1];
            // see: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L291
            printf("  connector[%d]:%d encoder_id:%d mmWidth:%d, mmHeight:%d, subpixel:%d, count_modes:%d, selected_mode.hdisplay:%d, selected_mode.vdisplay:%d\n%s\n",
                i_connector, cur_connector->connector_id, cur_connector->encoder_id, cur_connector->mmWidth, cur_connector->mmHeight, cur_connector->subpixel, cur_connector->count_modes, selected_mode->hdisplay, selected_mode->vdisplay,
                "        drmModeSubPixel:1-UNKNOWN,2-HORIZONTAL_RGB,3-HORIZONTAL_BGR,4-VERTICAL_RGB,5-VERTICAL_BGR,6-NONE"
            );
            printf("    encoders:");
            for (int i_encoder = 0; i_encoder < cur_connector->count_encoders; i_encoder++)
                printf(" %u", cur_connector->encoders[i_encoder]);
            printf("\n");
            for (int i_mode = 0; i_mode < cur_connector->count_modes; i_mode++)
                printf("    mode[%d]: hdisplay:%d,vdisplay:%d \tname:%s\n", i_mode, cur_connector->modes[i_mode].hdisplay, cur_connector->modes[i_mode].vdisplay, cur_connector->modes[i_mode].name);

            // ## create dumb buffer
            // see: https://manpages.debian.org/jessie/libdrm-dev/drm-memory.7.en.html#Dumb-Buffers
            const int bpp = 32;
            struct drm_mode_create_dumb create_req = {};
            create_req.width = selected_mode->hdisplay;
            create_req.height = selected_mode->vdisplay;
            create_req.bpp = bpp; // bits-per-pixel, must be a multiple of 8
            //create_req.width = 1024 * 1024 * 110 * 8l / bpp / create_req.height; // Cannot allocate memory (but 215M is ok)
            ok = ioctl(fd, DRM_IOCTL_MODE_CREATE_DUMB, &create_req);
            if (ok < 0) {
                perror("[compositor] Could not create a dumb buffer. ioctl");
                continue;
            }
            // see: https://manpages.debian.org/jessie/libdrm-dev/drm-memory.7.en.html - drm_mode_create_dumb
            printf("  drm_mode_create_dumb: handle:%d, pitch:%d, Pixels Per Row:%d, size:%lld\n", 
                create_req.handle, // gem handle that identifies the buffer
                // the pitch (or stride) of the new buffer, Most drivers use 32bit or 64bit aligned stride-values.
                // see also: https://jsandler18.github.io/extra/framebuffer.html - Pitch
                create_req.pitch, create_req.pitch * 8 / create_req.bpp,
                create_req.size // the absolute size in bytes of the buffer. This can normally also be computed with (height * pitch + width) * bpp / 4.
            );

            // ## create framebuffer
            // see: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L364
            //     Creates a new framebuffer with an buffer object as its scanout buffer.
            // see: https://docs.nvidia.com/drive/nvvib_docs/NVIDIA%20DRIVE%20Linux%20SDK%20Development%20Guide/baggage/group__direct__rendering__manager.html#ga2b6953f6bc86c2fd4851690038b7b52f
            //     creates a framebuffer with a specified size and format, using the specified buffer object as the memory backing store. The buffer object can be a "dumb buffer" created by a call to drmIoctl with the request parameter set to DRM_IOCTL_MODE_CREATE_DUMB, or it can be a dma-buf imported by a call to the drmPrimeFDToHandle function.
            uint32_t fb_id;
            //printf("before drmModeAddFB\n");
            //printResource(fd, resource);
            ok = drmModeAddFB(fd, create_req.width, create_req.height, 
                32, // depth. todo-Q: 与bpp的区别？不一定是8的倍数，可能小于bpp？ 如man的例子中就是24而bpp是32：https://manpages.debian.org/jessie/libdrm-dev/drm-memory.7.en.html
                create_req.bpp, 
                create_req.pitch,
                create_req.handle, // A handle for a buffer object to provide memory backin
                &fb_id
            );
            //printf("after drmModeAddFB\n");
            printf("------resource after drmModeAddFB:------\n");
            printResource(fd, resource);
            printf("----------------------------------------\n");
            if (ok < 0) {
                perror("[compositor] Could not make a DRM FB. drmModeAddFB");
                continue;
            }

            // ## retrieve the offset for mmap
            struct drm_mode_map_dumb map_req = {};
            map_req.handle = create_req.handle;
            ok = ioctl(fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req);
            if (ok < 0) {
                perror("[compositor] Could not prepare dumb buffer mmap. ioctl");
                continue;
            }
            printf("  drm_mode_map_dumb: offset:%lld\n", map_req.offset);

            // ## mmap
            // see: https://en.wikipedia.org/wiki/Mmap
            uint32_t *buffer;
            buffer = mmap(0, create_req.size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, map_req.offset);
            if (buffer == MAP_FAILED) {
                perror("[compositor] Could not mmap dumb buffer. mmap");
                continue;
            }

            // ## set pixels
            memset(buffer, 0xFF, create_req.size);
            for (int y = 0; y < create_req.height; ++y) {
                for (int x = 0; x < create_req.width; ++x) {
                    long location = x * (create_req.bpp / 8) + y * create_req.pitch;
                    // printf("x: %d, y: %d, location: %lld\n", x, y, location);
                    if (x % create_req.height == y || create_req.height - x % create_req.height == y)
                        *((uint32_t*)((char*)buffer + location)) = 0xFFFF00FF;
                }
            }

            // ## Sets a CRTC configuration (display)
            // see: https://docs.nvidia.com/drive/nvvib_docs/NVIDIA%20DRIVE%20Linux%20SDK%20Development%20Guide/baggage/group__direct__rendering__manager.html#gaca6c143ee52d6cd6b6f7cf1522be4c08
            // see: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L404
            //     Set the mode on a crtc crtcId with the given mode modeId.
            // see: https://manpages.debian.org/jessie/libdrm-dev/drm-kms.7.en.html#Mode-Setting
            // 注意会有撕裂(无double-buffer)
            //     double-buffer示例：
            //       https://blog.csdn.net/hexiaolong2009/article/details/84452020
            //       https://github.com/dvdhrm/docs/blob/master/drm-howto/modeset-double-buffered.c#L520
            //     vsync示例：
            //       https://github.com/dvdhrm/docs/blob/master/drm-howto/modeset-vsync.c#L670
            ok = drmModeSetCrtc(
                fd, // fd
                // see: https://manpages.debian.org/testing/libdrm-dev/drmModeGetResources.3.en.html
                // A CRTC is simply an object that can scan out a framebuffer to a display sink, and contains mode timing and relative position information. CRTCs drive encoders, which are responsible for converting the pixel stream into a specific display protocol (e.g., MIPI or HDMI).
                resource->crtcs[i_connector], // crtcId
                fb_id, // bufferId
                0, // x
                0, // y
                &cur_connector->connector_id, // connectors
                1, // count
                selected_mode // mode. 注意会改变printResource中connector的mode
            );
            printf("------resource after drmModeSetCrtc:------\n");
            printResource(fd, resource);
            printf("------------------------------------------\n");
            if (ok < 0) {
                perror("[modesetting] Could not set CRTC mode and framebuffer. drmModeSetCrtc"); // 在x11启动情况下会Permission denied (ctrl+alt+F3 or `sudo service gdm3 stop`)
                // continue;
            }
            printf("  type anything...\n");
            getchar();
            printf("  done\n");
        } // for connector

        printPlanes(fd);
    } // for devices

    // 本程序结束后，/dev/fb0的操作也不能及时生效，要ctrl-alt-f2再ctrl-alt-f3才能更新
    // todo: 是没有主动释放资源的原因？
}

