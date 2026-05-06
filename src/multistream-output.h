#pragma once

#include "stream-target.h"
#include "stream-config.h"
#include <obs.h>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>

/**
 * MultistreamOutput — top-level plugin output manager.
 *
 * Owns a list of StreamTarget objects.  Provides the obs_output_info
 * callbacks required by libobs so it integrates cleanly into the OBS
 * output lifecycle (Tools → Output Settings, etc.).
 *
 * Usage:
 *   MultistreamOutput *mgr = MultistreamOutput::instance();
 *   mgr->add_target(config);
 *   mgr->start_all();
 *   // ...
 *   mgr->stop_all();
 */
class MultistreamOutput {
public:
    // ── Singleton access ──────────────────────────────────────────────────────
    static MultistreamOutput *instance();

    // ── Target management ─────────────────────────────────────────────────────
    /** Add a new stream target. Returns the index of the new target. */
    int  add_target(const StreamConfig &cfg);

    /** Remove a target by index.  Stops it first if active. */
    bool remove_target(int index);

    /** Update the config of an existing target (must be stopped). */
    bool update_target(int index, const StreamConfig &cfg);

    /** Return a snapshot of all current configs (thread-safe). */
    std::vector<StreamConfig> get_configs() const;

    /** Return stats for all targets (thread-safe). */
    std::vector<StreamStats> get_stats() const;

    int target_count() const;

    // ── Saving/Loading ────────────────────────────────────────────────────────
    void load_settings(obs_data_t *data);
    void save_settings(obs_data_t *data) const;

    // ── Lifecycle ─────────────────────────────────────────────────────────────
    bool start_all();
    void stop_all();
    void destroy_all();

    bool start_target(int index);
    void stop_target(int index);

    // ── Callbacks (called from UI to refresh the table) ───────────────────────
    using StatsCallback = std::function<void()>;
    void set_stats_callback(StatsCallback cb) { m_stats_cb = std::move(cb); }

    // ── OBS output registration ───────────────────────────────────────────────
    static void register_output();

private:
    MultistreamOutput() = default;

    std::vector<std::unique_ptr<StreamTarget>> m_targets;
    mutable std::mutex                          m_mutex;
    StatsCallback                               m_stats_cb;

    // OBS output info callbacks
    static const char   *get_name(void *type_data);
    static void         *create(obs_data_t *settings, obs_output_t *output);
    static void          destroy(void *data);
    static bool          start(void *data);
    static void          stop(void *data, uint64_t ts);
    static obs_properties_t *get_properties(void *data);
};
