#include "camera_capture.h"

#include "config.h"

#include <libcamera/camera.h>
#include <libcamera/camera_manager.h>
#include <libcamera/control_ids.h>
#include <libcamera/controls.h>
#include <libcamera/formats.h>
#include <libcamera/framebuffer_allocator.h>
#include <libcamera/request.h>
#include <libcamera/stream.h>

#include <opencv2/imgproc.hpp>

#include <array>
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <condition_variable>
#include <sys/mman.h>
#include <unistd.h>
#include <unordered_map>
#include <vector>

namespace bench {

using namespace libcamera;

namespace {

void applyAutofocusControls(ControlList& ctl)
{
    if (!kCameraAfContinuous) {
        return;
    }
    ctl.set(controls::AfMode, controls::AfModeContinuous);
}

void applyCaptureControls(ControlList& ctl)
{
    applyAutofocusControls(ctl);
    const int64_t frame_us = 1000000 / kMainFps;
    const std::array<int64_t, 2> limits = {frame_us, frame_us};
    ctl.set(controls::FrameDurationLimits, limits);
}

void copyYPlane(const uint8_t* src, unsigned int src_stride, cv::Mat& dst, int w, int h)
{
    if (dst.rows != h || dst.cols != w || dst.type() != CV_8UC1) {
        dst.create(h, w, CV_8UC1);
    }
    if (static_cast<unsigned int>(dst.step[0]) == src_stride) {
        std::memcpy(dst.data, src, static_cast<size_t>(src_stride) * h);
        return;
    }
    for (int y = 0; y < h; ++y) {
        std::memcpy(dst.ptr(y), src + static_cast<size_t>(y) * src_stride,
                    static_cast<size_t>(w));
    }
}

struct MappedPlane {
    void*  addr = MAP_FAILED;
    size_t length = 0;
};

struct MappedBuffer {
    void*                    base = MAP_FAILED;
    size_t                   total_length = 0;
    std::vector<MappedPlane> planes;
};

void copyUvPlane(const uint8_t* src, unsigned int uv_stride, cv::Mat& dst, int w, int h)
{
    if (dst.rows != h / 2 || dst.cols != w || dst.type() != CV_8UC1) {
        dst.create(h / 2, w, CV_8UC1);
    }
    for (int y = 0; y < h / 2; ++y) {
        std::memcpy(dst.ptr(y), src + static_cast<size_t>(y) * uv_stride,
                    static_cast<size_t>(w));
    }
}

void copyRgbPlane(const uint8_t* src, unsigned int src_stride, cv::Mat& dst, int w, int h)
{
    if (dst.rows != h || dst.cols != w || dst.type() != CV_8UC3) {
        dst.create(h, w, CV_8UC3);
    }
    if (static_cast<unsigned int>(dst.step[0]) == src_stride) {
        std::memcpy(dst.data, src, static_cast<size_t>(src_stride) * h);
        return;
    }
    for (int y = 0; y < h; ++y) {
        std::memcpy(dst.ptr(y), src + static_cast<size_t>(y) * src_stride,
                    static_cast<size_t>(w) * 3);
    }
}

}  // namespace

struct CameraCapture::Impl {
    std::unique_ptr<CameraManager>        cm;
    std::shared_ptr<Camera>              camera;
    std::unique_ptr<CameraConfiguration> config;
    std::unique_ptr<FrameBufferAllocator> allocator;
    Stream* main_stream = nullptr;
    std::vector<std::unique_ptr<Request>> requests;
    std::unordered_map<const FrameBuffer*, MappedBuffer> mapped_by_buffer;

    struct Slot {
        cv::Mat  main_y;
        cv::Mat  main_uv;
        cv::Mat  main_rgb;
        cv::Mat  main_bgr;
        uint64_t seq = 0;
    };

    Slot slots[kCameraPublishSlots];
    int  published = 0;

    std::mutex              mtx;
    std::condition_variable cv;
    uint64_t                seq_counter = 0;

    bool mapBufferPlanes(FrameBuffer* buffer)
    {
        const auto& planes = buffer->planes();
        if (planes.empty()) {
            return false;
        }

        size_t total_length = 0;
        for (const FrameBuffer::Plane& plane : planes) {
            total_length = std::max(total_length,
                                    static_cast<size_t>(plane.offset) + plane.length);
        }

        const int fd = planes[0].fd.get();
        void* base =
            mmap(nullptr, total_length, PROT_READ, MAP_SHARED, fd, 0);
        if (base == MAP_FAILED) {
            return false;
        }

        MappedBuffer mapped_buf;
        mapped_buf.base          = base;
        mapped_buf.total_length  = total_length;
        mapped_buf.planes.reserve(planes.size());
        for (const FrameBuffer::Plane& plane : planes) {
            MappedPlane mp;
            mp.length = plane.length;
            mp.addr   = static_cast<uint8_t*>(base) + plane.offset;
            mapped_buf.planes.push_back(mp);
        }

        mapped_by_buffer[buffer] = std::move(mapped_buf);
        return true;
    }

    void publishMain(const FrameBuffer* buffer, const StreamConfiguration& cfg, Slot& slot)
    {
        const auto it = mapped_by_buffer.find(buffer);
        if (it == mapped_by_buffer.end() || it->second.planes.empty()) {
            return;
        }

        const unsigned int w = cfg.size.width;
        const unsigned int h = cfg.size.height;
        const auto* y_src = static_cast<const uint8_t*>(it->second.planes[0].addr);

        if (cfg.pixelFormat == formats::NV12 || cfg.pixelFormat == formats::YUV420) {
            copyYPlane(y_src, cfg.stride, slot.main_y, static_cast<int>(w),
                       static_cast<int>(h));
            if (it->second.planes.size() >= 2 &&
                it->second.planes[1].addr != MAP_FAILED) {
                const auto* uv_src =
                    static_cast<const uint8_t*>(it->second.planes[1].addr);
                unsigned int uv_stride = cfg.stride;
                if (h >= 2 && it->second.planes[1].length > 0) {
                    uv_stride = static_cast<unsigned int>(
                        it->second.planes[1].length / (h / 2));
                }
                copyUvPlane(uv_src, uv_stride, slot.main_uv, static_cast<int>(w),
                            static_cast<int>(h));
            }
            slot.main_rgb = cv::Mat();
            slot.main_bgr = cv::Mat();
        } else if (cfg.pixelFormat == formats::RGB888 ||
                   cfg.pixelFormat == formats::BGR888) {
            copyRgbPlane(y_src, cfg.stride, slot.main_rgb, static_cast<int>(w),
                           static_cast<int>(h));
            slot.main_bgr = cv::Mat();
            slot.main_uv  = cv::Mat();
            slot.main_y   = cv::Mat();
        }
    }

    void onRequestComplete(Request* request);
};

void CameraCapture::Impl::onRequestComplete(Request* request)
{
    if (request->status() == Request::RequestCancelled) {
        return;
    }

    const int write_idx = (published + 1) % kCameraPublishSlots;
    Slot&     slot      = slots[write_idx];

    const Request::BufferMap& buffers = request->buffers();
    for (auto const& [stream, buffer] : buffers) {
        const FrameMetadata& meta = buffer->metadata();
        if (meta.status != FrameMetadata::FrameSuccess) {
            continue;
        }

        if (stream == main_stream) {
            publishMain(buffer, stream->configuration(), slot);
        }
    }

    if (!slot.main_y.empty() || !slot.main_rgb.empty()) {
        std::lock_guard<std::mutex> lock(mtx);
        slot.seq = ++seq_counter;
        published = write_idx;
        cv.notify_all();
    }

    request->reuse(Request::ReuseBuffers);
    applyCaptureControls(request->controls());
    camera->queueRequest(request);
}

CameraCapture::~CameraCapture()
{
    close();
}

bool CameraCapture::open()
{
    if (open_) {
        return true;
    }

    auto* impl = new Impl();
    impl_      = impl;

    impl->cm = std::make_unique<CameraManager>();
    if (impl->cm->start()) {
        std::cerr << "bench: CameraManager::start failed\n";
        close();
        return false;
    }

    if (impl->cm->cameras().empty()) {
        std::cerr << "bench: no cameras\n";
        close();
        return false;
    }

    impl->camera = impl->cm->cameras()[0];
    if (impl->camera->acquire()) {
        std::cerr << "bench: acquire camera failed\n";
        close();
        return false;
    }

    const auto configureMain = [&](PixelFormat main_fmt) -> bool {
        impl->config = impl->camera->generateConfiguration({StreamRole::Viewfinder});
        if (!impl->config) {
            return false;
        }

        StreamConfiguration& main_sc = impl->config->at(0);
        main_sc.size.width           = static_cast<unsigned int>(kMainWidth);
        main_sc.size.height          = static_cast<unsigned int>(kMainHeight);
        main_sc.pixelFormat          = main_fmt;
        main_sc.bufferCount          = kCameraBufferCount;

        return impl->config->validate() != CameraConfiguration::Invalid;
    };

    if (!configureMain(formats::BGR888) && !configureMain(formats::RGB888) &&
        !configureMain(formats::NV12)) {
        std::cerr << "bench: invalid camera configuration (FullHD RGB888/NV12)\n";
        close();
        return false;
    }

    if (impl->camera->configure(impl->config.get()) < 0) {
        std::cerr << "bench: configure failed\n";
        close();
        return false;
    }

    impl->main_stream = impl->config->at(0).stream();

    impl->allocator = std::make_unique<FrameBufferAllocator>(impl->camera);

    const auto mapStream = [&](Stream* stream, const char* name) -> bool {
        if (!stream) {
            return true;
        }
        if (impl->allocator->allocate(stream) < 0) {
            std::cerr << "bench: allocate failed for " << name << " stream\n";
            return false;
        }
        for (const auto& buffer : impl->allocator->buffers(stream)) {
            if (!impl->mapBufferPlanes(buffer.get())) {
                std::cerr << "bench: mmap failed for " << name << " stream buffer\n";
                return false;
            }
        }
        return true;
    };

    if (!mapStream(impl->main_stream, "main")) {
        close();
        return false;
    }

    impl->camera->requestCompleted.connect(impl, &Impl::onRequestComplete);

    const auto& main_bufs = impl->allocator->buffers(impl->main_stream);

    for (size_t i = 0; i < main_bufs.size(); ++i) {
        std::unique_ptr<Request> request = impl->camera->createRequest();
        if (!request || request->addBuffer(impl->main_stream, main_bufs[i].get()) < 0) {
            close();
            return false;
        }
        applyCaptureControls(request->controls());
        impl->requests.push_back(std::move(request));
    }

    if (kCameraAfContinuous) {
        const auto& cam_ctrls = impl->camera->controls();
        if (cam_ctrls.find(&controls::AfMode) != cam_ctrls.end()) {
            std::cout << "bench: camera AF continuous (IMX708 PDAF)\n";
        } else {
            std::cerr << "bench: camera has no AF control (fixed-focus module?)\n";
        }
    }

    if (impl->camera->start()) {
        std::cerr << "bench: camera start failed\n";
        close();
        return false;
    }

    for (auto& req : impl->requests) {
        impl->camera->queueRequest(req.get());
    }

    open_ = true;
    const StreamConfiguration& sc = impl->config->at(0);
    has_rgb_main_ = (sc.pixelFormat == formats::RGB888 || sc.pixelFormat == formats::BGR888);
    std::cout << "bench: camera " << sc.size.width << 'x' << sc.size.height;
    if (has_rgb_main_) {
        std::cout << " RGB888 (ISP sRGB, lores "
                  << (kCameraRgb888BytesAreRgb ? "RGB→BGR" : "BGR bytes") << " @"
                  << kLoresFps << "fps)";
    } else {
        std::cout << " NV12";
    }
    std::cout << " @" << kMainFps << "fps (FullHD track+PAL " << kLoresFps << "fps)\n";
    return true;
}

void CameraCapture::close()
{
    auto* impl = impl_;
    if (!impl) {
        open_ = false;
        return;
    }

    if (impl->camera) {
        impl->camera->stop();
        impl->camera->release();
    }
    if (impl->cm) {
        impl->cm->stop();
    }

    for (auto& entry : impl->mapped_by_buffer) {
        if (entry.second.base != MAP_FAILED) {
            munmap(entry.second.base, entry.second.total_length);
        }
    }

    delete impl;
    impl_ = nullptr;
    open_ = false;
    has_rgb_main_ = false;
}

bool CameraCapture::grabLatest(CameraFrame& out, int timeout_ms)
{
    if (!open_ || !impl_) {
        return false;
    }

    std::unique_lock<std::mutex> lock(impl_->mtx);
    if (impl_->seq_counter == 0) {
        if (timeout_ms <= 0) {
            return false;
        }
        if (!impl_->cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                                [&] { return impl_->seq_counter > 0; })) {
            return false;
        }
    }

    const Impl::Slot& slot = impl_->slots[impl_->published];
    if (slot.seq == 0) {
        return false;
    }

    out.main_y    = slot.main_y;
    out.main_uv   = slot.main_uv;
    out.main_rgb  = slot.main_rgb;
    out.main_bgr  = slot.main_bgr;
    out.seq       = slot.seq;
    return !out.main_y.empty() || !out.main_bgr.empty() || !out.main_rgb.empty();
}

void CameraCapture::ensureMainBgr(CameraFrame& frame)
{
    if (!frame.main_bgr.empty()) {
        return;
    }
    if (!frame.main_rgb.empty()) {
        if (kCameraRgb888BytesAreRgb) {
            cv::cvtColor(frame.main_rgb, frame.main_bgr, cv::COLOR_RGB2BGR);
        } else {
            frame.main_rgb.copyTo(frame.main_bgr);
        }
        return;
    }
    if (!frame.main_y.empty() && !frame.main_uv.empty()) {
        cv::Mat uv_c2(frame.main_uv.rows, frame.main_uv.cols / 2, CV_8UC2,
                      frame.main_uv.data, frame.main_uv.step[0]);
        cv::cvtColorTwoPlane(frame.main_y, uv_c2, frame.main_bgr, cv::COLOR_YUV2BGR_NV12);
        return;
    }
    if (!frame.main_y.empty()) {
        cv::cvtColor(frame.main_y, frame.main_bgr, cv::COLOR_GRAY2BGR);
    }
}

cv::Mat makeLoresFrame(const cv::Mat& main_bgr)
{
    cv::Mat lores;
    if (main_bgr.empty()) {
        return lores;
    }
    cv::resize(main_bgr, lores, cv::Size(kLoresWidth, kLoresHeight), 0, 0, cv::INTER_LINEAR);
    return lores;
}

cv::Mat makeLoresFromFrame(const CameraFrame& frame)
{
    if (!frame.main_rgb.empty()) {
        cv::Mat small;
        cv::resize(frame.main_rgb, small, cv::Size(kLoresWidth, kLoresHeight), 0, 0,
                   cv::INTER_LINEAR);
        cv::Mat lores;
        if (kCameraRgb888BytesAreRgb) {
            cv::cvtColor(small, lores, cv::COLOR_RGB2BGR);
        } else {
            lores = small;
        }
        return lores;
    }
    if (!frame.main_bgr.empty()) {
        return makeLoresFrame(frame.main_bgr);
    }
    if (frame.main_y.empty() || frame.main_uv.empty()) {
        return {};
    }
    cv::Mat sy;
    cv::Mat suv;
    cv::resize(frame.main_y, sy, cv::Size(kLoresWidth, kLoresHeight), 0, 0, cv::INTER_LINEAR);
    cv::resize(frame.main_uv, suv, cv::Size(kLoresWidth, kLoresHeight / 2), 0, 0,
               cv::INTER_LINEAR);
    cv::Mat uv_c2(suv.rows, suv.cols / 2, CV_8UC2, suv.data, suv.step[0]);
    cv::Mat lores;
    cv::cvtColorTwoPlane(sy, uv_c2, lores, cv::COLOR_YUV2BGR_NV12);
    return lores;
}

}  // namespace bench
