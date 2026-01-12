#ifndef PLAYERDIALOG_H
#define PLAYERDIALOG_H

#include <QMainWindow>
#include <QDebug>
#include <QTimer>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QMenuBar>
#include <QStatusBar>
#include <QAction>
#include <QApplication>
#include <QGuiApplication>
#include <QScreen>
#include <QStyleOptionSlider>
#include <QLabel>
#include <QVector>
#include <QDateTime>
#include <QInputDialog>
#include <QProcess>
#include "watermarkdialog.h"

class RtspStreamThread;

QT_BEGIN_NAMESPACE
namespace Ui { class PlayerDialog; }
QT_END_NAMESPACE

QT_BEGIN_NAMESPACE
class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QSlider;
class QWidget;
QT_END_NAMESPACE

// Timer constants (from original code)
const int PLAY_TIMER = 1;

class PlayerDialog : public QMainWindow
{
    Q_OBJECT

public:
    explicit PlayerDialog(QWidget *parent = nullptr);
    ~PlayerDialog();

protected:
    // Qt event handlers (override from QWidget)
    void mousePressEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    void onPlayClicked();
    void onPauseClicked();
    void onStopClicked();
    
    // Timer slots
    void onTimerUpdate();
    void updatePosition();
    
    // Slider slots
    void onSliderPressed();
    void onSliderReleased();
    void onSliderValueChanged(int value);
    
    // Volume slider slots
    void onVolumeChanged(int value);
    void onVolumeBtnClicked();
    
    // Menu slots
    void onActionOpen();
    void onActionOpenURL();
    void onActionExit();
    void onActionSetPath();
    void onAcionAbout();
    void onAcionWatermark();
    
    // Toolbar slots
    void onActionPrev();
    void onActionPlayPause();
    void onActionStop();
    void onActionNext();
    void onActionSnap();
    void onActionSlower();
    void onActionFaster();
    
    // Status updates
    void onStatusChanged(const QString &status);

    // Update UI based on playback state
    void updatePlayPauseAction();

private:
    Ui::PlayerDialog *ui;
    
    // Setup functions
    void setupUI();
    void setupTimers();
    void setupMenus();
    
    // Wrapper functions matching original names
    void OnTimer(int nIDEvent);
    void OnLButtonDown(unsigned int nFlags, QPoint point);
    void OnLButtonUp(unsigned int nFlags, QPoint point);
    void OnLButtonDblClk(unsigned int nFlags, QPoint point);
    void OnKeyDown(unsigned int nChar, unsigned int nRepCnt, unsigned int nFlags);
    void OnSize(unsigned int nType, int cx, int cy);
    void OnMove(int x, int y);
    void OnDropFiles(const QStringList &files);
    void OnClose();
    void OnDestroy();
    
    // Helper functions from original
    void DrawStatus();
    void updateButtonStates();
    void adjustWindowSize();
    int calculateSliderValueFromPosition(const QPoint &pos);
    QString formatTime(qint64 seconds);
    QString formatSpeedText(float speed);
    void updateVolumeButtonIcon();

    // UI elements
    QWidget *m_centralWidget;
    QVBoxLayout *m_mainLayout;
    QHBoxLayout *m_buttonLayout;
    QPushButton *m_playButton;
    QPushButton *m_pauseButton;
    QPushButton *m_stopButton;
    QPushButton *m_fastButton;
    QPushButton *m_slowButton;
    QPushButton *m_preButton;
    QPushButton *m_nextButton;
    QPushButton *m_snapButton;
    QPushButton *m_voiceButton;
    QSlider *m_seekSlider;
    QSlider *m_volSlider;
    QWidget *m_videoDisplayWidget;
    QWidget *m_controlPanelWidget;
    QMenuBar *m_menuBar;
    QStatusBar *m_statusBar;
    QLabel *m_rightLabel;
    
    // Menu actions
    QAction *m_actionOpen;
    QAction *m_actionOpenURL;
    QAction *m_actionExit;
    QActionGroup *m_actionGroupPicFormat;
    QAction *m_actionSetPath;
    QAction *m_actionAbout;
    QAction *m_actionWatermark;
    
    // Toolbar actions
    QAction *m_actionPrev;
    QAction *m_actionPlayPause;
    QAction *m_actionStop;
    QAction *m_actionNext;
    QAction *m_actionSnap;
    QAction *m_actionSlower;
    QAction *m_actionFaster;

    // Timers
    QTimer *m_playTimer;
    QTimer *m_posTimer;
    
    // Media player
    class MediaPlayerWrapper *m_mediaPlayer;
    
    // State variables
    bool m_bStartDraw;
    QPoint m_StartPoint;
    QRect m_rcDraw;
    bool m_sliderDragging;
    bool m_seekSliderClickable;
    bool m_fileReferenceCreated;
    QVector<float> m_speedLevels;
    int m_currentSpeedIndex;
    QString m_SnapPath;
    bool m_isMuted;
    int m_previousVolume;

    //Watermark dialog
    WatermarkDialog *m_watermarkDlg;

    // RTSP streaming
    RtspStreamThread *m_rtspThread;
    QString m_currentStreamUrl;
};

// RTSP Stream Thread for handling network streaming
class RtspStreamThread : public QThread
{
    Q_OBJECT

public:
    explicit RtspStreamThread(class MediaPlayerWrapper *player, const QString &url, QObject *parent = nullptr);
    ~RtspStreamThread();

    void stopStream();

signals:
    void streamStarted();
    void streamError(const QString &error);
    void streamStopped();

protected:
    void run() override;

private:
    class MediaPlayerWrapper *m_player;
    QString m_url;
    QProcess *m_ffmpegProcess;
    volatile bool m_running;
};

#endif // PLAYERDIALOG_H
