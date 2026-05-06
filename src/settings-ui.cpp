#include "settings-ui.h"
#include "util/hw-capability.h"
#include "util/logger.h"
#include <obs-module.h>
#include <QHeaderView>
#include <QMessageBox>
#include <QSizePolicy>
#include <QGridLayout>
#include <QScrollArea>
#include <QString>
#include <sstream>
#include <QDesktopServices>
#include <QUrl>

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
    m_fps_combo->addItem(QString(obs_module_text("StreamDialog.FPS.Auto")) + " (Match OBS)", 0);
    m_fps_combo->addItem("60", 60);
    m_fps_combo->addItem("59.94", 60); // We map 59.94 to 60 for simplicity, or just store exact if needed. Actually let's just use int 60, 30.
    m_fps_combo->addItem("50", 50);
    m_fps_combo->addItem("30", 30);
    m_fps_combo->addItem("29.97", 30);
    m_fps_combo->addItem("25", 25);
    m_fps_combo->addItem("24", 24);
    vid_form->addRow(obs_module_text("StreamDialog.Label.FPS"), m_fps_combo);


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
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Table (config columns only) ──────────────────────────────────────────────
    m_table = new QTableWidget(0, COL_COUNT, this);
    m_table->setHorizontalHeaderLabels({
        "#", "Label", "Resolution", "Target Bitrate", "Status"
    });
    m_table->horizontalHeader()->setSectionResizeMode(COL_NUM,     QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(COL_LABEL,   QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(COL_RES,     QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(COL_BITRATE, QHeaderView::Fixed);
    m_table->horizontalHeader()->setSectionResizeMode(COL_STATUS,  QHeaderView::Stretch);
    m_table->setColumnWidth(COL_NUM,      28);
    m_table->setColumnWidth(COL_RES,     110);
    m_table->setColumnWidth(COL_BITRATE,  95);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setAlternatingRowColors(true);
    m_table->verticalHeader()->hide();
    // Let the table expand to take available vertical space, pushing the stats panel to the bottom
    m_table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_table, &QTableWidget::cellDoubleClicked,
            this, &MultistreamDock::on_edit_row);

    // ── Toolbar ───────────────────────────────────────────────────────────────
    auto *toolbar = new QHBoxLayout;

    m_add_btn = new QPushButton(obs_module_text("MultiStreamDock.AddStream"), this);
    m_remove_btn = new QPushButton(obs_module_text("MultiStreamDock.RemoveStream"), this);

    m_start_btn = new QPushButton(obs_module_text("MultiStreamDock.StartAll"), this);
    m_start_btn->setStyleSheet("QPushButton { background-color: #2d7a2d; color: white; "
                               "border-radius: 4px; padding: 4px 10px; font-weight: bold; }");

    m_stop_btn = new QPushButton(obs_module_text("MultiStreamDock.StopAll"), this);
    m_stop_btn->setStyleSheet("QPushButton { background-color: #7a2d2d; color: white; "
                              "border-radius: 4px; padding: 4px 10px; font-weight: bold; }");

    m_open_chats_btn = new QPushButton(obs_module_text("MultiStreamDock.OpenChats"), this);
    m_open_chats_btn->setStyleSheet("QPushButton { background-color: #4a4a8a; color: white; "
                                    "border-radius: 4px; padding: 4px 10px; font-weight: bold; }");

    toolbar->addWidget(m_add_btn);
    toolbar->addWidget(m_remove_btn);
    toolbar->addStretch();
    toolbar->addWidget(m_open_chats_btn);
    toolbar->addWidget(m_start_btn);
    toolbar->addWidget(m_stop_btn);

    connect(m_add_btn,    &QPushButton::clicked, this, &MultistreamDock::on_add_clicked);
    connect(m_remove_btn, &QPushButton::clicked, this, &MultistreamDock::on_remove_clicked);
    connect(m_open_chats_btn, &QPushButton::clicked, this, &MultistreamDock::on_open_chats_clicked);
    connect(m_start_btn,  &QPushButton::clicked, this, &MultistreamDock::on_start_all_clicked);
    connect(m_stop_btn,   &QPushButton::clicked, this, &MultistreamDock::on_stop_all_clicked);

    root->addLayout(toolbar);
    root->addWidget(m_table);

    // ── Live stats panel (always visible below the table) ────────────────────
    m_stats_panel = new QFrame(this);
    m_stats_panel->setObjectName("statsPanel");
    m_stats_panel->setAttribute(Qt::WA_StyledBackground, true);
    m_stats_panel->setStyleSheet(
        "#statsPanel { background: #1c1f26; border-top: 1px solid #333; }");
    m_stats_panel->setMinimumHeight(28);
    m_stats_layout = new QVBoxLayout(m_stats_panel);
    m_stats_layout->setContentsMargins(8, 4, 8, 4);
    m_stats_layout->setSpacing(2);
    // Always visible — shows "Idle" when no streams are live
    m_stats_panel->setVisible(true);

    // Placeholder label shown when nothing is active
    auto *placeholder = new QLabel(
        "<span style='color:#555; font-size:11px;'>No active streams — press ▶ Start All to begin.</span>",
        m_stats_panel);
    placeholder->setTextFormat(Qt::RichText);
    placeholder->setObjectName("stats_placeholder");
    m_stats_layout->addWidget(placeholder);

    // Overall stats label (bottom)
    m_overall_stats_label = new QLabel(m_stats_panel);
    m_overall_stats_label->setTextFormat(Qt::RichText);
    m_overall_stats_label->setWordWrap(false);
    m_overall_stats_label->setStyleSheet("padding: 2px 0; font-size: 11px; border-top: 1px solid #444; margin-top: 2px;");
    m_overall_stats_label->setVisible(false);
    m_stats_layout->addWidget(m_overall_stats_label);

    root->addWidget(m_stats_panel);

    refresh_table();
    update_controls();
}

void MultistreamDock::refresh_table()
{
    auto configs = m_mgr->get_configs();
    auto stats   = m_mgr->get_stats();

    m_table->setRowCount(static_cast<int>(configs.size()));

    for (int i = 0; i < (int)configs.size(); ++i) {
        const auto &c = configs[static_cast<size_t>(i)];
        const auto &s = i < (int)stats.size() ? stats[static_cast<size_t>(i)] : StreamStats{};

        auto set_cell = [&](int col, const QString &text, Qt::AlignmentFlag align = Qt::AlignLeft) {
            auto *item = m_table->item(i, col);
            if (!item) {
                item = new QTableWidgetItem(text);
                item->setTextAlignment(align | Qt::AlignVCenter);
                m_table->setItem(i, col, item);
            } else {
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


        // Status cell widget for rich text stats
        auto *status_widget = m_table->cellWidget(i, COL_STATUS);
        if (!status_widget) {
            auto *lbl = new QLabel();
            lbl->setTextFormat(Qt::RichText);
            lbl->setWordWrap(false);
            lbl->setStyleSheet("padding: 2px 4px; font-size: 11px;");
            lbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_table->setCellWidget(i, COL_STATUS, lbl);
        }
    }
}

void MultistreamDock::update_controls()
{
    m_remove_btn->setEnabled(m_table->currentRow() >= 0);

    // ── Live stats panel ──────────────────────────────────────────────────
    auto all_configs = m_mgr->get_configs();
    auto all_stats   = m_mgr->get_stats();

    int active_count = 0;
    double total_bitrate_kbps = 0.0;
    uint64_t total_bytes_sent = 0;
    uint32_t total_dropped_frames = 0;
    uint32_t global_render_total = 0;
    uint32_t global_render_lagged = 0;

    for (int i = 0; i < (int)all_configs.size(); ++i) {
        auto *lbl = qobject_cast<QLabel*>(m_table->cellWidget(i, COL_STATUS));
        if (!lbl) continue;

        const auto &c     = all_configs[i];
        const StreamStats &s = (i < (int)all_stats.size())
            ? all_stats[i] : StreamStats{};

        if (s.has_error) {
            lbl->setText(
                QString("<span style='color:#dc3232;font-weight:bold;'>✕ Error</span>"
                        "&nbsp;&nbsp;<span style='color:#aaa;'>%1</span>")
                .arg(QString::fromStdString(s.error_message)));
            continue;
        }

        if (!s.is_active) {
            lbl->setText("<span style='color:#999;'>○ Idle</span>");
            continue;
        }

        ++active_count;
        total_bitrate_kbps += s.net_bitrate_kbps;
        total_bytes_sent += s.bytes_sent;
        total_dropped_frames += s.dropped_frames;
        global_render_total = s.render_total_frames;
        global_render_lagged = s.render_lagged_frames;

        QString drop_color = (s.dropped_frames      > 0) ? "#e8a020" : "#5cb85c";
        double mb = static_cast<double>(s.bytes_sent) / (1024.0 * 1024.0);
        double mbps = s.net_bitrate_kbps / 1000.0;
        QString name = QString::fromStdString(
            c.label.empty() ? "Stream " + std::to_string(i+1) : c.label);

        // Smart units for total sent: show GB if >= 1000 MB
        QString sent_str;
        if (mb >= 1024.0)
            sent_str = QString("%1 GB").arg(mb / 1024.0, 0, 'f', 2);
        else
            sent_str = QString("%1 MB").arg(mb, 0, 'f', 1);

        lbl->setText(QString(
            "<span style='color:#5cb85c;font-weight:bold;'>● Live</span>"
            "&nbsp;&nbsp;"
            "<b style='color:#fff;'>%1 kbps</b>"
            "<span style='color:#aaa;font-size:10px;'>&nbsp;(%2 Mbps)</span>"
            "&nbsp;&nbsp;<span style='color:#666;'>|</span>&nbsp;&nbsp;"
            "<span style='color:#8ab4f8;'>%3 fps</span>"
            "&nbsp;&nbsp;<span style='color:#666;'>|</span>&nbsp;&nbsp;"
            "<span style='color:#aaa;'>%4 sent</span>"
            "&nbsp;&nbsp;<span style='color:#666;'>|</span>&nbsp;&nbsp;"
            "<span style='color:%5;'>enc-drop: %6</span>")
            .arg(static_cast<int>(s.net_bitrate_kbps))
            .arg(mbps, 0, 'f', 2)
            .arg(s.output_fps, 0, 'f', 2)
            .arg(sent_str)
            .arg(drop_color).arg(s.dropped_frames));
    }

    // Show placeholder only when no streams configured
    auto *placeholder = m_stats_panel->findChild<QLabel*>("stats_placeholder");
    if (placeholder)
        placeholder->setVisible(all_configs.empty());

    // Update overall stats label
    if (active_count > 0) {
        double total_mbps = total_bitrate_kbps / 1000.0;
        double total_mb = static_cast<double>(total_bytes_sent) / (1024.0 * 1024.0);
        QString total_sent_str;
        if (total_mb >= 1024.0)
            total_sent_str = QString("%1 GB").arg(total_mb / 1024.0, 0, 'f', 2);
        else
            total_sent_str = QString("%1 MB").arg(total_mb, 0, 'f', 1);

        double lag_pct    = (global_render_total > 0)
            ? (100.0 * global_render_lagged / global_render_total) : 0.0;
        QString lag_color  = (global_render_lagged > 0) ? "#e8a020" : "#5cb85c";
        QString drop_color = (total_dropped_frames > 0) ? "#e8a020" : "#5cb85c";

        m_overall_stats_label->setText(QString(
            "<span style='color:#fff;font-weight:bold;'>Total</span>"
            "&nbsp;&nbsp;"
            "<b style='color:#fff;'>%1 kbps</b>"
            "<span style='color:#aaa;font-size:10px;'>&nbsp;(%2 Mbps)</span>"
            "&nbsp;&nbsp;<span style='color:#666;'>|</span>&nbsp;&nbsp;"
            "<span style='color:#aaa;'>%3 sent</span>"
            "&nbsp;&nbsp;<span style='color:#666;'>|</span>&nbsp;&nbsp;"
            "<span style='color:%4;'>enc-drop: %5</span>"
            "&nbsp;&nbsp;<span style='color:#666;'>|</span>&nbsp;&nbsp;"
            "<span style='color:%6;'>render-lag: %7 (%8%)</span>")
            .arg(static_cast<int>(total_bitrate_kbps))
            .arg(total_mbps, 0, 'f', 2)
            .arg(total_sent_str)
            .arg(drop_color).arg(total_dropped_frames)
            .arg(lag_color).arg(global_render_lagged)
            .arg(lag_pct, 0, 'f', 1));
        
        m_overall_stats_label->setVisible(true);
    } else {
        m_overall_stats_label->setVisible(false);
    }

    m_stats_panel->setVisible(true);  // always visible
    m_stats_panel->update();
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
    refresh_table();
    update_controls();
}
