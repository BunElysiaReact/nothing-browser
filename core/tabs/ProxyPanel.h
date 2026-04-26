#pragma once
#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QComboBox>
#include <QSpinBox>
#include <QGroupBox>
#include <QTableWidget>
#include <QCheckBox>
#include <QProgressBar>
#include "ProxyManager.h"

class ProxyPanel : public QWidget {
    Q_OBJECT
public:
    explicit ProxyPanel(QWidget *parent = nullptr);

private slots:
    void onLoadTxt();
    void onFetchUrl();
    void onLoadOvpn();
    void onNext();
    void onDisable();
    void onRotationChanged(int idx);
    void onProxyChanged(const ProxyEntry &e);
    void onListLoaded(int count);
    void onFetchFailed(const QString &err);
    void onOvpnLoaded(const QString &remote, int port);

    // health checker slots
    void onCheckStarted(int total);
    void onCheckProgress(int index, ProxyEntry::Health result, int latencyMs);
    void onCheckFinished(int alive, int dead);
    void onCheckAll();
    void onStopCheck();
    void onTableRowClicked(int row, int col);

private:
    void buildUI();
    void refreshProxyTable();
    void refreshStatus();

    // ── Status ────────────────────────────────────────────────────────────────
    QLabel        *m_statusDot;
    QLabel        *m_statusText;
    QLabel        *m_countLabel;
    QLabel        *m_currentLabel;

    // ── Source ────────────────────────────────────────────────────────────────
    QPushButton   *m_loadTxtBtn;
    QPushButton   *m_loadOvpnBtn;
    QLineEdit     *m_fetchUrlEdit;
    QPushButton   *m_fetchBtn;
    QLabel        *m_ovpnLabel;

    // ── Health checker ────────────────────────────────────────────────────────
    QPushButton   *m_checkAllBtn;
    QPushButton   *m_stopCheckBtn;
    QCheckBox     *m_autoCheckBox;
    QCheckBox     *m_skipDeadBox;
    QProgressBar  *m_checkProgress;
    QLabel        *m_checkStats;    // "alive: 12 | dead: 88 | pending: 400"

    // ── Proxy list table ──────────────────────────────────────────────────────
    QTableWidget  *m_proxyTable;    // HOST | PORT | TYPE | STATUS | LATENCY

    // ── Rotation ─────────────────────────────────────────────────────────────
    QComboBox     *m_rotationCombo;
    QSpinBox      *m_intervalSpin;

    // ── Control ──────────────────────────────────────────────────────────────
    QPushButton   *m_nextBtn;
    QPushButton   *m_disableBtn;
    QPushButton   *m_enableBtn;

    int m_checkTotal = 0;
    int m_checkDone  = 0;
};