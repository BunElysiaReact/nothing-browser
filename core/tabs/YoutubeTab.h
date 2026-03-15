#pragma once
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>

class YoutubeTab : public QWidget {
    Q_OBJECT
public:
    explicit YoutubeTab(QWidget *parent = nullptr) : QWidget(parent) {
        auto *l = new QLabel("YouTube via NewPipe — Week 6", this);
        l->setStyleSheet("color:#555; font-family:monospace; font-size:13px;");
        l->setAlignment(Qt::AlignCenter);
        auto *layout = new QVBoxLayout(this);
        layout->addWidget(l);
    }
};