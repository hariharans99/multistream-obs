#include "stream-target.h"
#include "util/logger.h"
#include "util/hw-capability.h"
#include <obs.h>
#include <util/platform.h>
#include <sstream>

// ── Constructor / Destructor ──────────────────────────────────────────────────

StreamTarget::StreamTarget(int index, const StreamConfig &cfg)
    : m_index(index), m_config(cfg)
{
    // Resolve encoder at creation time so we can report it immediately
    m_resolved_encoder_type = hw_resolve_encoder(cfg.encoder_pref, m_resolved_encoder_id);
    mlog_info("StreamTarget[%d] created — encoder: %s (%s), resolution: %dx%d, bitrate: %u kbps",
              m_index,
              hw_encoder_label(m_resolved_encoder_type),
              m_resolved_encoder_id.c_str(),
              m_config.width, m_config.height,
              m_config.bitrate_kbps);
}

StreamTarget::~StreamTarget()
{
    if (m_active) stop();
    destroy_encoders();
    destroy_output();
}

// ── Public control ────────────────────────────────────────────────────────────

bool StreamTarget::start()
{
    if (m_active) {
        mlog_warn("StreamTarget[%d] start() called but already active", m_index);
        return true;
    }

    mlog_info("StreamTarget[%d] starting...", m_index);

    if (!create_video_encoder()) return false;
    if (!create_audio_encoder()) { destroy_encoders(); return false; }
    if (!create_output())        { destroy_encoders(); return false; }

    // Attach encoders to output
    obs_output_set_video_encoder(m_output, m_video_encoder);
    obs_output_set_audio_encoder(m_output, m_audio_encoder, 0);

    // Attach service to output
    obs_output_set_service(m_output, m_service);

    if (!obs_output_start(m_output)) {
        const char *err = obs_output_get_last_error(m_output);
        mlog_error("StreamTarget[%d] obs_output_start failed: %s", m_index,
                   err ? err : "(no error message)");
        {
            std::lock_guard<std::mutex> lk(m_stats_mutex);
            m_stats.has_error     = true;
            m_stats.error_message = err ? err : "Failed to start output";
        }
        destroy_encoders();
        destroy_output();
        return false;
    }

    m_active = true;
    mlog_info("StreamTarget[%d] started successfully", m_index);
    return true;
}

void StreamTarget::stop()
{
    if (!m_active) return;

    mlog_info("StreamTarget[%d] stopping...", m_index);

    if (m_output) {
        obs_output_stop(m_output);
        // obs_output_stop is async; wait for the stop signal
        // In practice the signal handler sets m_active = false
    }
}

void StreamTarget::update(const StreamConfig &cfg)
{
    if (m_active) {
        mlog_warn("StreamTarget[%d] update() called while active; stop first", m_index);
        return;
    }
    m_config = cfg;
    m_resolved_encoder_type = hw_resolve_encoder(cfg.encoder_pref, m_resolved_encoder_id);
    mlog_info("StreamTarget[%d] config updated — %dx%d @ %u kbps",
              m_index, cfg.width, cfg.height, cfg.bitrate_kbps);
}

StreamStats StreamTarget::get_stats() const
{
    std::lock_guard<std::mutex> lk(m_stats_mutex);
    StreamStats s = m_stats;
    if (m_active && m_output) {
        s.is_active           = true;
        s.bytes_sent          = obs_output_get_total_bytes(m_output);
        s.dropped_frames      = static_cast<uint32_t>(obs_output_get_frames_dropped(m_output));
        s.total_frames        = static_cast<uint32_t>(obs_output_get_total_frames(m_output));

        // Global OBS render pipeline stats (same for every stream)
        s.render_total_frames  = obs_get_total_frames();
        s.render_lagged_frames = obs_get_lagged_frames();

        s.output_fps          = obs_get_active_fps();

        // Actual throughput: byte delta since last call × 8 ÷ elapsed seconds
        uint64_t now_ns = os_gettime_ns();
        if (m_prev_time_ns == 0) {
            m_prev_time_ns = now_ns;
            m_prev_bytes = s.bytes_sent;
        } else {
            uint64_t delta_ns = now_ns - m_prev_time_ns;
            // Update only if at least ~500ms has elapsed (smooths out fluctuations)
            if (delta_ns >= 500000000ULL) {
                uint64_t delta_bytes = (s.bytes_sent > m_prev_bytes) ? (s.bytes_sent - m_prev_bytes) : 0;
                double seconds = static_cast<double>(delta_ns) / 1000000000.0;
                m_cached_bitrate_kbps = (static_cast<double>(delta_bytes) * 8.0 / 1000.0) / seconds;
                
                m_prev_bytes   = s.bytes_sent;
                m_prev_time_ns = now_ns;
            }
        }
        s.net_bitrate_kbps = m_cached_bitrate_kbps;
    } else {
        m_prev_time_ns = 0;
        m_prev_bytes = 0;
        m_cached_bitrate_kbps = 0.0;
    }
    return s;
}

// ── Private: encoder creation ─────────────────────────────────────────────────

bool StreamTarget::create_video_encoder()
{
    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, "bitrate", static_cast<long long>(m_config.bitrate_kbps));
    obs_data_set_int(settings, "keyint_sec", 2);   // 2-second keyframe interval (standard)

    if (m_config.fps > 0) {
        obs_data_set_int(settings, "fps_num", m_config.fps);
        obs_data_set_int(settings, "fps_den", 1);
        obs_data_set_int(settings, "framerate_num", m_config.fps);
        obs_data_set_int(settings, "framerate_den", 1);
    }

    // Encoder-specific tuning
    if (m_resolved_encoder_type == EncoderType::x264) {
        obs_data_set_string(settings, "preset",  "veryfast");
        obs_data_set_string(settings, "profile", "high");
        obs_data_set_string(settings, "tune",    "zerolatency");
    } else {
        // NVENC / AMF / QSV: use "quality" preset where supported
        obs_data_set_string(settings, "preset", "quality");
        obs_data_set_string(settings, "profile", "high");
    }

    std::string name = make_unique_name("ms_venc");
    m_video_encoder = obs_video_encoder_create(m_resolved_encoder_id.c_str(),
                                               name.c_str(),
                                               settings,
                                               nullptr);
    obs_data_release(settings);

    if (!m_video_encoder) {
        mlog_error("StreamTarget[%d] Failed to create video encoder '%s'",
                   m_index, m_resolved_encoder_id.c_str());
        return false;
    }

    // Set output resolution (libobs will scale from canvas)
    obs_encoder_set_scaled_size(m_video_encoder, m_config.width, m_config.height);
    obs_encoder_set_gpu_scale_type(m_video_encoder, OBS_SCALE_BICUBIC);

    // Attach to the main video mix
    obs_encoder_set_video(m_video_encoder, obs_get_video());

    mlog_info("StreamTarget[%d] video encoder created: %s @ %dx%d %u kbps",
              m_index, m_resolved_encoder_id.c_str(),
              m_config.width, m_config.height, m_config.bitrate_kbps);
    return true;
}

bool StreamTarget::create_audio_encoder()
{
    obs_data_t *settings = obs_data_create();
    obs_data_set_int(settings, "bitrate", static_cast<long long>(m_config.audio_bitrate_kbps));

    std::string name = make_unique_name("ms_aenc");
    m_audio_encoder = obs_audio_encoder_create("ffmpeg_aac",
                                               name.c_str(),
                                               settings,
                                               0,      // mixer track 0
                                               nullptr);
    obs_data_release(settings);

    if (!m_audio_encoder) {
        mlog_error("StreamTarget[%d] Failed to create audio encoder", m_index);
        return false;
    }

    obs_encoder_set_audio(m_audio_encoder, obs_get_audio());

    mlog_info("StreamTarget[%d] audio encoder created: AAC @ %u kbps",
              m_index, m_config.audio_bitrate_kbps);
    return true;
}

bool StreamTarget::create_output()
{
    // ── Service (carries the RTMP URL + stream key) ───────────────────────────
    obs_data_t *svc_settings = obs_data_create();
    obs_data_set_string(svc_settings, "server", m_config.rtmp_url.c_str());
    obs_data_set_string(svc_settings, "key",    m_config.stream_key.c_str());

    std::string svc_name = make_unique_name("ms_service");
    m_service = obs_service_create("rtmp_custom", svc_name.c_str(), svc_settings, nullptr);
    obs_data_release(svc_settings);

    if (!m_service) {
        mlog_error("StreamTarget[%d] Failed to create RTMP service", m_index);
        return false;
    }

    // ── Output ────────────────────────────────────────────────────────────────
    obs_data_t *out_settings = obs_data_create();
    // Reconnect on disconnect: up to 20 retries, 10s delay
    obs_data_set_bool(out_settings, "reconnect",       true);
    obs_data_set_int(out_settings,  "retry_delay",     10);
    obs_data_set_int(out_settings,  "max_retries",     20);

    std::string out_name = make_unique_name("ms_output");
    m_output = obs_output_create("rtmp_output", out_name.c_str(), out_settings, nullptr);
    obs_data_release(out_settings);

    if (!m_output) {
        mlog_error("StreamTarget[%d] Failed to create RTMP output", m_index);
        obs_service_release(m_service);
        m_service = nullptr;
        return false;
    }

    // ── Signal handlers ───────────────────────────────────────────────────────
    signal_handler_t *sh = obs_output_get_signal_handler(m_output);
    signal_handler_connect(sh, "start",              on_start,              this);
    signal_handler_connect(sh, "stop",               on_stop,               this);
    signal_handler_connect(sh, "reconnect",          on_reconnect,          this);
    signal_handler_connect(sh, "reconnect_success",  on_reconnect_success,  this);

    return true;
}

// ── Private: teardown ─────────────────────────────────────────────────────────

void StreamTarget::destroy_encoders()
{
    if (m_video_encoder) { obs_encoder_release(m_video_encoder); m_video_encoder = nullptr; }
    if (m_audio_encoder) { obs_encoder_release(m_audio_encoder); m_audio_encoder = nullptr; }
}

void StreamTarget::destroy_output()
{
    if (m_output) {
        // Disconnect signals before releasing
        signal_handler_t *sh = obs_output_get_signal_handler(m_output);
        signal_handler_disconnect(sh, "start",             on_start,             this);
        signal_handler_disconnect(sh, "stop",              on_stop,              this);
        signal_handler_disconnect(sh, "reconnect",         on_reconnect,         this);
        signal_handler_disconnect(sh, "reconnect_success", on_reconnect_success, this);

        obs_output_release(m_output);
        m_output = nullptr;
    }
    if (m_service) {
        obs_service_release(m_service);
        m_service = nullptr;
    }
}

// ── Private: helpers ──────────────────────────────────────────────────────────

std::string StreamTarget::make_unique_name(const char *prefix) const
{
    std::ostringstream ss;
    ss << prefix << "_" << m_index;
    return ss.str();
}

// ── Signal handlers ───────────────────────────────────────────────────────────

void StreamTarget::on_start(void *param, calldata_t *)
{
    auto *self = static_cast<StreamTarget *>(param);
    self->m_active = true;
    std::lock_guard<std::mutex> lk(self->m_stats_mutex);
    self->m_stats.is_active       = true;
    self->m_stats.has_error       = false;
    self->m_stats.error_message   = {};
    mlog_info("StreamTarget[%d] output started", self->m_index);
}

void StreamTarget::on_stop(void *param, calldata_t *data)
{
    auto *self = static_cast<StreamTarget *>(param);
    self->m_active = false;

    long long code = 0;
    const char *last_error = nullptr;
    calldata_get_int(data, "code", &code);
    calldata_get_string(data, "last_error", &last_error);

    std::lock_guard<std::mutex> lk(self->m_stats_mutex);
    self->m_stats.is_active = false;

    if (code != OBS_OUTPUT_SUCCESS) {
        self->m_stats.has_error     = true;
        self->m_stats.error_message = last_error ? last_error : "Unknown error";
        mlog_error("StreamTarget[%d] stopped with error (code %d): %s",
                   self->m_index, code, self->m_stats.error_message.c_str());
    } else {
        mlog_info("StreamTarget[%d] stopped cleanly", self->m_index);
    }
}

void StreamTarget::on_reconnect(void *param, calldata_t *)
{
    auto *self = static_cast<StreamTarget *>(param);
    std::lock_guard<std::mutex> lk(self->m_stats_mutex);
    self->m_stats.reconnect_count++;
    mlog_warn("StreamTarget[%d] reconnecting... (attempt %d)",
              self->m_index, self->m_stats.reconnect_count);
}

void StreamTarget::on_reconnect_success(void *param, calldata_t *)
{
    auto *self = static_cast<StreamTarget *>(param);
    mlog_info("StreamTarget[%d] reconnected successfully", self->m_index);
}
