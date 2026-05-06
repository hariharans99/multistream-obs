#pragma once

#include "stream-config.h"
#include "util/hw-capability.h"
#include <obs.h>
#include <memory>
#include <atomic>
#include <mutex>
#include <string>

/**
 * StreamTarget — owns a single output stream:
 *  - One video encoder (at the configured resolution + bitrate)
 *  - One audio encoder (AAC at the configured bitrate)
 *  - One RTMP output
 *
 * Lifecycle:
 *   create (ctor) → configure() → start() → [running] → stop() → ~StreamTarget()
 */
class StreamTarget {
public:
    explicit StreamTarget(int index, const StreamConfig &cfg);
    ~StreamTarget();

    // Non-copyable, movable
    StreamTarget(const StreamTarget &)            = delete;
    StreamTarget &operator=(const StreamTarget &) = delete;
    StreamTarget(StreamTarget &&)                 = default;

    // ── Control ───────────────────────────────────────────────────────────────
    bool start();
    void stop();

    /**
     * Update configuration while stopped.
     * Call stop() before update() and start() after if stream is active.
     */
    void update(const StreamConfig &cfg);

    // ── Status ────────────────────────────────────────────────────────────────
    bool         is_active()  const { return m_active.load(); }
    int          index()      const { return m_index; }
    StreamStats  get_stats()  const;
    const StreamConfig &config() const { return m_config; }

private:
    // ── OBS object lifecycle ─────────────────────────────────────────────────
    bool create_video_encoder();
    bool create_audio_encoder();
    bool create_output();
    void destroy_encoders();
    void destroy_output();

    // ── Helpers ───────────────────────────────────────────────────────────────
    std::string make_unique_name(const char *prefix) const;

    // ── Fields ────────────────────────────────────────────────────────────────
    int                         m_index;
    StreamConfig                m_config;
    std::string                 m_resolved_encoder_id;
    EncoderType                 m_resolved_encoder_type;

    obs_output_t               *m_output        = nullptr;
    obs_encoder_t              *m_video_encoder  = nullptr;
    obs_encoder_t              *m_audio_encoder  = nullptr;
    obs_service_t              *m_service        = nullptr;

    std::atomic<bool>           m_active{false};
    mutable std::mutex          m_stats_mutex;
    StreamStats                 m_stats;
    mutable uint64_t            m_prev_bytes    = 0;  // for net_bitrate_kbps delta
    mutable uint64_t            m_prev_time_ns  = 0;
    mutable double              m_cached_bitrate_kbps = 0.0;

    // OBS output signal callbacks (static, use param to get `this`)
    static void on_start(void *param, calldata_t *data);
    static void on_stop(void *param, calldata_t *data);
    static void on_reconnect(void *param, calldata_t *data);
    static void on_reconnect_success(void *param, calldata_t *data);
};
