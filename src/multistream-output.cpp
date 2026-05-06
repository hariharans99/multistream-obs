#include "multistream-output.h"
#include "util/logger.h"
#include <obs.h>
#include <algorithm>

// ── Singleton ─────────────────────────────────────────────────────────────────

MultistreamOutput *MultistreamOutput::instance()
{
    static MultistreamOutput s_instance;
    return &s_instance;
}

// ── Target management ─────────────────────────────────────────────────────────

int MultistreamOutput::add_target(const StreamConfig &cfg)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    int idx = static_cast<int>(m_targets.size());
    m_targets.push_back(std::make_unique<StreamTarget>(idx, cfg));
    mlog_info("MultistreamOutput: added target %d (%s)", idx, cfg.label.c_str());
    return idx;
}

bool MultistreamOutput::remove_target(int index)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (index < 0 || index >= static_cast<int>(m_targets.size())) return false;

    auto &t = m_targets[static_cast<size_t>(index)];
    if (t->is_active()) t->stop();

    m_targets.erase(m_targets.begin() + index);

    // Re-number remaining targets
    for (int i = index; i < static_cast<int>(m_targets.size()); ++i) {
        // StreamTarget stores its index for logging; update after vector shift
        // (index is read-only via index() — simply accepted as cosmetic shift)
    }

    mlog_info("MultistreamOutput: removed target %d", index);
    return true;
}

bool MultistreamOutput::update_target(int index, const StreamConfig &cfg)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (index < 0 || index >= static_cast<int>(m_targets.size())) return false;
    m_targets[static_cast<size_t>(index)]->update(cfg);
    return true;
}

std::vector<StreamConfig> MultistreamOutput::get_configs() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<StreamConfig> result;
    result.reserve(m_targets.size());
    for (const auto &t : m_targets)
        result.push_back(t->config());
    return result;
}

std::vector<StreamStats> MultistreamOutput::get_stats() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    std::vector<StreamStats> result;
    result.reserve(m_targets.size());
    for (const auto &t : m_targets)
        result.push_back(t->get_stats());
    return result;
}

int MultistreamOutput::target_count() const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    return static_cast<int>(m_targets.size());
}

// ── Saving/Loading ────────────────────────────────────────────────────────────

void MultistreamOutput::load_settings(obs_data_t *data)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    
    // Stop and clear existing targets before loading
    for (auto &t : m_targets) {
        if (t->is_active()) t->stop();
    }
    m_targets.clear();

    obs_data_array_t *array = obs_data_get_array(data, "multistream_targets");
    if (!array) return;

    size_t count = obs_data_array_count(array);
    for (size_t i = 0; i < count; ++i) {
        obs_data_t *item = obs_data_array_item(array, i);
        if (!item) continue;

        StreamConfig cfg;
        cfg.label = obs_data_get_string(item, "label");
        cfg.rtmp_url = obs_data_get_string(item, "rtmp_url");
        cfg.stream_key = obs_data_get_string(item, "stream_key");
        cfg.chat_url = obs_data_get_string(item, "chat_url");
        cfg.width = (uint32_t)obs_data_get_int(item, "width");
        cfg.height = (uint32_t)obs_data_get_int(item, "height");
        cfg.bitrate_kbps = (uint32_t)obs_data_get_int(item, "bitrate_kbps");
        cfg.audio_bitrate_kbps = (uint32_t)obs_data_get_int(item, "audio_bitrate_kbps");
        cfg.encoder_pref = (EncoderType)obs_data_get_int(item, "encoder_pref");
        cfg.scale_mode   = (ScalingMode)obs_data_get_int(item, "scale_mode");
        cfg.sharpening   = (float)obs_data_get_double(item, "sharpening");
        cfg.enabled = obs_data_get_bool(item, "enabled");

        // Validate missing or invalid values
        if (cfg.width == 0) cfg.width = 1280;
        if (cfg.height == 0) cfg.height = 720;
        if (cfg.bitrate_kbps == 0) cfg.bitrate_kbps = 3000;
        if (cfg.audio_bitrate_kbps == 0) cfg.audio_bitrate_kbps = 160;

        m_targets.push_back(std::make_unique<StreamTarget>(static_cast<int>(m_targets.size()), cfg));
        obs_data_release(item);
    }
    obs_data_array_release(array);
    
    mlog_info("MultistreamOutput: loaded %d targets from config", static_cast<int>(m_targets.size()));
}

void MultistreamOutput::save_settings(obs_data_t *data) const
{
    std::lock_guard<std::mutex> lk(m_mutex);
    obs_data_array_t *array = obs_data_array_create();

    for (const auto &t : m_targets) {
        obs_data_t *item = obs_data_create();
        const auto &cfg = t->config();

        obs_data_set_string(item, "label", cfg.label.c_str());
        obs_data_set_string(item, "rtmp_url", cfg.rtmp_url.c_str());
        obs_data_set_string(item, "stream_key", cfg.stream_key.c_str());
        obs_data_set_string(item, "chat_url", cfg.chat_url.c_str());
        obs_data_set_int(item, "width", cfg.width);
        obs_data_set_int(item, "height", cfg.height);
        obs_data_set_int(item, "bitrate_kbps", cfg.bitrate_kbps);
        obs_data_set_int(item, "audio_bitrate_kbps", cfg.audio_bitrate_kbps);
        obs_data_set_int(item, "encoder_pref", (int)cfg.encoder_pref);
        obs_data_set_int(item, "scale_mode", (int)cfg.scale_mode);
        obs_data_set_double(item, "sharpening", (double)cfg.sharpening);
        obs_data_set_bool(item, "enabled", cfg.enabled);

        obs_data_array_push_back(array, item);
        obs_data_release(item);
    }

    obs_data_set_array(data, "multistream_targets", array);
    obs_data_array_release(array);
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

bool MultistreamOutput::start_all()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    bool all_ok = true;
    for (auto &t : m_targets) {
        if (!t->is_active()) {
            if (!t->start()) {
                mlog_error("MultistreamOutput: failed to start target %d", t->index());
                all_ok = false;
            }
        }
    }
    return all_ok;
}

void MultistreamOutput::stop_all()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto &t : m_targets) {
        if (t->is_active()) t->stop();
    }
    mlog_info("MultistreamOutput: all targets stopped");
}

void MultistreamOutput::destroy_all()
{
    std::lock_guard<std::mutex> lk(m_mutex);
    for (auto &t : m_targets) {
        if (t->is_active()) t->stop();
    }
    m_targets.clear();
    mlog_info("MultistreamOutput: all targets destroyed");
}

bool MultistreamOutput::start_target(int index)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (index < 0 || index >= static_cast<int>(m_targets.size())) return false;
    return m_targets[static_cast<size_t>(index)]->start();
}

void MultistreamOutput::stop_target(int index)
{
    std::lock_guard<std::mutex> lk(m_mutex);
    if (index < 0 || index >= static_cast<int>(m_targets.size())) return;
    m_targets[static_cast<size_t>(index)]->stop();
}

// ── OBS output registration ───────────────────────────────────────────────────

void MultistreamOutput::register_output()
{
    static obs_output_info info = {};
    info.id           = "multistream_output";
    info.flags        = OBS_OUTPUT_AV | OBS_OUTPUT_SERVICE;
    info.get_name     = get_name;
    info.create       = create;
    info.destroy      = destroy;
    info.start        = start;
    info.stop         = stop;
    info.get_properties = get_properties;

    obs_register_output(&info);
    mlog_info("MultistreamOutput: registered OBS output type 'multistream_output'");
}

// ── OBS callbacks ─────────────────────────────────────────────────────────────

const char *MultistreamOutput::get_name(void *)
{
    return "Multi-Stream Output";
}

void *MultistreamOutput::create(obs_data_t *, obs_output_t *)
{
    // We return the singleton as the context pointer
    return MultistreamOutput::instance();
}

void MultistreamOutput::destroy(void *)
{
    // Singleton — nothing to delete; stop_all on module unload
    MultistreamOutput::instance()->stop_all();
}

bool MultistreamOutput::start(void *)
{
    return MultistreamOutput::instance()->start_all();
}

void MultistreamOutput::stop(void *, uint64_t)
{
    MultistreamOutput::instance()->stop_all();
}

obs_properties_t *MultistreamOutput::get_properties(void *)
{
    // Properties shown in the OBS Output Settings dialog (minimal — main UI is the dock)
    obs_properties_t *props = obs_properties_create();
    obs_properties_add_text(props, "info",
        "Configure streams using the Multi-Stream dock panel.",
        OBS_TEXT_INFO);
    return props;
}
