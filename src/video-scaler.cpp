#include "video-scaler.h"
#include "util/logger.h"
#include <util/bmem.h>
#include <cstring>
#include <algorithm>

// ── Destructor ─────────────────────────────────────────────────────────────────

VideoScaler::~VideoScaler()
{
    destroy();
}

// ── Public: init ───────────────────────────────────────────────────────────────

bool VideoScaler::init(uint32_t src_w, uint32_t src_h,
                       uint32_t dst_w, uint32_t dst_h,
                       AspectMode mode,
                       video_format fmt)
{
    destroy();

    m_src_w = src_w; m_src_h = src_h;
    m_dst_w = dst_w; m_dst_h = dst_h;
    m_mode  = mode;
    m_fmt   = fmt;

    compute_layout();

    // ── Build the libobs video_scaler_t ───────────────────────────────────────
    // For Stretch: scale directly src→dst.
    // For Letterbox/Crop: we use a two-step approach internally:
    //   step 1: scale src → (m_scaled_w × m_scaled_h)  via libobs
    //   step 2: blit the scaled image into the correct position in dst frame
    //           (Letterbox: offset by bar; Crop: no offset but src is pre-cropped)

    struct video_scale_info in_info  = {};
    struct video_scale_info out_info = {};

    in_info.format     = fmt;
    in_info.width      = src_w;
    in_info.height     = src_h;
    in_info.range      = VIDEO_RANGE_DEFAULT;
    in_info.colorspace = VIDEO_CS_DEFAULT;

    out_info.format     = fmt;
    out_info.width      = m_scaled_w;
    out_info.height     = m_scaled_h;
    out_info.range      = VIDEO_RANGE_DEFAULT;
    out_info.colorspace = VIDEO_CS_DEFAULT;

    int ret = video_scaler_create(&m_scaler, &out_info, &in_info, VIDEO_SCALE_BICUBIC);
    if (ret != 0 || !m_scaler) {
        mlog_error("VideoScaler::init — video_scaler_create failed (ret=%d)", ret);
        return false;
    }

    if (!allocate_output_buffer()) {
        destroy();
        return false;
    }

    mlog_info("VideoScaler initialized: %dx%d → %dx%d [mode: %s] (intermediate: %dx%d)",
              src_w, src_h, dst_w, dst_h,
              mode == AspectMode::Letterbox ? "Letterbox" :
              mode == AspectMode::Crop      ? "Crop"      : "Stretch",
              m_scaled_w, m_scaled_h);
    return true;
}

// ── Public: scale ──────────────────────────────────────────────────────────────

struct video_data *VideoScaler::scale(const struct video_data *in)
{
    if (!m_scaler || !in) return nullptr;

    if (m_mode == AspectMode::Stretch) {
        // Fast path: direct scale into output buffer
        video_scaler_scale(m_scaler,
                           m_out_frame.data, m_out_frame.linesize,
                           const_cast<const uint8_t **>(in->data),
                           in->linesize);
        m_out_frame.timestamp = in->timestamp;
        return &m_out_frame;
    }

    // ── Letterbox / Crop: two-step ─────────────────────────────────────────
    // Step 1 — scale the source into a temporary sub-buffer of size
    //          (m_scaled_w × m_scaled_h).
    //
    // We point the scaler output directly at the correct row/column offset
    // inside m_out_frame for Letterbox.  For Crop, we offset the *input*
    // pointers to the desired crop window, then scale to full dst size.

    if (m_mode == AspectMode::Letterbox) {
        // Clear output buffer to black (NV12: Y=0x10, UV=0x80)
        // Only need to clear when the bar regions exist
        if (m_bar_y > 0 || m_bar_x > 0) {
            // Y plane
            memset(m_out_buf[0], 0x10, m_dst_w * m_dst_h);
            // UV plane (half height for NV12)
            memset(m_out_buf[1], 0x80, m_dst_w * (m_dst_h / 2));
        }

        // Point scaler output at the offset within the full output buffer
        const uint8_t *scaled_out[MAX_AV_PLANES] = {};
        uint32_t scaled_linesize[MAX_AV_PLANES]   = {};

        // Y plane offset
        scaled_out[0]       = m_out_buf[0] + m_bar_y * m_dst_w + m_bar_x;
        scaled_linesize[0]  = m_dst_w;   // stride of the *full* output frame

        // UV plane offset (NV12: height/2)
        scaled_out[1]      = m_out_buf[1] + (m_bar_y / 2) * m_dst_w + m_bar_x;
        scaled_linesize[1] = m_dst_w;

        video_scaler_scale(m_scaler,
                           const_cast<uint8_t **>(scaled_out),
                           reinterpret_cast<uint32_t *>(scaled_linesize),
                           const_cast<const uint8_t **>(in->data),
                           in->linesize);
    }
    else { // Crop
        // Crop the source: compute the sub-rect of the input that matches
        // the target aspect ratio, then feed only that region to the scaler.
        float src_ar = static_cast<float>(m_src_w) / static_cast<float>(m_src_h);
        float dst_ar = static_cast<float>(m_dst_w) / static_cast<float>(m_dst_h);

        uint32_t crop_x = 0, crop_y = 0;
        uint32_t crop_w = m_src_w, crop_h = m_src_h;

        if (src_ar > dst_ar) {
            // Wider than target → crop left and right
            crop_w = static_cast<uint32_t>(m_src_h * dst_ar);
            crop_x = (m_src_w - crop_w) / 2;
        } else if (src_ar < dst_ar) {
            // Taller than target → crop top and bottom
            crop_h = static_cast<uint32_t>(m_src_w / dst_ar);
            crop_y = (m_src_h - crop_h) / 2;
        }

        // Adjust input pointers to the crop origin
        const uint8_t *cropped[MAX_AV_PLANES] = {};
        uint32_t        cl[MAX_AV_PLANES]      = {};

        cropped[0] = in->data[0] + crop_y       * in->linesize[0] + crop_x;
        cl[0]      = in->linesize[0];

        if (in->data[1]) {  // NV12: interleaved UV
            cropped[1] = in->data[1] + (crop_y / 2) * in->linesize[1] + (crop_x & ~1u);
            cl[1]      = in->linesize[1];
        }

        // Re-init scaler with crop_w×crop_h → dst_w×dst_h if dimensions changed
        // (happens only on first crop frame or resolution change)
        if (m_scaler && (m_scaled_w != m_dst_w || m_scaled_h != m_dst_h
                         || (crop_w != m_src_w) || (crop_h != m_src_h))) {
            // Rebuild scaler for crop dimensions → dst
            video_scaler_destroy(m_scaler);
            m_scaler = nullptr;

            struct video_scale_info ci = {}, co = {};
            ci.format = co.format = m_fmt;
            ci.width  = crop_w; ci.height  = crop_h;
            co.width  = m_dst_w; co.height = m_dst_h;
            ci.range  = co.range  = VIDEO_RANGE_DEFAULT;
            ci.colorspace = co.colorspace = VIDEO_CS_DEFAULT;

            video_scaler_create(&m_scaler, &co, &ci, VIDEO_SCALE_BICUBIC);
        }

        if (m_scaler) {
            video_scaler_scale(m_scaler,
                               m_out_frame.data,
                               m_out_frame.linesize,
                               cropped, cl);
        }
    }

    m_out_frame.timestamp = in->timestamp;
    return &m_out_frame;
}

// ── Private: compute geometry ─────────────────────────────────────────────────

void VideoScaler::compute_layout()
{
    if (m_mode == AspectMode::Stretch) {
        m_scaled_w = m_dst_w;
        m_scaled_h = m_dst_h;
        m_bar_x = m_bar_y = 0;
        return;
    }

    float src_ar = static_cast<float>(m_src_w) / static_cast<float>(m_src_h);
    float dst_ar = static_cast<float>(m_dst_w) / static_cast<float>(m_dst_h);

    if (m_mode == AspectMode::Letterbox) {
        // Fit src inside dst
        if (src_ar > dst_ar) {
            // Source is wider → fit width, add top/bottom bars
            m_scaled_w = m_dst_w;
            m_scaled_h = static_cast<uint32_t>(m_dst_w / src_ar);
            // Align to even (required for NV12 chroma subsampling)
            m_scaled_h &= ~1u;
            m_bar_x = 0;
            m_bar_y = (m_dst_h - m_scaled_h) / 2;
            m_bar_y &= ~1u;
        } else {
            // Source is taller → fit height, add left/right bars
            m_scaled_h = m_dst_h;
            m_scaled_w = static_cast<uint32_t>(m_dst_h * src_ar);
            m_scaled_w &= ~1u;
            m_bar_y = 0;
            m_bar_x = (m_dst_w - m_scaled_w) / 2;
            m_bar_x &= ~1u;
        }
    } else {
        // Crop mode: scaler scales directly to dst (cropping handled in scale())
        m_scaled_w = m_dst_w;
        m_scaled_h = m_dst_h;
        m_bar_x = m_bar_y = 0;
    }
}

// ── Private: buffer management ────────────────────────────────────────────────

bool VideoScaler::allocate_output_buffer()
{
    free_output_buffer();

    // NV12: Y plane = w*h bytes, UV plane = w*(h/2) bytes
    size_t y_size  = static_cast<size_t>(m_dst_w) * m_dst_h;
    size_t uv_size = static_cast<size_t>(m_dst_w) * (m_dst_h / 2);

    m_out_buf[0] = static_cast<uint8_t *>(bmalloc(y_size));
    m_out_buf[1] = static_cast<uint8_t *>(bmalloc(uv_size));

    if (!m_out_buf[0] || !m_out_buf[1]) {
        mlog_error("VideoScaler: failed to allocate output buffers");
        free_output_buffer();
        return false;
    }

    // Black frame (NV12: Y=16, UV=128)
    memset(m_out_buf[0], 0x10, y_size);
    memset(m_out_buf[1], 0x80, uv_size);

    m_out_frame.data[0]     = m_out_buf[0];
    m_out_frame.data[1]     = m_out_buf[1];
    m_out_frame.linesize[0] = m_dst_w;
    m_out_frame.linesize[1] = m_dst_w;

    return true;
}

void VideoScaler::free_output_buffer()
{
    for (int i = 0; i < MAX_AV_PLANES; ++i) {
        bfree(m_out_buf[i]);
        m_out_buf[i] = nullptr;
        m_out_frame.data[i]     = nullptr;
        m_out_frame.linesize[i] = 0;
    }
    bfree(m_crop_src_buf);
    m_crop_src_buf = nullptr;
}

// ── Private: destroy ──────────────────────────────────────────────────────────

void VideoScaler::destroy()
{
    if (m_scaler) {
        video_scaler_destroy(m_scaler);
        m_scaler = nullptr;
    }
    free_output_buffer();
}
