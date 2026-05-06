#include "hw-capability.h"
#include "logger.h"
#include <obs.h>
#include <mutex>

// ── Cached probe result ───────────────────────────────────────────────────────
static EncoderCapabilities g_caps;
static std::once_flag       g_probe_once;

// Encoder IDs to probe — ordered by preference within each vendor
static constexpr const char *NVENC_IDS[] = {
    "jim_nvenc",        // OBS built-in NVENC (preferred, lower overhead)
    "ffmpeg_nvenc",     // FFmpeg-backed NVENC
    nullptr
};
static constexpr const char *AMF_IDS[] = {
    "amd_amf_h264",
    "h264_texture_amf",
    nullptr
};
static constexpr const char *QSV_IDS[] = {
    "obs_qsv11",
    "obs_qsv11_v2",
    nullptr
};

// ── Helper: check if an encoder ID is registered in this OBS build ───────────
static bool encoder_registered(const char *id)
{
    // obs_get_encoder_codec returns nullptr if not found
    return (id != nullptr) && (obs_get_encoder_codec(id) != nullptr);
}

static std::string probe_first_available(const char * const ids[])
{
    for (int i = 0; ids[i] != nullptr; ++i) {
        if (encoder_registered(ids[i]))
            return ids[i];
    }
    return {};
}

// ── NVENC active session count (Windows only, best-effort) ────────────────────
#ifdef _WIN32
#include <windows.h>
static int nvenc_count_active_sessions()
{
    // Query the NVENC session count via registry key left by the NVIDIA driver.
    // Key: HKLM\SOFTWARE\NVIDIA Corporation\Global\NvTweak\Streams
    // This is a best-effort heuristic — it may not be available on all drivers.
    HKEY hKey = nullptr;
    DWORD count = 0;
    DWORD size  = sizeof(count);

    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE,
                      "SOFTWARE\\NVIDIA Corporation\\Global\\NvTweak",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        RegQueryValueExA(hKey, "ActiveEncodeSessionCount",
                         nullptr, nullptr,
                         reinterpret_cast<LPBYTE>(&count), &size);
        RegCloseKey(hKey);
    }

    // Fallback: assume 0 if key not found
    return static_cast<int>(count);
}
#else
static int nvenc_count_active_sessions() { return 0; }
#endif

// ── Main probe function ───────────────────────────────────────────────────────
const EncoderCapabilities &hw_probe_capabilities()
{
    std::call_once(g_probe_once, []() {
        g_caps.nvenc_id = probe_first_available(NVENC_IDS);
        g_caps.amf_id   = probe_first_available(AMF_IDS);
        g_caps.qsv_id   = probe_first_available(QSV_IDS);

        g_caps.has_nvenc = !g_caps.nvenc_id.empty();
        g_caps.has_amf   = !g_caps.amf_id.empty();
        g_caps.has_qsv   = !g_caps.qsv_id.empty();

        g_caps.nvenc_active_sessions = nvenc_count_active_sessions();

        mlog_info("Encoder probe complete:");
        mlog_info("  NVENC : %s (%s)",
                  g_caps.has_nvenc ? g_caps.nvenc_id.c_str() : "NOT FOUND",
                  g_caps.has_nvenc ? "available" : "unavailable");
        mlog_info("  AMF   : %s (%s)",
                  g_caps.has_amf ? g_caps.amf_id.c_str() : "NOT FOUND",
                  g_caps.has_amf ? "available" : "unavailable");
        mlog_info("  QSV   : %s (%s)",
                  g_caps.has_qsv ? g_caps.qsv_id.c_str() : "NOT FOUND",
                  g_caps.has_qsv ? "available" : "unavailable");
        mlog_info("  NVENC active sessions (system): %d / %d",
                  g_caps.nvenc_active_sessions, g_caps.nvenc_max_sessions);
    });

    return g_caps;
}

// ── Encoder resolution ────────────────────────────────────────────────────────
EncoderType hw_resolve_encoder(EncoderType preferred, std::string &obs_id)
{
    const auto &caps = hw_probe_capabilities();

    auto try_nvenc = [&]() -> bool {
        if (caps.has_nvenc) { obs_id = caps.nvenc_id; return true; }
        return false;
    };
    auto try_amf = [&]() -> bool {
        if (caps.has_amf) { obs_id = caps.amf_id; return true; }
        return false;
    };
    auto try_qsv = [&]() -> bool {
        if (caps.has_qsv) { obs_id = caps.qsv_id; return true; }
        return false;
    };
    auto use_x264 = [&]() {
        obs_id = "obs_x264";
    };

    switch (preferred) {
    case EncoderType::NVENC:
        if (try_nvenc()) return EncoderType::NVENC;
        mlog_warn("NVENC not available; falling back to x264");
        use_x264();
        return EncoderType::x264;

    case EncoderType::AMF:
        if (try_amf()) return EncoderType::AMF;
        mlog_warn("AMF not available; falling back to x264");
        use_x264();
        return EncoderType::x264;

    case EncoderType::QSV:
        if (try_qsv()) return EncoderType::QSV;
        mlog_warn("QSV not available; falling back to x264");
        use_x264();
        return EncoderType::x264;

    case EncoderType::Auto:
        // Priority: NVENC → AMF → QSV → x264
        if (try_nvenc()) return EncoderType::NVENC;
        if (try_amf())   return EncoderType::AMF;
        if (try_qsv())   return EncoderType::QSV;
        use_x264();
        return EncoderType::x264;

    case EncoderType::x264:
    default:
        use_x264();
        return EncoderType::x264;
    }
}

bool hw_nvenc_would_exceed_limit(int sessions_to_add)
{
    const auto &caps = hw_probe_capabilities();
    return (caps.nvenc_active_sessions + sessions_to_add) > caps.nvenc_max_sessions;
}

const char *hw_encoder_label(EncoderType type)
{
    switch (type) {
    case EncoderType::NVENC: return "NVENC";
    case EncoderType::AMF:   return "AMF";
    case EncoderType::QSV:   return "QSV";
    case EncoderType::x264:  return "x264";
    case EncoderType::Auto:  return "Auto";
    default:                 return "Unknown";
    }
}
