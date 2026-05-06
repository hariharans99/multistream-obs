#pragma once

#include "util/hw-capability.h"
#include <obs.h>
#include <string>
#include <cstdint>

// ── Configuration for one stream target ──────────────────────────────────────
struct StreamConfig {
    // Destination
    std::string rtmp_url;           // e.g. "rtmp://live.twitch.tv/app"
    std::string stream_key;
    std::string chat_url;           // e.g. "https://www.twitch.tv/popout/username/chat"

    // Video
    uint32_t    width       = 1280;
    uint32_t    height      = 720;
    uint32_t    bitrate_kbps = 3000; // video bitrate
    uint32_t    fps         = 0;     // 0 = match OBS

    // Audio
    uint32_t    audio_bitrate_kbps = 160;

    // Encoder
    EncoderType encoder_pref = EncoderType::Auto;

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
};
