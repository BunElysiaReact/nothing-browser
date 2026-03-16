#pragma once
#include <QWidget>
#include <QWebEngineView>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QLabel>
#include "../engine/NetworkCapture.h"

class Interceptor;

class BrowserTab : public QWidget {
    Q_OBJECT
public:
    explicit BrowserTab(QWidget *parent = nullptr);

    NetworkCapture *networkCapture() const { return m_capture; }

    // Run arbitrary JS in the current page
    void runJS(const QString &script);

signals:
    void requestCaptured(const QString &method,
                         const QString &url,
                         const QString &headers);

private slots:
    void navigate();
    void onUrlChanged(const QUrl &url);
    void toggleJS(bool enabled);
    void toggleCSS(bool enabled);
    void toggleImages(bool enabled);

private:
    QWebEngineView *m_view;
    QLineEdit      *m_urlBar;
    QPushButton    *m_goBtn;
    QCheckBox      *m_jsToggle;
    QCheckBox      *m_cssToggle;
    QCheckBox      *m_imgToggle;
    QLabel         *m_statusLabel;
    Interceptor    *m_interceptor;
    NetworkCapture *m_capture;
    void setupUI();
    void setupWebEngine();
    void applySettings();
};