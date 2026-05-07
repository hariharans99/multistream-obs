#include "settings-ui.h"
#include "util/hw-capability.h"
#include "util/logger.h"
#include <obs-module.h>
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

    // High-Fidelity Presets (60fps Optimized)
    connect(ultra_btn, &QPushButton::clicked, [=]() { apply_quality(0, 40000, 192, 0.50f); }); // 4K (40 Mbps)
    connect(high_btn,  &QPushButton::clicked, [=]() { apply_quality(2, 10000, 160, 0.40f); }); // 1080p (10 Mbps)
    connect(med_btn,   &QPushButton::clicked, [=]() { apply_quality(3, 6000, 128, 0.25f); });  // 720p (6 Mbps)
    connect(norm_btn,  &QPushButton::clicked, [=]() { apply_quality(4, 2500, 128, 0.10f); });  // 480p (2.5 Mbps)

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
    setWindowTitle(obs_module_text("MultiStreamDock.Title"));
    setup_ui();

    // Refresh stats every second
    m_stats_timer = new QTimer(this);
    connect(m_stats_timer, &QTimer::timeout, this, &MultistreamDock::on_stats_tick);
    m_stats_timer->start(1000);
}

void MultistreamDock::setup_ui()
{
    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(5, 5, 5, 5);
    root->setSpacing(5);

    // ── Stream List Table ─────────────────────────────────────────────────────
    m_table = new QTableWidget(0, COL_COUNT, this);
    m_table->setHorizontalHeaderLabels({"#", "Label", "Res", "Bitrate", "Status"});
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
    
    connect(m_add_btn, &QPushButton::clicked, this, &MultistreamDock::on_add_clicked);
    connect(m_remove_btn, &QPushButton::clicked, this, &MultistreamDock::on_remove_clicked);
    connect(m_start_btn, &QPushButton::clicked, this, &MultistreamDock::on_start_all_clicked);
    connect(m_stop_btn, &QPushButton::clicked, this, &MultistreamDock::on_stop_all_clicked);

    refresh_table();
    update_controls();
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

        set_cell(COL_NUM,     QString::number(i + 1), Qt::AlignHCenter);
        set_cell(COL_LABEL,   lbl);
        set_cell(COL_RES,     QString("%1×%2").arg(c.width).arg(c.height));
        set_cell(COL_BITRATE, QString("%1 kbps").arg(c.bitrate_kbps));

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

    for (int i = 0; i < (int)all_configs.size(); ++i) {
        auto *lbl = qobject_cast<QLabel*>(m_table->cellWidget(i, COL_STATUS));
        if (!lbl) continue;

        const StreamStats &s = (i < (int)all_stats.size()) ? all_stats[i] : StreamStats{};

        if (s.has_error) {
            lbl->setText(QString("<span style='color:#dc3232;'>✕ Error: %1</span>").arg(QString::fromStdString(s.error_message)));
        } else if (s.is_active) {
            lbl->setText(QString("<span style='color:#5cb85c;'>● Live (%1 kbps)</span>").arg(static_cast<int>(s.net_bitrate_kbps)));
        } else {
            lbl->setText("<span style='color:#999;'>○ Idle</span>");
        }
    }
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
