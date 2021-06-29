#include <stdio.h>
#include <stdbool.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <drm/drm.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

// see: https://github.com/ardera/flutter-pi/blob/master/src/modesetting.c
// see also: https://blog.csdn.net/hexiaolong2009/article/details/83721242
// see also: https://github.com/dvdhrm/docs/tree/master/drm-howto
int main()
{
    int ok;
    drmDevicePtr devices[64];
    int num_devices = drmGetDevices2(0, devices, sizeof(devices)/sizeof(*devices));
    printf("num_devices:%d\n", num_devices);

	for (int i = 0; i < num_devices; i++) {
        // ## get device
		drmDevicePtr device = devices[i];

        bool primary = device->available_nodes & (1 << DRM_NODE_PRIMARY);
        printf("device%d: available_nodes:%d, primary:%d\n", i, device->available_nodes, primary);
		if (!(device->available_nodes & (1 << DRM_NODE_PRIMARY))) {
			// We need a primary node.
			continue;
		}

        // ## get path
        const char *path = device->nodes[DRM_NODE_PRIMARY];
        printf("  path: %s\n", path);

        // ## get file descriptor
        int fd = open(path, O_RDWR);
        if (fd < 0) {
            perror("[modesetting] Could not open DRM device. open");
            continue;
        }

        //// ## expose all planes (overlay, primary, and cursor) to userspace
        //// see: https://dri.freedesktop.org/docs/drm/gpu/drm-uapi.html#c.DRM_CLIENT_CAP_UNIVERSAL_PLANES
        //ok = drmSetClientCap(fd, DRM_CLIENT_CAP_UNIVERSAL_PLANES, 1);
        //if (ok < 0) {
        //    perror("[modesetting] Could not set DRM client universal planes capable. drmSetClientCap");
        //    continue;
        //}

        // ## get resource
        drmModeRes *resource = drmModeGetResources(fd);
        if (resource == NULL) {
            perror("[modesetting] Could not get DRM device resources. drmModeGetResources");
            continue;
        }
        // see: https://manpages.debian.org/testing/libdrm-dev/drmModeGetResources.3.en.html
        printf("  resource: count_fbs:%d, count_crtcs:%d, count_encoders:%d, count_connectors:%d, minW:%d, maxW:%d, minH:%d, maxH:%d\n", 
            resource->count_fbs, resource->count_crtcs, resource->count_encoders, resource->count_connectors, resource->min_width, resource->max_width, resource->min_height, resource->max_height
        );

        // ## get connector
        drmModeConnector *selected_connector = drmModeGetConnector(fd, resource->connectors[0]);
        if (selected_connector == NULL) {
            perror("[modesetting] Could not get DRM device connector. drmModeGetConnector");
            continue;
        }
        drmModeModeInfoPtr selected_mode = &selected_connector->modes[0];
        // see: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L291
        printf("  connector: mmWidth:%d, mmHeight:%d, subpixel:%d, count_modes:%d, mode[0].hdisplay:%d, mode[0].vdisplay:%d\n%s\n", 
            selected_connector->mmWidth, selected_connector->mmHeight, selected_connector->subpixel, selected_connector->count_modes, selected_mode->hdisplay, selected_mode->vdisplay,
            "    drmModeSubPixel:1-UNKNOWN,2-HORIZONTAL_RGB,3-HORIZONTAL_BGR,4-VERTICAL_RGB,5-VERTICAL_BGR,6-NONE"
        );
        for (int i_m = 0; i_m < selected_connector->count_modes; i_m++)
            printf("    mode[%d]: hdisplay:%d,vdisplay:%d \tname:%s\n", i_m, selected_connector->modes[i_m].hdisplay, selected_connector->modes[i_m].vdisplay, selected_connector->modes[i_m].name);

        // ## create dump buffer
        // see: https://manpages.debian.org/jessie/libdrm-dev/drm-memory.7.en.html#Dumb-Buffers
        const int bpp = 32;
        struct drm_mode_create_dumb create_req = {};
        create_req.width = selected_mode->hdisplay;
        create_req.height = selected_mode->vdisplay;
        create_req.bpp = bpp; // bits-per-pixel, must be a multiple of 8
        //create_req.width = 1024 * 1024 * 216 * 8l / bpp / create_req.height; // Cannot allocate memory (but 215M is ok)
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
        ok = drmModeAddFB(fd, create_req.width, create_req.height, 
            32, // depth. todo-Q: 与bpp的区别？不一定是8的倍数，可能小于bpp？ 如man的例子中就是24而bpp是32：https://manpages.debian.org/jessie/libdrm-dev/drm-memory.7.en.html
            create_req.bpp, 
            create_req.pitch,
            create_req.handle, // A handle for a buffer object to provide memory backin
            &fb_id
        );
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
                    *((uint32_t*)((char*)buffer + location)) = 0xFFFF0000;
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
            resource->crtcs[0], // crtcId
            fb_id, // bufferId
            0, // x
            0, // y
            &selected_connector->connector_id, // connectors
            1, // count
            selected_mode // mode
        );
        if (ok < 0) {
            perror("[modesetting] Could not set CRTC mode and framebuffer. drmModeSetCrtc"); // 在x11启动情况下会Permission denied (ctrl+alt+F3 or `sudo service gdm3 stop`)
            continue;
        }
        printf("  type anything...\n");
        getchar();
        printf("  done\n");


        //// ## Planes信息
        //drmModePlaneRes *plane_res = drmModeGetPlaneResources(fd);
        //if (plane_res == NULL) {
        //    perror("[modesetting] Could not get DRM device planes resources. drmModeGetPlaneResources");
        //    continue;
        //}
        //// see: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L336
        //printf("  drmModePlaneRes count_planes:%d\n", plane_res->count_planes);
        //for (int i = 0; i < plane_res->count_planes; i++) {
        //    drmModePlane *plane = drmModeGetPlane(fd, plane_res->planes[i]);
        //    if (plane == NULL) {
        //        perror("[modesetting] Could not get DRM device plane. drmModeGetPlane");
        //        continue;
        //    }
        //    printf("    plane[%d]: crtc_x:%d, crtc_y:%d, x:%d, y:%d\n", i, plane->crtc_x, plane->crtc_y, plane->x, plane->y);

        //    drmModeObjectProperties *props = drmModeObjectGetProperties(fd, plane_res->planes[i], DRM_MODE_OBJECT_PLANE);
        //    if (props == NULL) {
        //        perror("[modesetting] Could not get DRM device planes' properties. drmModeObjectGetProperties");
        //        continue;
        //    }
        //    drmModePropertyRes **props_info = calloc(props->count_props, sizeof *props_info);
        //    if (props_info == NULL) {
        //        perror("[modesetting] Could not get DRM device planes' properties. drmModeObjectGetProperties");
        //        continue;
        //    }
        //    for (int j = 0; j < props->count_props; j++) {
        //        props_info[j] = drmModeGetProperty(fd, props->props[j]);
        //        if (props_info[j] == NULL) {
        //            perror("[modesetting] Could not get DRM device planes' properties' info. drmModeGetProperty");
        //            continue;
        //        }
        //        // see: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L234
        //        //   type value: https://github.com/grate-driver/libdrm/blob/master/xf86drmMode.h#L311
        //        printf("      props_info[%d]: count_values:%d, name:%s\n", j, props_info[j]->count_values, props_info[j]->name);
        //        printf("        prop_values[%d]:%lu\n", j, props->prop_values[j]);
        //        for (int k = 0; k < props_info[j]->count_values; k++) {
        //            printf("        props_info[%d]->values[%d]:%lu\n", j, k, props_info[j]->values[k]);
        //        }
        //    }
        //}
	}
}

