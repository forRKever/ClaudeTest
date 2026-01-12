#include "watermarkdialog.h"
#include <QDateTime>

WatermarkDialog::WatermarkDialog(MediaPlayerWrapper* player, QWidget* parent)
    : QDialog(parent), m_player(player)
{
    setWindowTitle("WaterMark");
    resize(280, 180);
    setStyleSheet("background-color: black; color: white;");
    setWindowFlags(windowFlags() | Qt::WindowStaysOnTopHint);

    m_label = new QLabel(this);
    m_label->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    QFont font;
    font.setPointSize(16);      // 字體大小
    font.setBold(true);         // 加粗
    m_label->setFont(font);

    QVBoxLayout* layout = new QVBoxLayout(this);
    layout->addWidget(m_label);
    setLayout(layout);

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &WatermarkDialog::updateWatermarkInfo);
//    m_timer->start(1000);
}

void WatermarkDialog::updateWatermarkInfo()
{
    QString info;
    auto data = m_player->getWatermarkData();
    if (data) {

        info = QString("Mac: %1\nDeviceSn: %2\nChan: %3\nGTime: %4\nDeviceInfo: %5\nDeviceType: %6")
            .arg(data->mac)
            .arg(data->deviceSN)
            .arg(data->channelNum)
            .arg(QString("%1-%2-%3 %4:%5:%6").arg(GET_FILE_YEAR(data->globalTime))
                                             .arg(GET_FILE_MONTH(data->globalTime))
                                             .arg(GET_FILE_DAY(data->globalTime))
                                             .arg(GET_FILE_HOUR(data->globalTime))
                                             .arg(GET_FILE_MINUTE(data->globalTime))
                                             .arg(GET_FILE_SECOND(data->globalTime)))
            .arg(data->deviceInfo)
            .arg(data->deviceType);
    } else {
        info = "Mac\nDeviceSn\nChan\nGTime\nDeviceInfo\nDeviceType";
    }

    m_label->setText(info);
}

void WatermarkDialog::m_setTimer(bool IsStart)
{
    if(IsStart)
        m_timer->start(500);
    else
    {
        m_clearInfo();
    }
}

void WatermarkDialog::m_clearInfo()
{
    m_timer->stop();
    QString info = "Mac\nDeviceSn\nChan\nGTime\nDeviceInfo\nDeviceType";
    m_label->setText(info);
}

void WatermarkDialog::closeEvent(QCloseEvent* event)
{
    m_clearInfo();
    QDialog::closeEvent(event);
}

