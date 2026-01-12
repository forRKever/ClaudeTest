#include "PlayerDialog.h"
#include "ui_PlayerDialog.h"
#include "MediaPlayerWrapper.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QWidget>
#include <QDebug>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QUrl>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>

PlayerDialog::PlayerDialog(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::PlayerDialog)
    , m_centralWidget(nullptr)
    , m_mainLayout(nullptr)
    , m_buttonLayout(nullptr)
    , m_playButton(nullptr)
    , m_pauseButton(nullptr)
    , m_stopButton(nullptr)
    , m_fastButton(nullptr)
    , m_slowButton(nullptr)
    , m_preButton(nullptr)
    , m_nextButton(nullptr)
    , m_snapButton(nullptr)
    , m_voiceButton(nullptr)
    , m_seekSlider(nullptr)
    , m_volSlider(nullptr)
    , m_videoDisplayWidget(nullptr)
    , m_controlPanelWidget(nullptr)
    , m_menuBar(nullptr)
    , m_statusBar(nullptr)
    , m_actionOpen(nullptr)
    , m_actionOpenURL(nullptr)
    , m_actionExit(nullptr)
    , m_actionGroupPicFormat(nullptr)
    , m_actionSetPath(nullptr)
    , m_actionAbout(nullptr)
    , m_actionWatermark(nullptr)
    , m_rtspThread(nullptr)
    , m_actionPrev(nullptr)
    , m_actionPlayPause(nullptr)
    , m_actionStop(nullptr)
    , m_actionNext(nullptr)
    , m_actionSnap(nullptr)
    , m_actionSlower(nullptr)
    , m_actionFaster(nullptr)
    , m_playTimer(nullptr)
    , m_posTimer(nullptr)
    , m_mediaPlayer(nullptr)
    , m_bStartDraw(false)
    , m_sliderDragging(false)
{
    setWindowTitle("Media Player");
    resize(400, 400);
    setAcceptDrops(true);  // Enable drag and drop
    setupUI();
    setupTimers();
    setupMenus();
    
    // Initialize media player
    m_mediaPlayer = new MediaPlayerWrapper(this);
    m_mediaPlayer->initialize();
    
    m_watermarkDlg = new WatermarkDialog(m_mediaPlayer, this);
    // Connect media player signals
    connect(m_mediaPlayer, &MediaPlayerWrapper::statusChanged, this, [this](int state) {
        qDebug() << "Media player status changed:" << state;
        updateButtonStates();
        updatePlayPauseAction();
        
        // Update status bar based on state
        if (state == MediaPlayerWrapper::Playing) {
            m_statusBar->showMessage("Playing...");
        } else if (state == MediaPlayerWrapper::Paused) {
            m_statusBar->showMessage("Paused");
        } else if (state == MediaPlayerWrapper::Stopped) {
            m_statusBar->showMessage("Stopped");
        } else if (state == MediaPlayerWrapper::Step) {
            m_statusBar->showMessage("Step");
        }
    });
    
    connect(m_mediaPlayer, &MediaPlayerWrapper::errorOccurred, this, [this](const QString &error) {
        QMessageBox::critical(this, "Playback Error", error);
        m_statusBar->showMessage("Error: " + error);
    });

    connect(m_mediaPlayer, &MediaPlayerWrapper::fileRefCreated, this, [this]() {
        qDebug()<<"File reference create success";
        m_fileReferenceCreated = true;
    });

    connect(m_watermarkDlg, &QDialog::finished, this, [this]() {
        m_actionWatermark->setEnabled(true);
    });
}

PlayerDialog::~PlayerDialog()
{
    if (m_playTimer) {
        m_playTimer->stop();
    }
    
    if (m_posTimer) {
        m_posTimer->stop();
    }
    
    if (m_rtspThread) {
        m_rtspThread->stopStream();
        m_rtspThread->wait(3000);
        delete m_rtspThread;
        m_rtspThread = nullptr;
    }

    if (m_mediaPlayer) {
        m_mediaPlayer->cleanup();
    }
    if(m_watermarkDlg)
    {
        m_watermarkDlg->close();
        delete m_watermarkDlg;
        m_watermarkDlg = nullptr;
    }
    delete ui;
}

void PlayerDialog::setupUI()
{
    // Setup UI from .ui file
    ui->setupUi(this);
    setWindowIcon(QIcon(":/PICS/shinsoft.ico"));
    // Get references to widgets from UI
    m_seekSlider = ui->seekSlider;
    m_volSlider = ui->volSlider;
    m_playButton = ui->playButton;
    m_pauseButton = ui->pauseButton;
    m_stopButton = ui->stopButton;
    m_fastButton = ui->fastButton;
    m_slowButton = ui->slowButton;
    m_preButton = ui->preButton;
    m_nextButton = ui->nextButton;
    m_snapButton = ui->snapButton;
    m_voiceButton = ui->volumeButton;

    m_playButton->setIcon(QIcon(":/PICS/play.png"));
    m_pauseButton->setIcon(QIcon(":/PICS/pause.png"));
    m_stopButton->setIcon(QIcon(":/PICS/stop.png"));
    m_fastButton->setIcon(QIcon(":/PICS/fast.png"));
    m_slowButton->setIcon(QIcon(":/PICS/slow.png"));
    m_preButton->setIcon(QIcon(":/PICS/pre.png"));
    m_nextButton->setIcon(QIcon(":/PICS/next.png"));
    m_snapButton->setIcon(QIcon(":/PICS/snap.png"));
    m_voiceButton->setIcon(QIcon(":/PICS/voice.png"));

    m_videoDisplayWidget = ui->videoDisplayWidget_2;
    //m_controlPanelWidget = ui->controlPanelWidget;
    
    // Connect seek slider signals
    if (m_seekSlider) {
        m_seekSlider->setRange(0, 10000);

        connect(m_seekSlider, &QSlider::sliderPressed, this, &PlayerDialog::onSliderPressed);
        connect(m_seekSlider, &QSlider::sliderReleased, this, &PlayerDialog::onSliderReleased);
        connect(m_seekSlider, &QSlider::valueChanged, this, &PlayerDialog::onSliderValueChanged);

        m_seekSlider->installEventFilter(this);
    }

    m_seekSliderClickable = false;
    m_seekSlider->setEnabled(m_seekSliderClickable);
    m_fileReferenceCreated = false;
    
    // Connect volume slider signals
    if (m_volSlider) {
        connect(m_volSlider, &QSlider::valueChanged, this, &PlayerDialog::onVolumeChanged);
        m_previousVolume = m_volSlider->value();
        m_isMuted = false;
    }
    
    // Connect button signals
    if (m_playButton) connect(m_playButton, &QPushButton::clicked, this, &PlayerDialog::onPlayClicked);
    if (m_pauseButton) connect(m_pauseButton, &QPushButton::clicked, this, &PlayerDialog::onPauseClicked);
    if (m_stopButton) connect(m_stopButton, &QPushButton::clicked, this, &PlayerDialog::onStopClicked);
    if (m_fastButton) connect(m_fastButton, &QPushButton::clicked, this, &PlayerDialog::onActionFaster);
    if (m_slowButton) connect(m_slowButton, &QPushButton::clicked, this, &PlayerDialog::onActionSlower);
    if (m_preButton) connect(m_preButton, &QPushButton::clicked, this, &PlayerDialog::onActionPrev);
    if (m_nextButton) connect(m_nextButton, &QPushButton::clicked, this, &PlayerDialog::onActionNext);
    if (m_snapButton) connect(m_snapButton, &QPushButton::clicked, this, &PlayerDialog::onActionSnap);
    if (m_voiceButton) connect(m_voiceButton, &QPushButton::clicked, this, &PlayerDialog::onVolumeBtnClicked);
    
    m_speedLevels = {
            1.0f/16.0f,  // 0: /16 (0.0625x)
            1.0f/8.0f,   // 1: /8  (0.125x)
            1.0f/4.0f,   // 2: /4  (0.25x)
            1.0f/2.0f,   // 3: /2  (0.5x)
            1.0f,        // 4: x1  (1.0x) - 正常速度
            2.0f,        // 5: x2  (2.0x)
            4.0f,        // 6: x4  (4.0x)
            8.0f,        // 7: x8  (8.0x)
            16.0f        // 8: x16 (16.0x)
        };
    m_currentSpeedIndex = 4;

    // Setup status bar
    m_rightLabel = new QLabel(this);
    m_statusBar = statusBar();
    m_statusBar->showMessage("Ready");
    statusBar()->addPermanentWidget(m_rightLabel);
    ui->videoLayout->addWidget(m_videoDisplayWidget);
    m_videoDisplayWidget->setStyleSheet("background-color:black;");
}

void PlayerDialog::onPlayClicked()
{
    qDebug() << "Play clicked";
    
    if (!m_mediaPlayer) {
        return;
    }
    
    // If no file is opened, show file dialog
//    if (!m_mediaPlayer->isFileOpened()) {
//        QString fileName = QFileDialog::getOpenFileName(this,
//            "Open Media File",
//            QString(),
//            "Media Files (*.mp4 *.avi *.mkv *.264 *.h264);;All Files (*.*)");
            
//        if (fileName.isEmpty()) {
//            return;
//        }
        
//        if (!m_mediaPlayer->openFile(fileName)) {
//            return;
//        }
//    }

    if (m_mediaPlayer->isPaused()) {
        // Resume from pause
        if (m_mediaPlayer->resume()) {
//            m_playButton->setText("⏸ Pause");
            m_mediaPlayer->playSound();
            m_posTimer->start(200);
        }
    } else if (!m_mediaPlayer->isStep() && !m_mediaPlayer->isPlaying()) {
        // Start playing - use video display widget for rendering
        HWND displayWnd = m_videoDisplayWidget ? 
            reinterpret_cast<HWND>(m_videoDisplayWidget->winId()) : 
            reinterpret_cast<HWND>(winId());
        if (m_mediaPlayer->play(displayWnd)) {
            m_currentSpeedIndex = 4;
            m_mediaPlayer->playSound();
            m_watermarkDlg->m_setTimer(true);
//            adjustWindowSize();
//            if (m_playButton) m_playButton->setText("⏸ Pause");
            m_playTimer->start(500); // Start status update timer
            m_posTimer->start(200);  // Start position update timer
        }
    }else if(m_mediaPlayer->isStep()){
        m_mediaPlayer->play();
        m_currentSpeedIndex = 4;
        m_mediaPlayer->playSound();
    }
    else {
        // Currently playing, pause it
        if (m_mediaPlayer->pause()) {
            m_mediaPlayer->stopSound();
//            m_playButton->setText("▶ Resume");
            m_posTimer->stop();
        }
    }
}

void PlayerDialog::onPauseClicked()
{
    qDebug() << "Pause clicked";
    
    if (m_mediaPlayer && m_mediaPlayer->isPlaying()) {
        if (m_mediaPlayer->pause()) {
            m_mediaPlayer->stopSound();
//            m_playButton->setText("▶ Resume");
            m_posTimer->stop();
        }
    }
}

void PlayerDialog::onStopClicked()
{
    qDebug() << "Stop clicked";

    // Stop RTSP stream if active
    if (m_rtspThread) {
        m_rtspThread->stopStream();
        m_rtspThread->wait(3000);
        delete m_rtspThread;
        m_rtspThread = nullptr;
    }

    if (m_mediaPlayer) {
        m_mediaPlayer->stopSound();
        if (m_mediaPlayer->isStreamMode()) {
            m_mediaPlayer->closeStream();
        } else {
            m_mediaPlayer->stop();
        }
        m_playTimer->stop();
        m_posTimer->stop();
        m_watermarkDlg->m_setTimer(false);
//        m_playButton->setText("▶ Play");
        m_seekSlider->setValue(0);
    }
}

void PlayerDialog::setupTimers()
{
    // Create timer matching original SetTimer(PLAY_TIMER, 500, NULL)
    m_playTimer = new QTimer(this);
    connect(m_playTimer, &QTimer::timeout, this, &PlayerDialog::onTimerUpdate);
    
    // Create position update timer (200ms as requested)
    m_posTimer = new QTimer(this);
    connect(m_posTimer, &QTimer::timeout, this, &PlayerDialog::updatePosition);
}

void PlayerDialog::setupMenus()
{
    // Create menu bar
    m_menuBar = ui->menubar;//menuBar();
    m_actionOpen = ui->actionOpen;
    m_actionOpenURL = ui->actionOpenURL;
    m_actionExit = ui->actionExit;
    m_actionSetPath = ui->actionSet_Cap_Pic_Path;
    m_actionAbout = ui->actionAbout;
    m_SnapPath = QCoreApplication::applicationDirPath();
    m_actionWatermark = ui->actionWatermark;

    connect(m_actionOpen, &QAction::triggered, this, &PlayerDialog::onActionOpen);
    connect(m_actionOpenURL, &QAction::triggered, this, &PlayerDialog::onActionOpenURL);
    connect(m_actionExit, &QAction::triggered, this, &PlayerDialog::onActionExit);
    connect(m_actionSetPath, &QAction::triggered, this, &PlayerDialog::onActionSetPath);
    connect(m_actionAbout, &QAction::triggered, this, &PlayerDialog::onAcionAbout);
    connect(m_actionWatermark, &QAction::triggered, this, &PlayerDialog::onAcionWatermark);

    m_actionGroupPicFormat = new QActionGroup(this);
    m_actionGroupPicFormat->addAction(ui->actionBMP);
    m_actionGroupPicFormat->addAction(ui->actionJPEG);
    m_actionGroupPicFormat->setExclusive(true);
    ui->actionBMP->setChecked(true);
    
    // Add actions to File menu
//    fileMenu->addAction(m_actionOpen);
//    fileMenu->addSeparator();
//    fileMenu->addAction(m_actionExit);
    

    
//    // Connect toolbar actions
//    if (m_actionPrev) connect(m_actionPrev, &QAction::triggered, this, &PlayerDialog::onActionPrev);
//    if (m_actionPlayPause) connect(m_actionPlayPause, &QAction::triggered, this, &PlayerDialog::onActionPlayPause);
//    if (m_actionStop) connect(m_actionStop, &QAction::triggered, this, &PlayerDialog::onActionStop);
//    if (m_actionNext) connect(m_actionNext, &QAction::triggered, this, &PlayerDialog::onActionNext);
//    if (m_actionSnap) connect(m_actionSnap, &QAction::triggered, this, &PlayerDialog::onActionSnap);
//    if (m_actionSlower) connect(m_actionSlower, &QAction::triggered, this, &PlayerDialog::onActionSlower);
//    if (m_actionFaster) connect(m_actionFaster, &QAction::triggered, this, &PlayerDialog::onActionFaster);
    
    // Create other menus (empty for now)
//    m_menuBar->addMenu("View");
//    m_menuBar->addMenu("Control");
//    m_menuBar->addMenu("Option");
//    m_menuBar->addMenu("Help");
}

void PlayerDialog::onTimerUpdate()
{
    // Call the wrapper function to maintain compatibility
    OnTimer(PLAY_TIMER);
}

// Qt event handlers
void PlayerDialog::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        unsigned int nFlags = 0;
        if (event->modifiers() & Qt::ControlModifier) nFlags |= 0x0008;  // MK_CONTROL
        if (event->modifiers() & Qt::ShiftModifier) nFlags |= 0x0004;    // MK_SHIFT
        
        OnLButtonDown(nFlags, event->pos());
    }
    QMainWindow::mousePressEvent(event);
}

void PlayerDialog::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        unsigned int nFlags = 0;
        if (event->modifiers() & Qt::ControlModifier) nFlags |= 0x0008;
        if (event->modifiers() & Qt::ShiftModifier) nFlags |= 0x0004;
        
        OnLButtonUp(nFlags, event->pos());
    }
    QMainWindow::mouseReleaseEvent(event);
}

void PlayerDialog::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        unsigned int nFlags = 0;
        if (event->modifiers() & Qt::ControlModifier) nFlags |= 0x0008;
        if (event->modifiers() & Qt::ShiftModifier) nFlags |= 0x0004;
        
        OnLButtonDblClk(nFlags, event->pos());
    }
    QMainWindow::mouseDoubleClickEvent(event);
}

void PlayerDialog::mouseMoveEvent(QMouseEvent *event)
{
    // Implement if needed for drawing functionality
    QMainWindow::mouseMoveEvent(event);
}

void PlayerDialog::keyPressEvent(QKeyEvent *event)
{
    unsigned int nChar = event->key();
    unsigned int nRepCnt = 1;  // Qt doesn't provide repeat count directly
    unsigned int nFlags = 0;
    
    OnKeyDown(nChar, nRepCnt, nFlags);
    QMainWindow::keyPressEvent(event);
}

void PlayerDialog::closeEvent(QCloseEvent *event)
{
    OnClose();
    QMainWindow::closeEvent(event);
}

void PlayerDialog::resizeEvent(QResizeEvent *event)
{
    OnSize(0, event->size().width(), event->size().height());
    m_videoDisplayWidget->resize(event->size().width()-20, event->size().height()-110);
    QMainWindow::resizeEvent(event);
}

void PlayerDialog::moveEvent(QMoveEvent *event)
{
    OnMove(event->pos().x(), event->pos().y());
    QMainWindow::moveEvent(event);
}

void PlayerDialog::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void PlayerDialog::dropEvent(QDropEvent *event)
{
    QStringList files;
    foreach (const QUrl &url, event->mimeData()->urls()) {
        files << url.toLocalFile();
    }
    if (!files.isEmpty()) {
        OnDropFiles(files);
    }
}

bool PlayerDialog::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == m_seekSlider && m_seekSliderClickable) {
        if (event->type() == QEvent::MouseButtonPress) {
            QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
            if (mouseEvent->button() == Qt::LeftButton) {
                // 計算點擊位置對應的數值
                int sliderValue = calculateSliderValueFromPosition(mouseEvent->pos());
                m_seekSlider->setValue(sliderValue);

                // 立即執行跳轉
                if (m_mediaPlayer && m_mediaPlayer->isFileOpened()) {
                    qint64 totalPos = m_mediaPlayer->duration();
                    if (totalPos > 0) {
                        qint64 newPos = (sliderValue * totalPos) / 10000;  // 使用新的範圍
                        m_mediaPlayer->seekMs(newPos);
//                        m_statusBar->showMessage(QString("Seeking to: %1%").arg(sliderValue / 100.0, 0, 'f', 1), 1000);
                    }
                }
                return true;  // 事件已處理
            }
        }
    }
    return QMainWindow::eventFilter(obj, event);
}

// Wrapper functions matching original MFC names
void PlayerDialog::OnTimer(int nIDEvent)
{
    if (nIDEvent == PLAY_TIMER) {
        DrawStatus();
    }
}

void PlayerDialog::OnLButtonDown(unsigned int nFlags, QPoint point)
{
    qDebug() << "OnLButtonDown - flags:" << nFlags << "point:" << point;
    // Original code (commented out):
    // m_StartPoint = point;
    // m_bStartDraw = TRUE;
    // InvalidateRect(m_rcDraw, TRUE);
    // ZeroMemory(&m_rcDraw, sizeof(m_rcDraw));
}

void PlayerDialog::OnLButtonUp(unsigned int nFlags, QPoint point)
{
    qDebug() << "OnLButtonUp - flags:" << nFlags << "point:" << point;
    // Original code (commented out) handled drawing completion
}

void PlayerDialog::OnLButtonDblClk(unsigned int nFlags, QPoint point)
{
    qDebug() << "OnLButtonDblClk - flags:" << nFlags << "point:" << point;
    // Original implementation toggled fullscreen
}

void PlayerDialog::OnKeyDown(unsigned int nChar, unsigned int nRepCnt, unsigned int nFlags)
{
    qDebug() << "OnKeyDown - char:" << nChar << "repCnt:" << nRepCnt << "flags:" << nFlags;
    // Original implementation handled keyboard shortcuts
}

void PlayerDialog::OnSize(unsigned int nType, int cx, int cy)
{
    qDebug() << "OnSize - type:" << nType << "width:" << cx << "height:" << cy;
    // Original called SortControl() to adjust control positions
}

void PlayerDialog::OnMove(int x, int y)
{
//    qDebug() << "OnMove - x:" << x << "y:" << y;
    // Original implementation saved window position
}

void PlayerDialog::OnDropFiles(const QStringList &files)
{
    qDebug() << "OnDropFiles - files:" << files;
    
    if (files.isEmpty() || !m_mediaPlayer) {
        return;
    }
    
    // Stop current playback if any
    if (m_mediaPlayer->isPlaying() || m_mediaPlayer->isStep()) {
        m_mediaPlayer->stopSound();
        m_mediaPlayer->stop();
        m_watermarkDlg->m_setTimer(false);
        m_playTimer->stop();
    }
    
    // Open the first dropped file
    QString filePath = files.first();
    if (m_mediaPlayer->openFile(filePath)) {
        QFileInfo fileInfo(filePath);
        QString status = QString("Playing: %1").arg(fileInfo.fileName());
        m_statusBar->showMessage(status);
        qDebug() << "Opened dropped file:" << filePath;
        this->setWindowTitle(filePath);
        
        if (m_seekSlider) {
            m_seekSlider->setValue(0);
        }

        // Reset volume to 80
        if (m_volSlider) {
            m_volSlider->setValue(80);
            onVolumeChanged(80);
        }

        //Reset play speed
        m_currentSpeedIndex = 4;
        m_mediaPlayer->setPlaySpeed(1.0f);
        
        // Auto-play the dropped file
        HWND displayWnd = m_videoDisplayWidget ? 
            reinterpret_cast<HWND>(m_videoDisplayWidget->winId()) : 
            reinterpret_cast<HWND>(winId());
        m_mediaPlayer->play(displayWnd);
        m_mediaPlayer->playSound();
        adjustWindowSize();
        m_watermarkDlg->m_setTimer(true);
        if(!m_watermarkDlg->isVisible())
            m_actionWatermark->setEnabled(true);
        m_playTimer->start(500);
        m_posTimer->start(200);
    } else {
        m_statusBar->showMessage("Failed to open dropped file");
    }
}

void PlayerDialog::OnClose()
{
    qDebug() << "OnClose";
    // Original implementation cleaned up resources
}

void PlayerDialog::OnDestroy()
{
    qDebug() << "OnDestroy";
    // Original implementation performed final cleanup
}

void PlayerDialog::DrawStatus()
{
    if (!m_mediaPlayer || (!m_mediaPlayer->isPlaying() && !m_mediaPlayer->isStep())) {
        return;
    }
    
    // Get playback information
    DWORD currentFrame = m_mediaPlayer->getCurrentFrameNum();
    DWORD totalFrame = m_mediaPlayer->getTotalFrames();

    DWORD playedTime = m_mediaPlayer->getPlayedTime();
    DWORD totalTime = m_mediaPlayer->getFileTime();
    
    QString sRightText = QString("Pos: %1/%2  ,  Time: %3/%4")
        .arg(currentFrame)
        .arg(totalFrame)
        .arg(formatTime(playedTime))
        .arg(formatTime(totalTime));
    
    m_rightLabel->setText(sRightText);
    // Update UI elements here (progress bar, time display, etc.)
    // This is where the original implementation updated the display
}

void PlayerDialog::updateButtonStates()
{
    if (!m_mediaPlayer) {
        return;
    }
    
    bool isPlaying = m_mediaPlayer->isPlaying();
    bool isPaused = m_mediaPlayer->isPaused();
    bool isStep = m_mediaPlayer->isStep();
    bool isStop = m_mediaPlayer->isStop();
    
    // Update button enabled states
    if(isStep)
    {
        m_playButton->setEnabled(true);
        m_pauseButton->setEnabled(false);
    }
    else
    {
        m_playButton->setEnabled(!isPlaying || isPaused);
        m_pauseButton->setEnabled(isPlaying && !isPaused);
    }
    m_stopButton->setEnabled(isPlaying || isPaused || isStep);
    m_nextButton->setEnabled(!isStop);
    m_preButton->setEnabled(!isStop);
    m_fastButton->setEnabled(!isStop);
    m_slowButton->setEnabled(!isStop);
    m_seekSliderClickable = !isStop;
    m_seekSlider->setEnabled(m_seekSliderClickable);
}

void PlayerDialog::updatePosition()
{
    if (!m_mediaPlayer || !m_mediaPlayer->isFileOpened() || m_sliderDragging) {
        return;
    }
    
    qint64 currentPos = m_mediaPlayer->position();
    qint64 totalPos = m_mediaPlayer->duration();
    
    if (totalPos > 0) {
        int sliderValue = (int)((currentPos * 10000) / totalPos);
        m_seekSlider->setValue(sliderValue);
    }
}

void PlayerDialog::onSliderPressed()
{
    m_sliderDragging = true;
    m_posTimer->stop();
}

void PlayerDialog::onSliderReleased()
{
    if (!m_mediaPlayer || !m_mediaPlayer->isFileOpened()) {
        m_sliderDragging = false;
        return;
    }
    
    qint64 totalPos = m_mediaPlayer->duration();
    if (totalPos > 0) {
        int sliderValue = m_seekSlider->value();
        qint64 newPos = (sliderValue * totalPos) / 10000;
        m_mediaPlayer->seekMs(newPos);
    }
    
    m_sliderDragging = false;
    if (m_mediaPlayer->isPlaying()) {
        m_posTimer->start(200);
    }
}

void PlayerDialog::onSliderValueChanged(int value)
{
    // This is called when the slider value changes
    // We only handle it during dragging, seeking is done on release
    Q_UNUSED(value)
}

void PlayerDialog::onActionOpen()
{
    qDebug() << "Action Open triggered";
    
    if (!m_mediaPlayer) {
        return;
    }
    
    QString fileName = QFileDialog::getOpenFileName(this, 
        "Open Media File", 
        QString(), 
        "Media Files (*.mp4 *.avi *.mkv *.264 *.h264);;All Files (*.*)");
        
    if (fileName.isEmpty()) {
        return;
    }
    
    // Stop current playback if any
    if (m_mediaPlayer->isPlaying() || m_mediaPlayer->isStep()) {
        m_mediaPlayer->stopSound();
        m_mediaPlayer->stop();
        m_watermarkDlg->m_setTimer(false);
        m_playTimer->stop();
        m_posTimer->stop();
    }
    
    // Open the file
    if (m_mediaPlayer->openFile(fileName)) {
        QFileInfo fileInfo(fileName);
        QString status = QString("Loaded: %1").arg(fileInfo.fileName());
        m_statusBar->showMessage(status);
        qDebug() << "Opened file:" << fileName;
        this->setWindowTitle(fileName);
        
        // Reset UI
        if (m_seekSlider) m_seekSlider->setValue(0);
        
        // Reset volume to 80
        if (m_volSlider) {
            m_volSlider->setValue(80);
            // Trigger volume change to sync with media player
            onVolumeChanged(80);
        }

        //Reset play speed
        m_currentSpeedIndex = 4;
        m_mediaPlayer->setPlaySpeed(1.0f);

        HWND displayWnd = m_videoDisplayWidget ?
            reinterpret_cast<HWND>(m_videoDisplayWidget->winId()) :
            reinterpret_cast<HWND>(winId());
        m_mediaPlayer->play(displayWnd);
        m_mediaPlayer->playSound();
        adjustWindowSize();
        m_watermarkDlg->m_setTimer(true);
        if(!m_watermarkDlg->isVisible())
            m_actionWatermark->setEnabled(true);
        m_playTimer->start(500);
        m_posTimer->start(200);

    } else {
        m_statusBar->showMessage("Failed to open file");
    }
}

void PlayerDialog::onActionOpenURL()
{
    qDebug() << "Action Open URL triggered";

    if (!m_mediaPlayer) {
        return;
    }

    bool ok;
    QString url = QInputDialog::getText(this,
        "Open URL",
        "Enter RTSP/HTTP stream URL:",
        QLineEdit::Normal,
        m_currentStreamUrl.isEmpty() ? "rtsp://" : m_currentStreamUrl,
        &ok);

    if (!ok || url.isEmpty()) {
        return;
    }

    // Stop current playback if any
    if (m_rtspThread) {
        m_rtspThread->stopStream();
        m_rtspThread->wait(3000);
        delete m_rtspThread;
        m_rtspThread = nullptr;
    }

    if (m_mediaPlayer->isPlaying() || m_mediaPlayer->isStep()) {
        m_mediaPlayer->stopSound();
        m_mediaPlayer->stop();
        m_watermarkDlg->m_setTimer(false);
        m_playTimer->stop();
        m_posTimer->stop();
    }

    m_currentStreamUrl = url;
    m_statusBar->showMessage("Connecting to stream...");

    // Open stream in MediaPlayerWrapper
    if (!m_mediaPlayer->openStream(url)) {
        QMessageBox::critical(this, "Stream Error", "Failed to initialize stream playback.");
        m_statusBar->showMessage("Failed to open stream");
        return;
    }

    // Create and start RTSP thread
    m_rtspThread = new RtspStreamThread(m_mediaPlayer, url, this);

    connect(m_rtspThread, &RtspStreamThread::streamStarted, this, [this]() {
        qDebug() << "Stream started";
        m_statusBar->showMessage("Stream connected");
        this->setWindowTitle(m_currentStreamUrl);

        // Start playback
        HWND displayWnd = m_videoDisplayWidget ?
            reinterpret_cast<HWND>(m_videoDisplayWidget->winId()) :
            reinterpret_cast<HWND>(winId());
        m_mediaPlayer->play(displayWnd);
        m_mediaPlayer->playSound();
        m_playTimer->start(500);
    });

    connect(m_rtspThread, &RtspStreamThread::streamError, this, [this](const QString &error) {
        qDebug() << "Stream error:" << error;
        m_statusBar->showMessage("Stream error: " + error);
        QMessageBox::warning(this, "Stream Error", error);
    });

    connect(m_rtspThread, &RtspStreamThread::streamStopped, this, [this]() {
        qDebug() << "Stream stopped";
        m_statusBar->showMessage("Stream disconnected");
    });

    m_rtspThread->start();
}

void PlayerDialog::onActionExit()
{
    qDebug() << "Action Exit triggered";
    QApplication::quit();
}

void PlayerDialog::onActionSetPath()
{
    qDebug() << "Action Set path triggered";
    QString dir = QFileDialog::getExistingDirectory(this,
                    "Select a directory",
                    m_SnapPath,
                    QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);
    if(!dir.isEmpty())
        m_SnapPath = dir;
}

void PlayerDialog::onAcionAbout()
{
    qDebug() << "Action About triggered";
    QMessageBox aboutBox(this);
    aboutBox.setWindowTitle("About ShinPlayer");
    aboutBox.setIconPixmap(QPixmap(":/PICS/shinsoft.ico"));
    aboutBox.setText("<b>Version 1.0.1</b><br/><br/>SHINSOFT CO., LTD<br/>");
    aboutBox.exec();
}

void PlayerDialog::onAcionWatermark()
{
    m_actionWatermark->setEnabled(false);
    m_watermarkDlg->move(this->pos().x()+5, this->pos().y()+30);
    m_watermarkDlg->m_setTimer(true);
    m_watermarkDlg->show();
}

void PlayerDialog::onStatusChanged(const QString &status)
{
    m_statusBar->showMessage(status);
}

void PlayerDialog::adjustWindowSize()
{
    if (!m_mediaPlayer || !m_mediaPlayer->isFileOpened()) return;

    LONG w = 0, h = 0;
    if (!m_mediaPlayer->getPictureSize(&w, &h)) {
        qDebug() << "get size false";
        return;
    }

    int videoWidth = static_cast<int>(w);
    int videoHeight = static_cast<int>(h);


    if (videoWidth <= 0 || videoHeight <= 0) return;

    // 取得螢幕大小
    QRect screenGeometry = QGuiApplication::primaryScreen()->availableGeometry();
    int screenWidth = screenGeometry.width();
    int screenHeight = screenGeometry.height() - 40;

    // 限制最大視窗大小為螢幕大小
    int finalWidth = qMin(videoWidth, screenWidth);
    int finalHeight = qMin(videoHeight, screenHeight);

    // 將視窗移動到螢幕中央
    int x = (screenWidth - finalWidth) / 2 + screenGeometry.left();
    int y = (screenHeight - finalHeight) / 2 + screenGeometry.top();

    resize(finalWidth, finalHeight);
    move(x, y);
}

// 新增：根據鼠標位置計算滑桿數值的輔助函數
int PlayerDialog::calculateSliderValueFromPosition(const QPoint &pos)
{
    if (!m_seekSlider) return 0;

    QStyleOptionSlider option;
    option.initFrom(m_seekSlider);
    option.rect = m_seekSlider->rect();
    option.sliderPosition = m_seekSlider->value();
    option.sliderValue = m_seekSlider->value();
    option.minimum = m_seekSlider->minimum();
    option.maximum = m_seekSlider->maximum();
    option.orientation = m_seekSlider->orientation();

    // 獲取滑桿軌道的有效區域
    QRect grooveRect = m_seekSlider->style()->subControlRect(
        QStyle::CC_Slider, &option, QStyle::SC_SliderGroove, m_seekSlider);

    int sliderLength, sliderPos;
    if (m_seekSlider->orientation() == Qt::Horizontal) {
        sliderLength = grooveRect.width();
        sliderPos = pos.x() - grooveRect.x();
    } else {
        sliderLength = grooveRect.height();
        sliderPos = grooveRect.bottom() - pos.y();  // 垂直滑桿需要反轉
    }

    // 計算對應的數值
    int range = m_seekSlider->maximum() - m_seekSlider->minimum();
    int value = m_seekSlider->minimum() + (sliderPos * range) / sliderLength;

    // 確保數值在有效範圍內
    return qBound(m_seekSlider->minimum(), value, m_seekSlider->maximum());
}

QString PlayerDialog::formatTime(qint64 seconds)
{
    qint64 minutes = seconds / 60;
    qint64 hours = minutes / 60;

    seconds %= 60;
    minutes %= 60;

    if (hours > 0) {
        return QString("%1:%2:%3")
            .arg(hours, 2, 10, QChar('0'))
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    } else {
        return QString("%1:%2")
            .arg(minutes, 2, 10, QChar('0'))
            .arg(seconds, 2, 10, QChar('0'));
    }
}

QString PlayerDialog::formatSpeedText(float speed)
{
    if (speed == 1.0f) {
        return "Normal (x1)";
    } else if (speed > 1.0f) {
        // 加速：x2, x4, x8, x16
        if (speed == 2.0f) return "Speed x2";
        if (speed == 4.0f) return "Speed x4";
        if (speed == 8.0f) return "Speed x8";
        if (speed == 16.0f) return "Speed x16";
        return QString("Speed x%1").arg(speed, 0, 'f', 1);
    } else {
        // 減速：/2, /4, /8, /16
        float divisor = 1.0f / speed;
        if (divisor == 2.0f) return "Speed /2";
        if (divisor == 4.0f) return "Speed /4";
        if (divisor == 8.0f) return "Speed /8";
        if (divisor == 16.0f) return "Speed /16";
        return QString("Speed /%1").arg(divisor, 0, 'f', 1);
    }
}

void PlayerDialog::onActionPlayPause()
{
    qDebug() << "Play/Pause toggle";
    if (!m_mediaPlayer || !m_mediaPlayer->isFileOpened()) {
        // No file opened, trigger file open
        onActionOpen();
        return;
    }
    
    if (m_mediaPlayer->isPlaying()) {
        // Currently playing, pause it
        m_mediaPlayer->pause();
        m_mediaPlayer->stopSound();
    } else {
        // Not playing (stopped or paused), start/resume
        if (m_mediaPlayer->isPaused()) {
            m_mediaPlayer->resume();
        } else {
            HWND displayWnd = m_videoDisplayWidget ? 
                reinterpret_cast<HWND>(m_videoDisplayWidget->winId()) : 
                reinterpret_cast<HWND>(winId());
            m_mediaPlayer->play(displayWnd);
            m_mediaPlayer->playSound();
            adjustWindowSize();
            m_posTimer->start(200);
        }
    }
}

void PlayerDialog::onActionStop()
{
    qDebug() << "Stop";
    if (m_mediaPlayer && m_mediaPlayer->isFileOpened()) {
        m_mediaPlayer->stopSound();
        m_mediaPlayer->stop();
        m_posTimer->stop();
        m_seekSlider->setValue(0);
    }
}

void PlayerDialog::onActionPrev()
{
    qDebug() << "Previous frame";
    if (m_mediaPlayer && m_mediaPlayer->isFileOpened()) {
        if(m_fileReferenceCreated)
            qDebug() <<m_mediaPlayer->stepFrame(-1);
        else
            m_statusBar->showMessage("This video does not support Step Backward");
    }
}

void PlayerDialog::onActionNext()
{
    qDebug() << "Next frame";
    if (m_mediaPlayer && m_mediaPlayer->isFileOpened()) {
        m_mediaPlayer->stepFrame(1);
    }
}

void PlayerDialog::onActionSnap()
{
    qDebug() << "Take snapshot";
    if (!m_mediaPlayer || !m_mediaPlayer->isFileOpened()) {
        QMessageBox::warning(this, "Snapshot", "No media file is currently loaded.");
        return;
    }
    
    int nPicFormat;
    if(m_actionGroupPicFormat->checkedAction()==ui->actionBMP)
        nPicFormat = 0;
    if(m_actionGroupPicFormat->checkedAction()==ui->actionJPEG)
        nPicFormat = 1;

    //QString exeDir = QCoreApplication::applicationDirPath();
    if (m_mediaPlayer->snapshot(m_SnapPath, nPicFormat)) {
        QString sSavePath = m_mediaPlayer->getLastSnapshotPath();
        m_statusBar->showMessage(QString("Snapshot saved: %1").arg(sSavePath));
    } else {
        QMessageBox::critical(this, "Snapshot", "Failed to take snapshot.");
        m_statusBar->showMessage("Snapshot failed");
    }
    qDebug() << "select format:"<<nPicFormat;
}

void PlayerDialog::onVolumeBtnClicked()
{
    qDebug() << "Volume button clicked";

    if (m_isMuted) {
        // 當前是靜音，恢復之前的音量
        m_isMuted = false;
        m_volSlider->setValue(m_previousVolume);
        onVolumeChanged(m_previousVolume);  // 實際設置音量
    } else {
        // 當前不是靜音，記住當前音量並設為靜音
        m_previousVolume = m_volSlider->value();
        m_isMuted = true;
        m_volSlider->setValue(0);
        onVolumeChanged(0);  // 實際設置音量為0
    }

    updateVolumeButtonIcon();
}

void PlayerDialog::updateVolumeButtonIcon()
{
    if (!m_voiceButton) {
            return;
    }

    if (m_volSlider->value() == 0 || m_isMuted) {
        m_voiceButton->setIcon(QIcon(":/PICS/mute.png"));
        m_voiceButton->setToolTip("Unmute");
    } else {
        m_voiceButton->setIcon(QIcon(":/PICS/voice.png"));
        m_voiceButton->setToolTip("Mute");
    }
}

void PlayerDialog::onActionSlower()
{
    qDebug() << "Decrease speed";
    if (m_mediaPlayer && m_mediaPlayer->isFileOpened()) {
        //float currentSpeed = m_speedLevels[m_currentSpeedIndex];//m_mediaPlayer->getPlaySpeed();
        if(m_currentSpeedIndex>0)
            m_currentSpeedIndex--;

        float newSpeed = m_speedLevels[m_currentSpeedIndex]; // If already slow or normal, go slower; if fast, go normal
        
        if (m_mediaPlayer->setSpeedMultiplier(newSpeed)) {
            m_statusBar->showMessage(formatSpeedText(newSpeed));
//            if (newSpeed == 1.0f) {
//                m_statusBar->showMessage("Speed: Normal (1.0x)");
//            } else {
//                m_statusBar->showMessage(QString("Speed: %1x").arg(newSpeed));
//            }
        } else {
            m_statusBar->showMessage("Speed control not available");
        }
    }
}

void PlayerDialog::onActionFaster()
{
    qDebug() << "Increase speed";
    if (m_mediaPlayer && m_mediaPlayer->isFileOpened()) {
//        float currentSpeed = m_mediaPlayer->getPlaySpeed();
        if (m_currentSpeedIndex < m_speedLevels.size() - 1)
                m_currentSpeedIndex++;

        float newSpeed = m_speedLevels[m_currentSpeedIndex]; // If already fast or normal, go faster; if slow, go normal
        
        if (m_mediaPlayer->setSpeedMultiplier(newSpeed)) {
            m_statusBar->showMessage(formatSpeedText(newSpeed));
//            if (newSpeed == 1.0f) {
//                m_statusBar->showMessage("Speed: Normal (1.0x)");
//            } else {
//                m_statusBar->showMessage(QString("Speed: %1x").arg(newSpeed));
//            }
        } else {
            m_statusBar->showMessage("Speed control not available");
        }
    }
}

void PlayerDialog::updatePlayPauseAction()
{
    if (!m_actionPlayPause || !m_mediaPlayer) {
        return;
    }
    
    if (m_mediaPlayer->isPlaying()) {
        m_actionPlayPause->setText("⏸ Pause");
        m_actionPlayPause->setToolTip("Pause");
    } else {
        m_actionPlayPause->setText("▶ Play");
        m_actionPlayPause->setToolTip("Play");
    }
}

void PlayerDialog::onVolumeChanged(int value)
{
    qDebug() << "Volume changed to:" << value;
    if (m_mediaPlayer) {
        // Convert 0-100 range to 0-65535 range for PlayM4
        WORD volume = static_cast<WORD>((value * 65535) / 100);
        m_mediaPlayer->setVolume(volume);
        m_statusBar->showMessage(QString("Volume: %1%").arg(value), 2000);
    }

    if (value == 0 && !m_isMuted) {
        m_isMuted = true;
    } else if (value > 0 && m_isMuted) {
        m_isMuted = false;
        m_previousVolume = value;
    } else if (value > 0) {
        m_previousVolume = value;
    }

    updateVolumeButtonIcon();
}

// RtspStreamThread implementation
RtspStreamThread::RtspStreamThread(MediaPlayerWrapper *player, const QString &url, QObject *parent)
    : QThread(parent)
    , m_player(player)
    , m_url(url)
    , m_ffmpegProcess(nullptr)
    , m_running(false)
{
}

RtspStreamThread::~RtspStreamThread()
{
    stopStream();
}

void RtspStreamThread::stopStream()
{
    m_running = false;
    if (m_ffmpegProcess) {
        m_ffmpegProcess->terminate();
        if (!m_ffmpegProcess->waitForFinished(3000)) {
            m_ffmpegProcess->kill();
        }
    }
}

void RtspStreamThread::run()
{
    m_running = true;

    // Use FFmpeg to capture RTSP stream and output raw H.264 data
    m_ffmpegProcess = new QProcess();

    QStringList args;
    args << "-rtsp_transport" << "tcp"    // Use TCP for RTSP (more reliable)
         << "-i" << m_url                  // Input URL
         << "-c:v" << "copy"               // Copy video codec (no re-encoding)
         << "-an"                          // Disable audio for now
         << "-f" << "h264"                 // Output format
         << "-";                           // Output to stdout

    m_ffmpegProcess->start("ffmpeg", args);

    if (!m_ffmpegProcess->waitForStarted(5000)) {
        emit streamError("Failed to start FFmpeg. Please ensure FFmpeg is installed and in PATH.");
        delete m_ffmpegProcess;
        m_ffmpegProcess = nullptr;
        return;
    }

    emit streamStarted();

    // Read data from FFmpeg and feed to PlayM4
    while (m_running && m_ffmpegProcess->state() == QProcess::Running) {
        if (m_ffmpegProcess->waitForReadyRead(100)) {
            QByteArray data = m_ffmpegProcess->readAllStandardOutput();
            if (!data.isEmpty() && m_player) {
                m_player->inputStreamData(reinterpret_cast<PBYTE>(data.data()),
                                          static_cast<DWORD>(data.size()));
            }
        }

        // Check for errors
        QByteArray errorData = m_ffmpegProcess->readAllStandardError();
        if (!errorData.isEmpty()) {
            QString errorStr = QString::fromLocal8Bit(errorData);
            if (errorStr.contains("error", Qt::CaseInsensitive) ||
                errorStr.contains("failed", Qt::CaseInsensitive)) {
                qDebug() << "FFmpeg error:" << errorStr;
            }
        }
    }

    m_ffmpegProcess->waitForFinished(1000);
    delete m_ffmpegProcess;
    m_ffmpegProcess = nullptr;

    emit streamStopped();
}
