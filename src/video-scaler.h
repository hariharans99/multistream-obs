#pragma once

#include "stream-config.h"
#include <obs.h>
#include <media-io/video-scaler.h>
#include <cstdint>

/**
 * VideoScaler — wraps libobs video_scaler_t and implements aspect-ratio-aware
 * frame conversion from the OBS canvas resolution to a target resolution.
 *
 * Three modes:
 *  Letterbox — source is shrunk to fit inside the target, black bars fill gaps.
 *  Crop      — source is grown to fill the target width or height, then cropped.
 *  Stretch   — source is stretched directly to target (may distort).
 *
 * Frame layout (example: 16:9 source → 9:16 target in Letterbox mode):
 *
 *   Target frame (target_w × target_h)
 *   ┌────────────────┐
 *   │████████████████│  ← top black bar  (bar_y rows)
 *   │████████████████│
 *   ├────────────────┤
 *   │  scaled src    │  ← (scaled_w × scaled_h)
 *   │                │
 *   ├────────────────┤
 *   │████████████████│  ← bottom black bar
 *   └────────────────┘
 */
class VideoScaler {
public:
    VideoScaler() = default;
    ~VideoScaler();

    VideoScaler(const VideoScaler &)            = delete;
    VideoScaler &operator=(const VideoScaler &) = delete;

    /**
     * (Re)initialize the scaler with source and target dimensions.
     *
     * @param src_w / src_h   OBS canvas size (e.g. 1920×1080)
     * @param dst_w / dst_h   Target stream size (e.g. 720×1280)
     * @param mode            Letterbox / Crop / Stretch
     * @param fmt             Pixel format (default: VIDEO_FORMAT_NV12)
     */
    bool init(uint32_t src_w, uint32_t src_h,
              uint32_t dst_w, uint32_t dst_h,
              AspectMode mode,
              video_format fmt = VIDEO_FORMAT_NV12);

    /**
     * Scale one frame.  The output frame buffer is internally managed.
     *
     * @param in   Pointer to incoming raw video_data from OBS
     * @return     Pointer to the scaled frame (owned by this scaler, valid until
     *             the next call to scale()), or nullptr on error.
     */
    struct video_data *scale(const struct video_data *in);

    bool is_initialized() const { return m_scaler != nullptr; }

    uint32_t dst_width()  const { return m_dst_w; }
    uint32_t dst_height() const { return m_dst_h; }

private:
    void destroy();
    void compute_layout();

    video_scaler_t *m_scaler = nullptr;

    uint32_t m_src_w = 0, m_src_h = 0;
    uint32_t m_dst_w = 0, m_dst_h = 0;
    AspectMode m_mode = AspectMode::Letterbox;
    video_format m_fmt = VIDEO_FORMAT_NV12;

    // Letterbox / Crop geometry
    uint32_t m_scaled_w = 0, m_scaled_h = 0;  // intermediate size from libobs scaler
    int32_t  m_bar_x    = 0, m_bar_y    = 0;  // pixel offset into output frame

    // Output frame buffer (pre-allocated)
    struct video_data m_out_frame{};
    uint8_t *m_out_buf[MAX_AV_PLANES] = {};
    size_t   m_out_linesize[MAX_AV_PLANES] = {};

    bool allocate_output_buffer();
    void free_output_buffer();

    // Intermediate buffer used for Crop mode
    uint8_t *m_crop_src_buf = nullptr;
};
