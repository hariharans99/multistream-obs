#include <obs-module.h>
#include <obs-frontend-api.h>
#include "multistream-output.h"
#include "settings-ui.h"
#include "util/logger.h"
#include "util/hw-capability.h"

OBS_DECLARE_MODULE()
OBS_MODULE_USE_DEFAULT_LOCALE("obs-multistream-plugin", "en-US")
MODULE_EXPORT const char *obs_module_description(void)
{
    return "Multi-Stream Output: simultaneously stream to multiple RTMP destinations "
           "with independent resolutions, bitrates, and aspect ratios.";
}

static void frontend_save_cb(obs_data_t *save_data, bool saving, void *)
{
    if (saving) {
        MultistreamOutput::instance()->save_settings(save_data);
    } else {
        MultistreamOutput::instance()->load_settings(save_data);
    }
}

// ── Module load ───────────────────────────────────────────────────────────────

bool obs_module_load(void)
{
    mlog_info("Module loading...");

    // Probe hardware encoders early (result is cached)
    const auto &caps = hw_probe_capabilities();
    mlog_info("Hardware encoders: NVENC=%s, AMF=%s, QSV=%s",
              caps.has_nvenc ? "YES" : "NO",
              caps.has_amf   ? "YES" : "NO",
              caps.has_qsv   ? "YES" : "NO");


    // Register our custom output type with libobs
    MultistreamOutput::register_output();

#ifdef ENABLE_FRONTEND_API
    // Add the dock widget to the OBS main window
    obs_frontend_add_dock_by_id("multistream-dock",
                                 obs_module_text("MultiStreamDock.Title"),
                                 new MultistreamDock());
    mlog_info("Dock registered: 'multistream-dock'");

    // Register config save/load callback
    obs_frontend_add_save_callback(frontend_save_cb, nullptr);
#endif

    mlog_info("Module loaded successfully.");
    return true;
}

// ── Module unload ─────────────────────────────────────────────────────────────

void obs_module_unload(void)
{
    mlog_info("Module unloading — stopping and destroying all streams...");
    MultistreamOutput::instance()->destroy_all();
    

    mlog_info("Module unloaded.");
}
