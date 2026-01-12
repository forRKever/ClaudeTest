#ifndef WATERMARKDIALOG_H
#define WATERMARKDIALOG_H

#include <QDialog>

#include <QLabel>
#include <QVBoxLayout>
#include <QTimer>
#include "MediaPlayerWrapper.h"

#define GET_FILE_YEAR(time)		((time>>26)+2000)
#define GET_FILE_MONTH(time)	((time>>22)&15)
#define GET_FILE_DAY(time)		((time>>17)&31)
#define GET_FILE_HOUR(time)		((time>>12)&31)
#define GET_FILE_MINUTE(time)	((time>>6)&63)
#define GET_FILE_SECOND(time)	((time>>0)&63)

class WatermarkDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WatermarkDialog(MediaPlayerWrapper* player, QWidget* parent = nullptr);
    void m_setTimer(bool IsStart);

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void updateWatermarkInfo();

private:
    QLabel* m_label;
    QTimer* m_timer;
    MediaPlayerWrapper* m_player;

    void m_clearInfo();
};

#endif // WATERMARKDIALOG_H
