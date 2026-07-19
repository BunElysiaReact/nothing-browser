#pragma once
#include <QWidget>
#include <QWebEngineView>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include <QJsonArray>
#include "../engine/NetworkCapture.h"
#include "../engine/PluginManager.h"

class Interceptor;

class BrowserTab : public QWidget {
    Q_OBJECT
public:
    explicit BrowserTab(QWidget *parent = nullptr);

    NetworkCapture *networkCapture() const { return m_capture; }
    void runJS(const QString &script);

signals:
    void requestCaptured(const QString &method,
                         const QString &url,
                         const QString &headers);

private slots:
    void navigate();
    void navigateToUrl(const QUrl &url);
    void onUrlChanged(const QUrl &url);
    void toggleJS(bool enabled);
    void toggleCSS(bool enabled);
    void toggleImages(bool enabled);
    void goHome();
    void goBack();
    void refreshPage();

private:
    QWebEngineView *m_view;
    QLineEdit      *m_urlBar;
    QPushButton    *m_goBtn;
    QPushButton    *m_homeBtn;
    QPushButton    *m_backBtn;
    QPushButton    *m_refreshBtn;
    QCheckBox      *m_jsToggle;
    QCheckBox      *m_cssToggle;
    QCheckBox      *m_imgToggle;
    QLabel         *m_statusLabel;
    Interceptor    *m_interceptor;
    NetworkCapture *m_capture;
    bool            m_onHomePage = true;

    // Shortcuts storage
    QJsonArray      m_shortcuts;
    QString         m_shortcutsPath;
    QString         m_bgImagePath;

    void setupUI();
    void setupWebEngine();
    void applySettings();
    void showHomePage();
    void loadShortcuts();
    void saveShortcuts();
    QString buildHomeHTML();
    QString shortcutsJson();
    QString toolbarButtonStyle(const QString &extra = "");
};