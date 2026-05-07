#pragma once

#include "multistream-output.h"
#include "stream-config.h"
#include <QWidget>
#include <QDialog>
#include <QTableWidget>
#include <QPushButton>
#include <QLabel>
#include <QTimer>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QTabWidget>
#include <QListWidget>
#include <QSplitter>

// ── Per-stream settings dialog ────────────────────────────────────────────────

class StreamDialog : public QDialog {
    Q_OBJECT
public:
    explicit StreamDialog(QWidget *parent = nullptr,
                          const StreamConfig &cfg = StreamConfig{});

    StreamConfig get_config() const;

private:
    void setup_ui();
    void populate(const StreamConfig &cfg);

    QLineEdit   *m_url_edit;
    QLineEdit   *m_key_edit;
    QLineEdit   *m_chat_edit;
    QLineEdit   *m_label_edit;
    QComboBox   *m_platform_combo;
    QComboBox   *m_res_combo;
    QSpinBox    *m_custom_w;
    QSpinBox    *m_custom_h;
    QSpinBox    *m_bitrate_spin;
    QComboBox   *m_encoder_combo;
    QComboBox   *m_fps_combo;
    QComboBox   *m_scale_combo;
    QSlider     *m_sharpen_slider;
    QLabel      *m_sharpen_label;
    QSpinBox    *m_audio_bitrate_spin;

    static const std::vector<std::pair<QString, std::pair<uint32_t,uint32_t>>> PRESETS;
};

// ── Main dock widget ──────────────────────────────────────────────────────────

class MultistreamDock : public QWidget {
    Q_OBJECT
public:
    explicit MultistreamDock(QWidget *parent = nullptr);
    ~MultistreamDock() override = default;

private slots:
    void on_add_clicked();
    void on_remove_clicked();
    void on_edit_row(int row, int col);
    void on_start_all_clicked();
    void on_stop_all_clicked();
    void on_open_chats_clicked();
    void on_stats_tick();

private:
    void setup_ui();
    void refresh_table();
    void refresh_table_internal();
    void update_controls();

    // Table columns
    enum Col {
        COL_NUM      = 0,
        COL_PLATFORM = 1,
        COL_LABEL    = 2,
        COL_RES      = 3,
        COL_BITRATE  = 4,
        COL_STATUS   = 5,
        COL_COUNT    = 6
    };

    QTableWidget   *m_table;
    QPushButton    *m_add_btn;
    QPushButton    *m_remove_btn;
    QPushButton    *m_start_btn;
    QPushButton    *m_stop_btn;
    QTimer         *m_stats_timer;

    MultistreamOutput *m_mgr;
};
