#include "composite_out.h"

#include "config.h"

#include <opencv2/imgproc.hpp>

#include <drm/drm.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_mode.h>
#include <xf86drm.h>
#include <xf86drmMode.h>

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <vector>

#include <sys/mman.h>
#include <unistd.h>

namespace bench {

struct CompositeOutput::Impl {
    int drm_fd = -1;
    uint32_t connector_id = 0;
    uint32_t crtc_id = 0;
    uint32_t fb_id = 0;
    uint32_t handle = 0;
    uint32_t pitch = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    void* map = nullptr;
    size_t map_size = 0;
    cv::Mat rgb_scratch;
};

CompositeOutput::~CompositeOutput()
{
    close();
}

static bool looksLikeComposite(const drmModeConnector* conn)
{
    if (!conn) {
        return false;
    }
    if (conn->connector_type == DRM_MODE_CONNECTOR_TV ||
        conn->connector_type == DRM_MODE_CONNECTOR_Composite) {
        return true;
    }
    // Pi 5 VEC (J7): PAL 720×576i на card1, тип иногда не Composite/TV.
    for (int i = 0; i < conn->count_modes; ++i) {
        const drmModeModeInfo& m = conn->modes[i];
        if (m.hdisplay == static_cast<uint32_t>(kLoresWidth) &&
            m.vdisplay == static_cast<uint32_t>(kLoresHeight) &&
            (m.flags & DRM_MODE_FLAG_INTERLACE)) {
            return true;
        }
    }
    return false;
}

static uint32_t findCompositeConnector(int fd)
{
    drmModeRes* res = drmModeGetResources(fd);
    if (!res) {
        return 0;
    }

    uint32_t id = 0;
    for (int i = 0; i < res->count_connectors; ++i) {
        drmModeConnector* conn = drmModeGetConnector(fd, res->connectors[i]);
        if (!conn) {
            continue;
        }
        if (looksLikeComposite(conn) && conn->count_modes > 0 &&
            conn->connection == DRM_MODE_CONNECTED) {
            id = conn->connector_id;
            drmModeFreeConnector(conn);
            break;
        }
        drmModeFreeConnector(conn);
    }
    drmModeFreeResources(res);
    return id;
}

static int openCompositeDri(uint32_t& connector_id)
{
    for (int card = 0; card < 8; ++card) {
        char path[32];
        std::snprintf(path, sizeof(path), "/dev/dri/card%d", card);
        const int fd = ::open(path, O_RDWR | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        connector_id = findCompositeConnector(fd);
        if (connector_id) {
            return fd;
        }
        ::close(fd);
    }
    return -1;
}

static const drmModeModeInfo* pickPalMode(const drmModeConnector* conn)
{
    if (!conn || conn->count_modes == 0) {
        return nullptr;
    }
    for (int i = 0; i < conn->count_modes; ++i) {
        const drmModeModeInfo& m = conn->modes[i];
        if (m.hdisplay == static_cast<uint32_t>(kLoresWidth) &&
            m.vdisplay == static_cast<uint32_t>(kLoresHeight) &&
            (m.flags & DRM_MODE_FLAG_INTERLACE)) {
            return &conn->modes[i];
        }
    }
    return &conn->modes[0];
}

static uint32_t pickCrtc(int fd, uint32_t connector_id)
{
    drmModeConnector* conn = drmModeGetConnector(fd, connector_id);
    if (!conn) {
        return 0;
    }

    uint32_t crtc = 0;
    if (conn->encoder_id) {
        drmModeEncoder* enc = drmModeGetEncoder(fd, conn->encoder_id);
        if (enc) {
            if (enc->crtc_id) {
                crtc = enc->crtc_id;
            } else {
                drmModeRes* res = drmModeGetResources(fd);
                if (res) {
                    for (uint32_t i = 0; i < res->count_crtcs; ++i) {
                        if (enc->possible_crtcs & (1u << i)) {
                            crtc = res->crtcs[i];
                            break;
                        }
                    }
                    drmModeFreeResources(res);
                }
            }
            drmModeFreeEncoder(enc);
        }
    }

    if (!crtc) {
        drmModeRes* res = drmModeGetResources(fd);
        if (res && res->count_crtcs > 0) {
            crtc = res->crtcs[0];
        }
        drmModeFreeResources(res);
    }

    drmModeFreeConnector(conn);
    return crtc;
}

bool CompositeOutput::open()
{
    if (open_) {
        return true;
    }

    impl_ = new Impl();
    impl_->drm_fd = openCompositeDri(impl_->connector_id);
    if (impl_->drm_fd < 0) {
        std::cerr << "bench: composite connector not found (enable dtoverlay=...,composite)\n";
        close();
        return false;
    }

    impl_->crtc_id = pickCrtc(impl_->drm_fd, impl_->connector_id);
    if (!impl_->crtc_id) {
        std::cerr << "bench: no CRTC for composite\n";
        close();
        return false;
    }

    drmModeConnector* conn =
        drmModeGetConnector(impl_->drm_fd, impl_->connector_id);
    if (!conn || conn->connection != DRM_MODE_CONNECTED || conn->count_modes == 0) {
        std::cerr << "bench: composite connector has no modes\n";
        if (conn) {
            drmModeFreeConnector(conn);
        }
        close();
        return false;
    }

    const drmModeModeInfo* mode_ptr = pickPalMode(conn);
    if (!mode_ptr) {
        drmModeFreeConnector(conn);
        close();
        return false;
    }
    const drmModeModeInfo mode = *mode_ptr;

    impl_->width  = mode.hdisplay;
    impl_->height = mode.vdisplay;

    drm_mode_create_dumb create {};
    create.width  = impl_->width;
    create.height = impl_->height;
    create.bpp    = 24;

    if (drmIoctl(impl_->drm_fd, DRM_IOCTL_MODE_CREATE_DUMB, &create) != 0) {
        std::cerr << "bench: DRM_IOCTL_MODE_CREATE_DUMB failed\n";
        close();
        return false;
    }

    impl_->handle = create.handle;
    impl_->pitch  = create.pitch;
    impl_->map_size = create.size;

    uint32_t handles[4] = {impl_->handle, 0, 0, 0};
    uint32_t pitches[4] = {impl_->pitch, 0, 0, 0};
    uint32_t offsets[4] = {0, 0, 0, 0};
    if (drmModeAddFB2(impl_->drm_fd, impl_->width, impl_->height, DRM_FORMAT_RGB888,
                      handles, pitches, offsets, &impl_->fb_id, 0) != 0) {
        if (drmModeAddFB(impl_->drm_fd, impl_->width, impl_->height, 24, 24,
                         impl_->pitch, impl_->handle, &impl_->fb_id) != 0) {
            std::cerr << "bench: drmModeAddFB2/AddFB failed\n";
            close();
            return false;
        }
        std::cout << "bench: composite FB legacy 24bpp\n";
    } else {
        std::cout << "bench: composite FB RGB888 (VEC RG24)\n";
    }

    drm_mode_map_dumb map_req {};
    map_req.handle = impl_->handle;
    if (drmIoctl(impl_->drm_fd, DRM_IOCTL_MODE_MAP_DUMB, &map_req) != 0) {
        std::cerr << "bench: DRM_IOCTL_MODE_MAP_DUMB failed\n";
        close();
        return false;
    }

    impl_->map = mmap(nullptr, impl_->map_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                      impl_->drm_fd, map_req.offset);
    if (impl_->map == MAP_FAILED) {
        std::cerr << "bench: mmap dumb buffer failed\n";
        close();
        return false;
    }

    drmModeModeInfo mode_copy = mode;
    if (drmModeSetCrtc(impl_->drm_fd, impl_->crtc_id, impl_->fb_id, 0, 0,
                       &impl_->connector_id, 1, &mode_copy) != 0) {
        std::cerr << "bench: drmModeSetCrtc failed: " << std::strerror(errno)
                  << " (check PAL in cmdline/config)\n";
        drmModeFreeConnector(conn);
        close();
        return false;
    }

    drmModeFreeConnector(conn);

    open_ = true;
    std::cout << "bench: composite output " << impl_->width << 'x' << impl_->height
              << " PAL\n";
    return true;
}

void CompositeOutput::close()
{
    if (!impl_) {
        open_ = false;
        return;
    }

    if (impl_->map && impl_->map != MAP_FAILED) {
        munmap(impl_->map, impl_->map_size);
    }
    if (impl_->fb_id && impl_->drm_fd >= 0) {
        drmModeRmFB(impl_->drm_fd, impl_->fb_id);
    }
    if (impl_->handle && impl_->drm_fd >= 0) {
        drm_mode_destroy_dumb destroy {};
        destroy.handle = impl_->handle;
        drmIoctl(impl_->drm_fd, DRM_IOCTL_MODE_DESTROY_DUMB, &destroy);
    }
    if (impl_->drm_fd >= 0) {
        ::close(impl_->drm_fd);
    }

    delete impl_;
    impl_ = nullptr;
    open_ = false;
}

bool CompositeOutput::present(const cv::Mat& lores_bgr)
{
    if (!open_ || !impl_ || !impl_->map || lores_bgr.empty()) {
        return false;
    }

    cv::Mat resized;
    if (lores_bgr.cols != static_cast<int>(impl_->width) ||
        lores_bgr.rows != static_cast<int>(impl_->height)) {
        cv::resize(lores_bgr, resized, cv::Size(impl_->width, impl_->height));
    } else {
        resized = lores_bgr;
    }

    if (impl_->rgb_scratch.rows != resized.rows || impl_->rgb_scratch.cols != resized.cols ||
        impl_->rgb_scratch.type() != CV_8UC3) {
        impl_->rgb_scratch.create(resized.rows, resized.cols, CV_8UC3);
    }
    cv::cvtColor(resized, impl_->rgb_scratch, cv::COLOR_BGR2RGB);

    auto* dst = static_cast<uint8_t*>(impl_->map);
    const uint32_t row_bytes = impl_->width * 3;
    for (uint32_t y = 0; y < impl_->height; ++y) {
        std::memcpy(dst + y * impl_->pitch, impl_->rgb_scratch.ptr(y), row_bytes);
    }

    return true;
}

}  // namespace bench
