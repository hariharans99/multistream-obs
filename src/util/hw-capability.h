#pragma once

#include <string>
#include <vector>

// ── Encoder type enum ─────────────────────────────────────────────────────────
enum class EncoderType {
    Auto,     // Probe at runtime and pick best
    x264,     // Software fallback (always available)
    NVENC,    // NVIDIA hardware
    AMF,      // AMD hardware
    QSV       // Intel QuickSync
};

// ── Result of an encoder probe ────────────────────────────────────────────────
struct EncoderCapabilities {
    bool has_nvenc       = false;
    bool has_amf         = false;
    bool has_qsv         = false;

    // NVENC-specific: consumer GPUs allow a maximum of 5 concurrent sessions
    int  nvenc_max_sessions     = 5;
    int  nvenc_active_sessions  = 0;  // Sessions already in use by OBS + others

    // obs encoder IDs we resolved at probe time
    std::string nvenc_id;   // e.g. "jim_nvenc" or "ffmpeg_nvenc"
    std::string amf_id;     // e.g. "amd_amf_h264"
    std::string qsv_id;     // e.g. "obs_qsv11"
};

// ── Public API ────────────────────────────────────────────────────────────────

/**
 * Probe the system for available hardware encoders.
 * Call once at plugin load time; results are cached.
 */
const EncoderCapabilities &hw_probe_capabilities();

/**
 * Given a user preference, resolve the actual obs encoder ID to use.
 * Falls back to x264 if the requested hardware encoder is unavailable.
 *
 * @param preferred   The user-preferred encoder type.
 * @param[out] obs_id The resolved obs encoder ID string (e.g. "jim_nvenc").
 * @return            The resolved encoder type (may differ from preferred if
 *                    fallback occurred).
 */
EncoderType hw_resolve_encoder(EncoderType preferred, std::string &obs_id);

/**
 * Check whether adding one more NVENC session would exceed the system limit.
 * Used to show a warning badge in the UI.
 */
bool hw_nvenc_would_exceed_limit(int sessions_we_want_to_add = 1);

/**
 * Return a human-readable label for the encoder type.
 */
const char *hw_encoder_label(EncoderType type);
