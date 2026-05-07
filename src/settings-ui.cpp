#include "settings-ui.h"
#include "util/hw-capability.h"
#include "util/logger.h"
#include <obs-module.h>
#include <util/platform.h>
#include <QHeaderView>
#include <QMessageBox>
#include <QGroupBox>
#include <QSlider>
#include <obs-frontend-api.h>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QSizePolicy>
#include <QGridLayout>
#include <QScrollArea>
#include <QString>
#include <sstream>
#include <QDesktopServices>
#include <QUrl>
#include <QInputDialog>

// ═══════════════════════════════════════════════════════════════════════════════
// StreamDialog
// ═══════════════════════════════════════════════════════════════════════════════

const std::vector<std::pair<QString, std::pair<uint32_t, uint32_t>>>
StreamDialog::PRESETS = {
    { "3840×2160 (4K)",   {3840, 2160} },
    { "2560×1440 (1440p)",{2560, 1440} },
    { "1920×1080 (1080p)",{1920, 1080} },
    { "1280×720  (720p)", {1280,  720} },
    { "854×480   (480p)", { 854,  480} },
    { "640×360   (360p)", { 640,  360} },
    { "720×1280  (9:16 HD)",  { 720, 1280} },
    { "1080×1920 (9:16 FHD)",{1080, 1920} },
    { "1080×1080 (1:1 FHD)", {1080, 1080} },
    { "Custom…",          {0, 0} },
};

StreamDialog::StreamDialog(QWidget *parent, const StreamConfig &cfg)
    : QDialog(parent)
{
    setWindowTitle(obs_module_text("StreamDialog.Title"));
    setModal(true);
    setMinimumWidth(480);
    setup_ui();
    populate(cfg);
}

void StreamDialog::setup_ui()
{
    auto *root = new QVBoxLayout(this);

    // ── Destination ───────────────────────────────────────────────────────────
    auto *dest_grp = new QGroupBox("Destination", this);
    auto *dest_form = new QFormLayout(dest_grp);

    m_label_edit = new QLineEdit(this);
    m_label_edit->setPlaceholderText("Stream 1");
    dest_form->addRow("Label", m_label_edit);

    m_platform_combo = new QComboBox(this);
    m_platform_combo->addItem("YouTube Live", "youtube");
    m_platform_combo->addItem("Twitch",       "twitch");
    m_platform_combo->addItem("Kick",         "kick");
    m_platform_combo->addItem("Facebook",     "facebook");
    m_platform_combo->addItem("TikTok",       "tiktok");
    m_platform_combo->addItem("Instagram",    "instagram");
    m_platform_combo->addItem("X (Twitter)",  "x");
    m_platform_combo->addItem("Trovo",        "trovo");
    dest_form->addRow("Target Platform", m_platform_combo);

    m_url_edit = new QLineEdit(this);
    m_url_edit->setPlaceholderText("rtmp://live.twitch.tv/app");
    dest_form->addRow(obs_module_text("StreamDialog.Label.URL"), m_url_edit);

    m_key_edit = new QLineEdit(this);
    m_key_edit->setEchoMode(QLineEdit::Password);
    m_key_edit->setPlaceholderText("live_xxxxx_xxxx");
    dest_form->addRow(obs_module_text("StreamDialog.Label.StreamKey"), m_key_edit);

    m_chat_edit = new QLineEdit(this);
    m_chat_edit->setPlaceholderText("https://...");
    dest_form->addRow(obs_module_text("StreamDialog.Label.ChatUrl"), m_chat_edit);

    // ── Quick Presets ──────────────────────────────────────────────────────────
    auto *preset_grp = new QGroupBox("Quick Presets", this);
    auto *preset_layout = new QHBoxLayout(preset_grp);
    
    auto *ultra_btn = new QPushButton("Ultra Quality (4K)", this);
    auto *high_btn  = new QPushButton("High Quality (1080p)", this);
    auto *med_btn   = new QPushButton("Medium Quality (720p)", this);
    auto *norm_btn  = new QPushButton("Normal Quality (480p)", this);

    ultra_btn->setStyleSheet("background-color: #3d3d4c; font-weight: bold;");
    high_btn->setStyleSheet("background-color: #3d4c3d; font-weight: bold;");
    med_btn->setStyleSheet("background-color: #4c4c3d; font-weight: bold;");
    norm_btn->setStyleSheet("background-color: #4c3d3d; font-weight: bold;");

    preset_layout->addWidget(ultra_btn);
    preset_layout->addWidget(high_btn);
    preset_layout->addWidget(med_btn);
    preset_layout->addWidget(norm_btn);

    auto apply_quality = [this](int res_idx, int bitrate, int audio_bitrate, float sharpening) {
        m_res_combo->setCurrentIndex(res_idx);
        m_bitrate_spin->setValue(bitrate);
        m_audio_bitrate_spin->setValue(audio_bitrate);
        m_sharpen_slider->setValue((int)(sharpening * 100.0f));
    };

    auto on_preset_clicked = [this, apply_quality](int level) {
        QString platform = m_platform_combo->currentData().toString();
        
        // level: 0=Ultra, 1=High, 2=Medium, 3=Low
        
        if (platform == "youtube") {
            if (level == 0) apply_quality(0, 40000, 192, 0.50f); // 4K
            if (level == 1) apply_quality(2, 10000, 128, 0.40f); // 1080p
            if (level == 2) apply_quality(3, 6000,  128, 0.25f); // 720p
            if (level == 3) apply_quality(4, 2500,  128, 0.10f); // 480p
        }
        else if (platform == "twitch" || platform == "kick" || platform == "x" || platform == "trovo") {
            // Standard H.264 platforms
            if (level == 0) apply_quality(2, 8000, 160, 0.40f); // 1080p Max
            if (level == 1) apply_quality(2, 6000, 128, 0.35f); // 1080p Rec
            if (level == 2) apply_quality(3, 4500, 128, 0.25f); // 720p60
            if (level == 3) apply_quality(3, 2500, 128, 0.15f); // 720p30
        }
        else if (platform == "facebook") {
            if (level == 0) apply_quality(2, 9000, 128, 0.40f); // 1080p Max
            if (level == 1) apply_quality(2, 6000, 128, 0.35f); // 1080p Rec
            if (level == 2) apply_quality(3, 4000, 128, 0.25f); // 720p
            if (level == 3) apply_quality(4, 1500, 128, 0.10f); // 480p
        }
        else if (platform == "tiktok" || platform == "instagram") {
            // Vertical specialized
            if (level == 0) apply_quality(7, 8000, 128, 0.45f); // 1080x1920 Max
            if (level == 1) apply_quality(7, 6000, 128, 0.40f); // 1080x1920 Rec
            if (level == 2) apply_quality(6, 3500, 128, 0.25f); // 720x1280
            if (level == 3) apply_quality(6, 1500, 128, 0.15f); // 720x1280 Low
        }
    };

    connect(ultra_btn, &QPushButton::clicked, [=]() { on_preset_clicked(0); });
    connect(high_btn,  &QPushButton::clicked, [=]() { on_preset_clicked(1); });
    connect(med_btn,   &QPushButton::clicked, [=]() { on_preset_clicked(2); });
    connect(norm_btn,  &QPushButton::clicked, [=]() { on_preset_clicked(3); });

    // ── Video ─────────────────────────────────────────────────────────────────
    auto *vid_grp  = new QGroupBox("Video", this);
    auto *vid_form = new QFormLayout(vid_grp);

    m_res_combo = new QComboBox(this);
    for (const auto &p : PRESETS)
        m_res_combo->addItem(p.first);

    auto *res_row = new QHBoxLayout;
    res_row->addWidget(m_res_combo);

    m_custom_w = new QSpinBox(this);
    m_custom_w->setRange(160, 7680);
    m_custom_w->setSingleStep(2);
    m_custom_w->setPrefix("W: ");

    m_custom_h = new QSpinBox(this);
    m_custom_h->setRange(90, 4320);
    m_custom_h->setSingleStep(2);
    m_custom_h->setPrefix("H: ");

    res_row->addWidget(m_custom_w);
    res_row->addWidget(m_custom_h);
    vid_form->addRow(obs_module_text("StreamDialog.Label.Resolution"), res_row);

    m_bitrate_spin = new QSpinBox(this);
    m_bitrate_spin->setRange(100, 50000);
    m_bitrate_spin->setSingleStep(500);
    m_bitrate_spin->setSuffix(" kbps");
    m_bitrate_spin->setValue(3000);
    vid_form->addRow(obs_module_text("StreamDialog.Label.Bitrate"), m_bitrate_spin);

    m_encoder_combo = new QComboBox(this);
    m_encoder_combo->addItem(obs_module_text("StreamDialog.Encoder.Auto"),   (int)EncoderType::Auto);
    m_encoder_combo->addItem(obs_module_text("StreamDialog.Encoder.x264"),   (int)EncoderType::x264);
    m_encoder_combo->addItem(obs_module_text("StreamDialog.Encoder.NVENC"),  (int)EncoderType::NVENC);
    m_encoder_combo->addItem(obs_module_text("StreamDialog.Encoder.AMF"),    (int)EncoderType::AMF);
    m_encoder_combo->addItem(obs_module_text("StreamDialog.Encoder.QSV"),    (int)EncoderType::QSV);
    vid_form->addRow(obs_module_text("StreamDialog.Label.Encoder"), m_encoder_combo);

    m_fps_combo = new QComboBox(this);
    m_fps_combo->addItem(obs_module_text("StreamDialog.FPS.Auto"), 0);
    m_fps_combo->addItem("60", 60);
    m_fps_combo->addItem("59.94", 59); // value will be handled in target
    m_fps_combo->addItem("50", 50);
    m_fps_combo->addItem("30", 30);
    m_fps_combo->addItem("29.97", 29);
    m_fps_combo->addItem("24", 24);
    vid_form->addRow(obs_module_text("StreamDialog.Label.FPS"), m_fps_combo);
    
    m_scale_combo = new QComboBox(this);
    m_scale_combo->addItem("Auto (Recommended)",         (int)ScalingMode::Auto);
    m_scale_combo->addItem("AMD FSR (Spatial)",          (int)ScalingMode::FSR);
    m_scale_combo->addItem("NVIDIA NIS",                 (int)ScalingMode::NIS);
    m_scale_combo->addItem("Adaptive Sharpening (CAS)", (int)ScalingMode::CAS);
    m_scale_combo->addItem("Lanczos",                    (int)ScalingMode::Lanczos);
    m_scale_combo->addItem("Bicubic",                    (int)ScalingMode::Bicubic);
    m_scale_combo->addItem("Area",                       (int)ScalingMode::Area);
    m_scale_combo->addItem("Bilinear",                   (int)ScalingMode::Bilinear);
    m_scale_combo->addItem("Point",                      (int)ScalingMode::Point);

    m_scale_combo->setItemData(0, "Intelligently select the best filter based on resolution change.", Qt::ToolTipRole);
    m_scale_combo->setItemData(1, "AMD FidelityFX Super Resolution: High-quality spatial upscaling.", Qt::ToolTipRole);
    m_scale_combo->setItemData(2, "NVIDIA Image Scaling: High-quality spatial upscaling and sharpening.", Qt::ToolTipRole);
    m_scale_combo->setItemData(3, "Contrast Adaptive Sharpening: Sharpens without halos or artifacts.", Qt::ToolTipRole);
    m_scale_combo->setItemData(4, "Sharpest scaling, best for downscaling text/HUDs.", Qt::ToolTipRole);
    m_scale_combo->setItemData(5, "Balanced quality and performance.", Qt::ToolTipRole);
    m_scale_combo->setItemData(6, "Weighted average, good for downscaling video.", Qt::ToolTipRole);
    m_scale_combo->setItemData(7, "Fastest scaling, lower quality.", Qt::ToolTipRole);
    m_scale_combo->setItemData(8, "Pixel-perfect nearest-neighbor scaling.", Qt::ToolTipRole);

    vid_form->addRow("Scaling Filter", m_scale_combo);

    m_sharpen_slider = new QSlider(Qt::Horizontal, this);
    m_sharpen_slider->setRange(0, 100);
    
    m_sharpen_label = new QLabel("0%", this);
    m_sharpen_label->setMinimumWidth(35);
    
    auto *sharpen_layout = new QHBoxLayout();
    sharpen_layout->addWidget(m_sharpen_slider);
    sharpen_layout->addWidget(m_sharpen_label);
    
    vid_form->addRow("Sharpening Amount", sharpen_layout);

    connect(m_sharpen_slider, &QSlider::valueChanged, [this](int val) {
        m_sharpen_label->setText(QString("%1%").arg(val));
    });

    // ── Audio ─────────────────────────────────────────────────────────────────
    auto *aud_grp  = new QGroupBox("Audio", this);
    auto *aud_form = new QFormLayout(aud_grp);

    m_audio_bitrate_spin = new QSpinBox(this);
    m_audio_bitrate_spin->setRange(32, 320);
    m_audio_bitrate_spin->setSingleStep(32);
    m_audio_bitrate_spin->setSuffix(" kbps");
    m_audio_bitrate_spin->setValue(160);
    aud_form->addRow(obs_module_text("StreamDialog.Label.AudioBitrate"), m_audio_bitrate_spin);

    // ── Buttons ───────────────────────────────────────────────────────────────
    auto *btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(btns, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, this, &QDialog::reject);

    // Toggle custom size fields
    connect(m_res_combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int idx) {
                bool custom = (idx == (int)PRESETS.size() - 1);
                m_custom_w->setVisible(custom);
                m_custom_h->setVisible(custom);
                if (!custom) {
                    auto [w, h] = PRESETS[idx].second;
                    m_custom_w->setValue(w);
                    m_custom_h->setValue(h);
                }
            });

    root->addWidget(dest_grp);
    root->addWidget(preset_grp);
    root->addWidget(vid_grp);
    root->addWidget(aud_grp);
    root->addWidget(btns);
}

void StreamDialog::populate(const StreamConfig &cfg)
{
    m_label_edit->setText(QString::fromStdString(cfg.label));
    m_url_edit->setText(QString::fromStdString(cfg.rtmp_url));
    m_key_edit->setText(QString::fromStdString(cfg.stream_key));
    m_chat_edit->setText(QString::fromStdString(cfg.chat_url));
    
    // Platform
    QString platform = QString::fromStdString(cfg.platform);
    int p_idx = m_platform_combo->findData(platform);
    if (p_idx != -1) m_platform_combo->setCurrentIndex(p_idx);
    else m_platform_combo->setCurrentIndex(0); // Default to YouTube

    m_bitrate_spin->setValue(static_cast<int>(cfg.bitrate_kbps));
    m_audio_bitrate_spin->setValue(static_cast<int>(cfg.audio_bitrate_kbps));

    // Find matching resolution preset
    bool found = false;
    for (int i = 0; i < (int)PRESETS.size() - 1; ++i) {
        if (PRESETS[i].second.first  == cfg.width &&
            PRESETS[i].second.second == cfg.height) {
            m_res_combo->setCurrentIndex(i);
            m_custom_w->setValue(cfg.width);
            m_custom_h->setValue(cfg.height);
            m_custom_w->setVisible(false);
            m_custom_h->setVisible(false);
            found = true;
            break;
        }
    }
    if (!found) {
        m_res_combo->setCurrentIndex((int)PRESETS.size() - 1);
        m_custom_w->setValue(cfg.width);
        m_custom_h->setValue(cfg.height);
        m_custom_w->setVisible(true);
        m_custom_h->setVisible(true);
    }

    // Encoder
    for (int i = 0; i < m_encoder_combo->count(); ++i) {
        if (m_encoder_combo->itemData(i).toInt() == (int)cfg.encoder_pref) {
            m_encoder_combo->setCurrentIndex(i);
            break;
        }
    }

    // FPS
    for (int i = 0; i < m_fps_combo->count(); ++i) {
        if (m_fps_combo->itemData(i).toInt() == (int)cfg.fps) {
            m_fps_combo->setCurrentIndex(i);
            break;
        }
    }

    int sharpen_val = (int)(cfg.sharpening * 100.0f);
    m_sharpen_slider->setValue(sharpen_val);
    m_sharpen_label->setText(QString("%1%").arg(sharpen_val));
}

StreamConfig StreamDialog::get_config() const
{
    StreamConfig cfg;
    cfg.label             = m_label_edit->text().toStdString();
    cfg.rtmp_url          = m_url_edit->text().toStdString();
    cfg.stream_key        = m_key_edit->text().toStdString();
    cfg.chat_url          = m_chat_edit->text().toStdString();
    cfg.platform          = m_platform_combo->currentData().toString().toStdString();
    cfg.bitrate_kbps      = static_cast<uint32_t>(m_bitrate_spin->value());
    cfg.audio_bitrate_kbps = static_cast<uint32_t>(m_audio_bitrate_spin->value());

    int res_idx = m_res_combo->currentIndex();
    if (res_idx == (int)PRESETS.size() - 1) {
        cfg.width  = static_cast<uint32_t>(m_custom_w->value());
        cfg.height = static_cast<uint32_t>(m_custom_h->value());
    } else {
        cfg.width  = PRESETS[res_idx].second.first;
        cfg.height = PRESETS[res_idx].second.second;
    }

    cfg.encoder_pref = static_cast<EncoderType>(m_encoder_combo->currentData().toInt());
    cfg.fps          = static_cast<uint32_t>(m_fps_combo->currentData().toInt());
    cfg.scale_mode   = static_cast<ScalingMode>(m_scale_combo->currentData().toInt());
    cfg.sharpening   = (float)m_sharpen_slider->value() / 100.0f;
    return cfg;
}

// ═══════════════════════════════════════════════════════════════════════════════
// MultistreamDock
// ═══════════════════════════════════════════════════════════════════════════════

MultistreamDock::MultistreamDock(QWidget *parent)
    : QWidget(parent), m_mgr(MultistreamOutput::instance())
{
    m_cpu_tracker = nullptr;
    setWindowTitle(obs_module_text("MultiStreamDock.Title"));
    setup_ui();

    // Refresh stats every second
    m_stats_timer = new QTimer(this);
    connect(m_stats_timer, &QTimer::timeout, this, &MultistreamDock::on_stats_tick);
    m_stats_timer->start(1000);

    refresh_table();
}

MultistreamDock::~MultistreamDock()
{
    if (m_cpu_tracker) os_cpu_usage_info_destroy(m_cpu_tracker);
}

void MultistreamDock::setup_ui()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(5, 5, 5, 5);
    root->setSpacing(5);

    // ── Stream List Table ─────────────────────────────────────────────────────
    m_table = new QTableWidget(0, COL_COUNT, this);
    m_table->setHorizontalHeaderLabels({"#", "Platform", "Label", "Res", "Bitrate", "Status"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->hide();
    connect(m_table, &QTableWidget::cellDoubleClicked, this, &MultistreamDock::on_edit_row);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto *toolbar = new QHBoxLayout();
    m_add_btn = new QPushButton("+ Add", this);
    m_remove_btn = new QPushButton("- Remove", this);
    m_start_btn = new QPushButton("Start All", this);
    m_stop_btn = new QPushButton("Stop All", this);
    
    m_add_btn->setMinimumHeight(28);
    m_start_btn->setStyleSheet("background-color: #2c5e2c; font-weight: bold;");
    m_stop_btn->setStyleSheet("background-color: #5e2c2c; font-weight: bold;");

    toolbar->addWidget(m_add_btn);
    toolbar->addWidget(m_remove_btn);
    toolbar->addStretch();
    toolbar->addWidget(m_start_btn);
    toolbar->addWidget(m_stop_btn);

    root->addLayout(toolbar);
    root->addWidget(m_table);

    // ── Unified Telemetry Footer (Single Row) ────────────────────────────────
    auto *footer = new QGroupBox("System Telemetry", this);
    auto *footer_layout = new QHBoxLayout(footer);
    footer_layout->setContentsMargins(10, 5, 10, 5);
    footer_layout->setSpacing(15);
    
    m_total_kbps_lbl    = new QLabel("Bitrate: 0 kbps", this);
    m_total_dropped_lbl = new QLabel("Dropped: 0", this);
    m_total_data_lbl    = new QLabel("Total: 0 MB", this);
    m_obs_cpu_lbl       = new QLabel("CPU: 0.0%", this);
    m_ram_usage_lbl     = new QLabel("RAM: 0 MB", this);
    m_obs_gpu_lbl       = new QLabel("GPU: 0.0ms", this);
    m_render_lag_lbl    = new QLabel("R-Lag: 0 (0.0%)", this);
    m_encoder_lag_lbl   = new QLabel("E-Lag: 0 (0.0%)", this);

    m_total_kbps_lbl->setStyleSheet("font-weight: bold; color: #5cb85c;");
    m_total_dropped_lbl->setStyleSheet("font-weight: bold; color: #dc3232;");
    m_obs_cpu_lbl->setStyleSheet("font-weight: bold; color: #3498db;");
    m_ram_usage_lbl->setStyleSheet("font-weight: bold; color: #9b59b6;");
    m_obs_gpu_lbl->setStyleSheet("font-weight: bold; color: #e67e22;");
    m_render_lag_lbl->setStyleSheet("font-weight: bold; color: #f1c40f;");
    m_encoder_lag_lbl->setStyleSheet("font-weight: bold; color: #e74c3c;");

    footer_layout->addWidget(m_total_kbps_lbl);
    footer_layout->addWidget(m_total_dropped_lbl);
    footer_layout->addWidget(m_total_data_lbl);
    footer_layout->addSpacing(10);
    footer_layout->addWidget(m_obs_cpu_lbl);
    footer_layout->addWidget(m_ram_usage_lbl);
    footer_layout->addWidget(m_obs_gpu_lbl);
    footer_layout->addWidget(m_render_lag_lbl);
    footer_layout->addWidget(m_encoder_lag_lbl);
    footer_layout->addStretch();

    root->addWidget(footer);
    
    connect(m_add_btn, &QPushButton::clicked, this, &MultistreamDock::on_add_clicked);
    connect(m_remove_btn, &QPushButton::clicked, this, &MultistreamDock::on_remove_clicked);
    connect(m_start_btn, &QPushButton::clicked, this, &MultistreamDock::on_start_all_clicked);
    connect(m_stop_btn, &QPushButton::clicked, this, &MultistreamDock::on_stop_all_clicked);
}

void MultistreamDock::refresh_table()
{
    refresh_table_internal();
}

void MultistreamDock::refresh_table_internal()
{
    auto configs = m_mgr->get_configs();
    auto stats   = m_mgr->get_stats();

    m_table->setRowCount(static_cast<int>(configs.size()));

    for (int i = 0; i < (int)configs.size(); ++i) {
        const auto &c = configs[static_cast<size_t>(i)];
        
        auto set_cell = [&](int col, const QString &text, Qt::AlignmentFlag align = Qt::AlignLeft) {
            auto *item = m_table->item(i, col);
            if (!item) {
                item = new QTableWidgetItem(text);
                item->setTextAlignment(align | Qt::AlignVCenter);
                m_table->setItem(i, col, item);
            } else if (item->text() != text) {
                item->setText(text);
            }
        };

        QString lbl = c.label.empty()
            ? QString("Stream %1").arg(i + 1)
            : QString::fromStdString(c.label);

        QString plat = QString::fromStdString(c.platform);
        if (plat == "youtube") plat = "YouTube";
        else if (plat == "twitch") plat = "Twitch";
        else if (plat == "kick") plat = "Kick";
        else if (plat == "facebook") plat = "Facebook";
        else if (plat == "tiktok") plat = "TikTok";
        else if (plat == "instagram") plat = "Instagram";
        else if (plat == "x") plat = "X";
        else if (plat == "trovo") plat = "Trovo";
        else if (plat.isEmpty()) plat = "-";

        set_cell(COL_NUM,      QString::number(i + 1), Qt::AlignHCenter);
        set_cell(COL_PLATFORM, plat, Qt::AlignHCenter);
        set_cell(COL_LABEL,    lbl);
        set_cell(COL_RES,      QString("%1×%2").arg(c.width).arg(c.height));
        set_cell(COL_BITRATE,  QString("%1 kbps").arg(c.bitrate_kbps));

        auto *status_lbl = qobject_cast<QLabel*>(m_table->cellWidget(i, COL_STATUS));
        if (!status_lbl) {
            status_lbl = new QLabel();
            status_lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            status_lbl->setContentsMargins(4, 0, 4, 0);
            m_table->setCellWidget(i, COL_STATUS, status_lbl);
        }
    }
}

void MultistreamDock::update_controls()
{
    m_remove_btn->setEnabled(m_table->currentRow() >= 0);

    auto all_configs = m_mgr->get_configs();
    auto all_stats   = m_mgr->get_stats();

    double total_kbps = 0;
    uint32_t total_dropped = 0;
    uint64_t total_bytes = 0;
    uint32_t total_render_lag = 0;
    uint32_t total_render_frames = 0;
    uint32_t total_encoder_lag = 0;
    uint32_t total_encoder_frames = 0;

    for (int i = 0; i < (int)all_configs.size(); ++i) {
        auto *lbl = qobject_cast<QLabel*>(m_table->cellWidget(i, COL_STATUS));
        if (!lbl) continue;

        const StreamStats &s = (i < (int)all_stats.size()) ? all_stats[i] : StreamStats{};

        if (s.is_active) {
            total_kbps += s.net_bitrate_kbps;
            total_dropped += s.dropped_frames;
            total_bytes += s.bytes_sent;
            
            total_render_lag += s.render_lagged_frames;
            total_render_frames = s.render_total_frames; // Use latest global count
            
            total_encoder_lag += s.skipped_frames;
            total_encoder_frames += s.total_encoded_frames;
        }

        if (s.has_error) {
            lbl->setText(QString("<span style='color:#dc3232;'>✕ Error: %1</span>").arg(QString::fromStdString(s.error_message)));
        } else if (s.is_active) {
            double mb = static_cast<double>(s.bytes_sent) / (1024.0 * 1024.0);
            QString sent_str = (mb > 1024.0) 
                ? QString("%1 GB").arg(mb / 1024.0, 0, 'f', 2)
                : QString("%1 MB").arg(mb, 0, 'f', 1);

            lbl->setText(QString("<span style='color:#5cb85c;'>● %1 kbps | %2</span><br/><span style='font-size:9px; color:#aaa;'>%3 | %4</span>")
                .arg(static_cast<int>(s.net_bitrate_kbps))
                .arg(sent_str)
                .arg(QString::fromStdString(s.encoder_name))
                .arg(QString::fromStdString(s.scale_filter)));
        } else {
            lbl->setText("<span style='color:#999;'>○ Idle</span>");
        }
    }

    if (!m_total_kbps_lbl) return;

    m_total_kbps_lbl->setText(QString("Bitrate: %1 kbps").arg(static_cast<int>(total_kbps)));
    m_total_dropped_lbl->setText(QString("Dropped: %1").arg(total_dropped));
    
    double total_mb = static_cast<double>(total_bytes) / (1024.0 * 1024.0);
    if (total_mb > 1024.0) {
        m_total_data_lbl->setText(QString("Total: %1 GB").arg(total_mb / 1024.0, 0, 'f', 2));
    } else {
        m_total_data_lbl->setText(QString("Total: %1 MB").arg(total_mb, 0, 'f', 1));
    }

    // Global OBS Resource Usage
    double cpu_usage = 0.0;
    if (obs_get_active_fps() > 0) {
        if (!m_cpu_tracker) {
            m_cpu_tracker = os_cpu_usage_info_start();
        }
        if (m_cpu_tracker) {
            cpu_usage = os_cpu_usage_info_query(m_cpu_tracker);
        }
    }

    double gpu_time  = obs_get_average_frame_time_ns() / 1000000.0;

    m_obs_cpu_lbl->setText(QString("CPU: %1%").arg(cpu_usage, 0, 'f', 1));
    
    // RAM usage (Resident Set Size)
    uint64_t rss = os_get_proc_resident_size();
    double rss_mb = static_cast<double>(rss) / (1024.0 * 1024.0);
    if (rss_mb > 1024.0) {
        m_ram_usage_lbl->setText(QString("RAM: %1 GB").arg(rss_mb / 1024.0, 0, 'f', 2));
    } else {
        m_ram_usage_lbl->setText(QString("RAM: %1 MB").arg(rss_mb, 0, 'f', 0));
    }
    
    uint64_t interval = obs_get_frame_interval_ns();
    if (interval > 0) {
        m_obs_gpu_lbl->setText(QString("GPU: %1ms").arg(gpu_time, 0, 'f', 2));
    } else {
        m_obs_gpu_lbl->setText("GPU: 0.0ms");
    }

    double render_lag_pct = (total_render_frames > 0) ? (static_cast<double>(total_render_lag) * 100.0 / total_render_frames) : 0.0;
    double encoder_lag_pct = (total_encoder_frames > 0) ? (static_cast<double>(total_encoder_lag) * 100.0 / total_encoder_frames) : 0.0;

    m_render_lag_lbl->setText(QString("Render Lag: %1 (%2%)").arg(total_render_lag).arg(render_lag_pct, 0, 'f', 1));
    m_encoder_lag_lbl->setText(QString("Encoder Lag: %1 (%2%)").arg(total_encoder_lag).arg(encoder_lag_pct, 0, 'f', 1));
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MultistreamDock::on_add_clicked()
{
    StreamConfig def;
    def.label  = "Stream " + std::to_string(m_mgr->target_count() + 1);

    StreamDialog dlg(this, def);
    if (dlg.exec() == QDialog::Accepted) {
        StreamConfig cfg = dlg.get_config();
        m_mgr->add_target(cfg);
        refresh_table();
        update_controls();
    }
}

void MultistreamDock::on_remove_clicked()
{
    int row = m_table->currentRow();
    if (row < 0) return;

    auto stats = m_mgr->get_stats();
    if (row < (int)stats.size() && stats[static_cast<size_t>(row)].is_active) {
        auto ans = QMessageBox::question(this, "Remove Stream",
            "This stream is live. Stop and remove it?",
            QMessageBox::Yes | QMessageBox::No);
        if (ans != QMessageBox::Yes) return;
    }

    m_mgr->remove_target(row);
    refresh_table();
    update_controls();
}

void MultistreamDock::on_edit_row(int row, int)
{
    auto configs = m_mgr->get_configs();
    if (row < 0 || row >= (int)configs.size()) return;

    StreamDialog dlg(this, configs[static_cast<size_t>(row)]);
    if (dlg.exec() == QDialog::Accepted) {
        m_mgr->update_target(row, dlg.get_config());
        refresh_table();
    }
}

void MultistreamDock::on_open_chats_clicked()
{
    auto configs = m_mgr->get_configs();
    int opened = 0;
    for (const auto &cfg : configs) {
        if (!cfg.chat_url.empty()) {
            QDesktopServices::openUrl(QUrl(QString::fromStdString(cfg.chat_url)));
            opened++;
        }
    }
    
    if (opened == 0) {
        QMessageBox::information(this, "No Chat URLs",
            "None of your configured streams have a Chat URL set.");
    }
}

void MultistreamDock::on_start_all_clicked()
{
    if (m_mgr->target_count() == 0) {
        QMessageBox::information(this, "No Streams",
            "Add at least one stream target first.");
        return;
    }
    m_mgr->start_all();
    refresh_table();
}

void MultistreamDock::on_stop_all_clicked()
{
    m_mgr->stop_all();
    refresh_table();
}

void MultistreamDock::on_stats_tick()
{
    refresh_table_internal();
    update_controls();
}
