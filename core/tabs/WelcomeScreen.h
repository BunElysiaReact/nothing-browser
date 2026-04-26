#pragma once
#include <QWidget>

class WelcomeScreen : public QWidget {
    Q_OBJECT

public:
    explicit WelcomeScreen(QWidget *parent = nullptr);

signals:
    void accepted();
};