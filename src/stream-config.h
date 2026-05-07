#pragma once

#include "util/hw-capability.h"
#include <obs-module.h>
#include <string>
#include <cstdint>

enum class ScalingMode {
    Auto,
    Point,
    Bilinear,
    Bicubic,
    Lanczos,
    Area,
    FSR,
    NIS,
    CAS
};

// ── Configuration for one stream target ──────────────────────────────────────
struct StreamConfig {
    // Destination
    std::string rtmp_url;           // e.g. "rtmp://live.twitch.tv/app"
    std::string stream_key;
    std::string chat_url;           // e.g. "https://www.twitch.tv/popout/username/chat"
    std::string platform;           // e.g. "YouTube", "Twitch"

    // Video
    uint32_t    width       = 1280;
    uint32_t    height      = 720;
    uint32_t    bitrate_kbps = 3000; // video bitrate
    uint32_t    fps         = 0;     // 0 = match OBS

    // Audio
    uint32_t    audio_bitrate_kbps = 160;

    // Encoder
    EncoderType encoder_pref = EncoderType::Auto;
    ScalingMode scale_mode   = ScalingMode::Auto;
    float       sharpening   = 0.0f; // 0.0 to 1.0

    // Display name shown in the UI table
    std::string label;              // auto-generated if empty


    bool        enabled     = true;
};


// ── Runtime stats (read-only, updated by the output thread) ──────────────────
struct StreamStats {
    bool        is_active           = false;
    bool        has_error           = false;
    std::string error_message;

    // Network / encoder
    uint64_t    bytes_sent          = 0;    // cumulative bytes sent
    double      net_bitrate_kbps    = 0.0;  // actual throughput this second (kbps)
    uint32_t    dropped_frames      = 0;    // frames dropped by encoder/network
    uint32_t    total_frames        = 0;    // total frames output by OBS

    // Rendering health (global OBS render pipeline, same for all streams)
    uint32_t    render_total_frames = 0;    // obs_get_total_frames()
    uint32_t    render_lagged_frames= 0;    // obs_get_lagged_frames()  (missed renders)

    double      output_fps          = 0.0;  // obs_get_active_fps()
    int         reconnect_count     = 0;

    // Detailed metrics
    double      avg_render_time_ms  = 0.0;
    uint32_t    skipped_frames      = 0;    // skipped due to encoding lag
    uint32_t    total_encoded_frames = 0;
    std::string encoder_name;
    std::string scale_filter;
};
