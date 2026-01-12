#include "MediaPlayerWrapper.h"
#include <QDebug>
#include <QFileInfo>
#include <QDateTime>
#include <QFile>

MediaPlayerWrapper::MediaPlayerWrapper(QObject *parent)
    : QObject(parent)
    , m_lPort(-1)
    , m_playState(Stopped)
    , m_bInitialized(false)
    , m_bFileOpened(false)
    , m_displayWnd(nullptr)
    , m_currentSpeed(1.0f)
{
    qDebug() << "[Ctor] MediaPlayerWrapper created at" << this;
}

MediaPlayerWrapper::~MediaPlayerWrapper()
{
    cleanup();
}

bool MediaPlayerWrapper::initialize()
{
    if (m_bInitialized) {
        return true;
    }

    // Initialize DirectDraw device (from original code)
    NAME(PlayM4_InitDDrawDevice)();
    
    // Get DirectDraw device info
    DWORD deviceCount = NAME(PlayM4_GetDDrawDeviceTotalNums)();
    qDebug() << "DirectDraw devices found:" << deviceCount;
    
    if (deviceCount > 0) {
        char driverDesc[50];
        char driverName[50];
        HMONITOR hMonitor;
        NAME(PlayM4_GetDDrawDeviceInfo)(0, driverDesc, 50, driverName, 50, &hMonitor);
        qDebug() << "Using DirectDraw device:" << driverDesc;
    }

    m_bInitialized = true;
    return true;
}

void MediaPlayerWrapper::cleanup()
{
    if (m_bFileOpened) {
        closeFile();
    }
    
    if (m_bInitialized) {
        NAME(PlayM4_RealeseDDraw)();
        m_bInitialized = false;
    }
}

bool MediaPlayerWrapper::getPort()
{
    if (m_lPort >= 0) {
        return true;
    }

    if (!NAME(PlayM4_GetPort)(&m_lPort)) {
        qDebug() << "Failed to get play port";
        emit errorOccurred("Failed to get play port");
        return false;
    }

    qDebug() << "Got play port:" << m_lPort;
    
    // Set DirectDraw device for this port
    NAME(PlayM4_SetDDrawDevice)(m_lPort, 0);
    
    return true;
}

void MediaPlayerWrapper::releasePort()
{
    if (m_lPort >= 0) {
        NAME(PlayM4_FreePort)(m_lPort);
        m_lPort = -1;
    }
}

bool MediaPlayerWrapper::openFile(const QString &filePath)
{
    if (m_bFileOpened) {
        closeFile();
    }

    if (!QFileInfo::exists(filePath)) {
        emit errorOccurred("File does not exist: " + filePath);
        return false;
    }

    if (!getPort()) {
        return false;
    }

    // Convert QString to char* for SDK
    QByteArray filePathBytes = filePath.toLocal8Bit();

    setFileRefDoneCallback(MediaPlayerWrapper::fileRefCallBack, this);
    setCheckWatermarkCallback(MediaPlayerWrapper::watermarkCallBack, this);
//    if(!NAME(PlayM4_SetFileRefCallBack)(m_lPort, fileRefCallBack, static_cast<DWORD>(reinterpret_cast<DWORD_PTR>(this)))){
//        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
//        qDebug() << "Failed to setFileRefDoneCallback:" << getErrorString(error);
//    }

    if (!NAME(PlayM4_OpenFile)(m_lPort, filePathBytes.data())) {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        QString errorMsg = QString("Failed to open file: %1").arg(getErrorString(error));
        emit errorOccurred(errorMsg);
        releasePort();
        return false;
    }

    m_currentFile = filePath;
    m_bFileOpened = true;
    
    qDebug() << "File opened successfully:" << filePath;
    emit statusChanged(Stopped);
    
    return true;
}

void CALLBACK MediaPlayerWrapper::fileRefCallBack(DWORD nPort, void* nUser)
{
    MediaPlayerWrapper* self = reinterpret_cast<MediaPlayerWrapper*>(nUser);
    if (self) {
        qDebug() << "File reference created!";
        QMetaObject::invokeMethod(self, "onFileRefCreated", Qt::QueuedConnection);
    }
}

void CALLBACK MediaPlayerWrapper::watermarkCallBack(long nPort, WATERMARK_INFO* pInfo, void *nUser)
{
    MediaPlayerWrapper* wrapper = reinterpret_cast<MediaPlayerWrapper*>(nUser);
    if (!wrapper || !pInfo || !pInfo->pDataBuf) return;

    auto data = QSharedPointer<WatermarkData>::create();
    char* pBuf = (char*)pInfo->pDataBuf;

    data->globalTime = *(DWORD*)pBuf; pBuf += sizeof(DWORD);
    data->deviceSN = *(DWORD*)pBuf; pBuf += sizeof(DWORD);

    QString mac;
    for (int i = 0; i < 6; ++i) {
        mac += QString("%1").arg((unsigned char)pBuf[i], 2, 16, QChar('0'));
        if (i < 5) mac += ":";
    }
    pBuf += 6;

    data->mac = mac;
    data->deviceType = *(unsigned char*)pBuf++;
    data->deviceInfo = *(unsigned char*)pBuf++;
    data->channelNum = *(unsigned char*)pBuf++;

    QMutexLocker locker(&wrapper->m_watermarkMutex);
    wrapper->m_watermarkData = data;
}

void MediaPlayerWrapper::onFileRefCreated()
{
    emit fileRefCreated();
}

QSharedPointer<WatermarkData> MediaPlayerWrapper::getWatermarkData()
{
    QMutexLocker locker(&m_watermarkMutex);
    return m_watermarkData;
}

void MediaPlayerWrapper::closeFile()
{
    if (!m_bFileOpened) {
        return;
    }

    stop();
    
    NAME(PlayM4_CloseFile)(m_lPort);
    releasePort();
    
    m_bFileOpened = false;
    m_currentFile.clear();
    
    emit statusChanged(Stopped);
}

bool MediaPlayerWrapper::play(HWND displayWnd)
{
    if (!m_bFileOpened) {
        emit errorOccurred("No file opened");
        return false;
    }

    if (displayWnd) {
        m_displayWnd = displayWnd;
    }
    
    if (m_playState == Paused) {
        // Resume from pause
        return resume();
    }
    
    if (!NAME(PlayM4_Play)(m_lPort, m_displayWnd)) {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        emit errorOccurred("Failed to start playback: " + getErrorString(error));
        return false;
    }

    m_playState = Playing;
    emit statusChanged(Playing);
    
    return true;
}

bool MediaPlayerWrapper::pause()
{
    if (m_playState != Playing) {
        return false;
    }

    if (!NAME(PlayM4_Pause)(m_lPort, TRUE)) {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        emit errorOccurred("Failed to pause: " + getErrorString(error));
        return false;
    }

    m_playState = Paused;
    emit statusChanged(Paused);
    
    return true;
}

bool MediaPlayerWrapper::resume()
{
    if (m_playState != Paused) {
        return false;
    }

    if (!NAME(PlayM4_Pause)(m_lPort, FALSE)) {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        emit errorOccurred("Failed to resume: " + getErrorString(error));
        return false;
    }

    m_playState = Playing;
    emit statusChanged(Playing);
    
    return true;
}

bool MediaPlayerWrapper::stop()
{
    if (m_playState == Stopped) {
        return true;
    }

    if (!NAME(PlayM4_Stop)(m_lPort)) {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        emit errorOccurred("Failed to stop: " + getErrorString(error));
        return false;
    }

    m_watermarkData.clear();
    m_playState = Stopped;
    emit statusChanged(Stopped);
    
    return true;
}

bool MediaPlayerWrapper::seek(float fRelativePos)
{
    if (!m_bFileOpened) {
        return false;
    }

    if (!NAME(PlayM4_SetPlayPos)(m_lPort, fRelativePos)) {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        emit errorOccurred("Failed to seek: " + getErrorString(error));
        return false;
    }

    emit positionChanged(fRelativePos);
    return true;
}

bool MediaPlayerWrapper::seekMs(qint64 ms)
{
    if (!m_bFileOpened) {
        return false;
    }

    qint64 totalMs = duration();
    if (totalMs <= 0) {
        return false;
    }

    float relativePos = (float)ms / totalMs;
    if (relativePos < 0.0f) relativePos = 0.0f;
    if (relativePos > 1.0f) relativePos = 1.0f;

    return seek(relativePos);
}

bool MediaPlayerWrapper::stepForward()
{
    if (!m_bFileOpened) {
        return false;
    }

    if(NAME(PlayM4_OneByOne)(m_lPort))
        return true;
    else
    {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        qDebug() << "Sound play error:"<<getErrorString(error);
        return false;
    }
//    return NAME(PlayM4_OneByOne)(m_lPort) == TRUE;
}

bool MediaPlayerWrapper::stepBackward()
{
    if (!m_bFileOpened) {
        return false;
    }

    return NAME(PlayM4_OneByOneBack)(m_lPort) == TRUE;
}

bool MediaPlayerWrapper::stepFrame(int direction)
{
    if (!m_bFileOpened) {
        return false;
    }

    if(m_playState!=Step)
    {
        m_playState = Step;
        emit statusChanged(Step);
    }
    if (direction > 0) {
        return stepForward();
    } else if (direction < 0) {
        return stepBackward();
    }
    
    return false;
}

bool MediaPlayerWrapper::setPlaySpeed(float speed)
{
    if (!m_bFileOpened) {
        return false;
    }

    m_currentSpeed = speed;
    // Note: PlayM4_SetPlaySpeed may not be available in this SDK version
    // Using PlayM4_Fast/Slow instead or implementing custom speed control
    qDebug() << "Set PlaySpeed to normal";
    return true;
}

float MediaPlayerWrapper::getPlaySpeed() const
{
    if (!m_bFileOpened) {
        return 1.0f;
    }

    // Note: PlayM4_GetPlaySpeed may not be available in this SDK version
    return m_currentSpeed;
}

bool MediaPlayerWrapper::setSpeedMultiplier(float multiplier)
{
    if (!m_bFileOpened) {
        return false;
    }
/*
    // Store the speed for reporting
    m_currentSpeed = multiplier;
    
    // Try to use PlayM4 speed functions if available
    if (multiplier > 1.0f) {
        // Faster playback
        if (NAME(PlayM4_Fast)(m_lPort) == TRUE) {
            qDebug() << "Speed increased to" << multiplier;
            return true;
        }
    } else if (multiplier < 1.0f) {
        // Slower playback
        if (NAME(PlayM4_Slow)(m_lPort) == TRUE) {
            qDebug() << "Speed decreased to" << multiplier;
            return true;
        }
    } else {
        // Normal speed - PlayM4_Normal doesn't exist, so we simulate it
        // by stopping and restarting at normal speed
        bool wasPlaying = (m_playState == Playing);
        if (wasPlaying) {
            // Get current position before stopping
            float currentPos = getPlayPos();
            
            // Stop and restart at normal speed
            NAME(PlayM4_Stop)(m_lPort);
            if (NAME(PlayM4_Play)(m_lPort, m_displayWnd) == TRUE) {
                // Restore position
                NAME(PlayM4_SetPlayPos)(m_lPort, currentPos);
                m_playState = Playing;
                qDebug() << "Speed reset to normal";
                return true;
            }
        } else {
            // Not playing, just reset the speed tracker
            qDebug() << "Speed reset to normal (not playing)";
            return true;
        }
    }
    */

    if(multiplier>m_currentSpeed)
    {
        if (NAME(PlayM4_Fast)(m_lPort) == TRUE) {
            qDebug() << "Speed increased to" << multiplier;
            m_currentSpeed = multiplier;
            return true;
        }
    }
    else if(multiplier<m_currentSpeed)
    {
        // Slower playback
        if (NAME(PlayM4_Slow)(m_lPort) == TRUE) {
            qDebug() << "Speed decreased to" << multiplier;
            m_currentSpeed = multiplier;
            return true;
        }
    }
    else
        return true;

    qDebug() << "Speed control failed";
    return false;
}

bool MediaPlayerWrapper::playSound()
{
    if (m_lPort < 0) {
        emit errorOccurred("Invalid port for sound playback");
        return false;
    }

    if (!NAME(PlayM4_PlaySound)(m_lPort)) {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        qDebug() << "Sound play error:"<<getErrorString(error);
//        emit errorOccurred("Failed to play sound: " + getErrorString(error));
        return false;
    }

    qDebug() << "Sound playback started.";
    return true;
}

bool MediaPlayerWrapper::stopSound()
{
    if (!NAME(PlayM4_StopSound)()) {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        qDebug() << "Sound stop error:"<<getErrorString(error);
//        emit errorOccurred("Failed to stop sound: " + getErrorString(error));
        return false;
    }

    qDebug() << "Sound playback stopped.";
    return true;
}

bool MediaPlayerWrapper::setVolume(WORD volume)
{
    if (m_lPort < 0) {
        return false;
    }

    return NAME(PlayM4_SetVolume)(m_lPort, volume) == TRUE;
}

WORD MediaPlayerWrapper::getVolume() const
{
    if (m_lPort < 0) {
        return 0;
    }

    return NAME(PlayM4_GetVolume)(m_lPort);
}

float MediaPlayerWrapper::getPlayPos() const
{
    if (!m_bFileOpened) {
        return 0.0f;
    }

    return NAME(PlayM4_GetPlayPos)(m_lPort);
}

DWORD MediaPlayerWrapper::getFileTime() const
{
    if (!m_bFileOpened) {
        return 0;
    }

    return NAME(PlayM4_GetFileTime)(m_lPort);
}

DWORD MediaPlayerWrapper::getPlayedTime() const
{
    if (!m_bFileOpened) {
        return 0;
    }

    return NAME(PlayM4_GetPlayedTime)(m_lPort);
}

DWORD MediaPlayerWrapper::getCurrentFrameNum() const
{
    if (!m_bFileOpened) {
        return 0;
    }

    return NAME(PlayM4_GetCurrentFrameNum)(m_lPort);
}

DWORD MediaPlayerWrapper::getTotalFrames() const
{
    if (!m_bFileOpened) {
        return 0;
    }

    return NAME(PlayM4_GetFileTotalFrames)(m_lPort);
}

bool MediaPlayerWrapper::getPictureSize(LONG *pWidth, LONG *pHeight) const
{
    if (!m_bFileOpened || !pWidth || !pHeight) {
        return false;
    }

    return NAME(PlayM4_GetPictureSize)(m_lPort, pWidth, pHeight) == TRUE;
}

void MediaPlayerWrapper::setFileRefDoneCallback(FileRefDone callback, void* userData)
{
    if (m_lPort >= 0) {
        if(!NAME(PlayM4_SetFileRefCallBack)(m_lPort, callback, userData)){
            DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
            qDebug() << "Failed to setFileRefDoneCallback:" << getErrorString(error);
        }
    }
}

void MediaPlayerWrapper::setCheckWatermarkCallback(CheckWatermarkCallBackFunc callback, void *userData)
{
    if (m_lPort >= 0) {
        if(!NAME(PlayM4_SetCheckWatermarkCallBack)(m_lPort, callback, userData)){
            DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
            qDebug() << "Failed to setCheckWatermarkCallback:" << getErrorString(error);
        }
    }
}

void MediaPlayerWrapper::setEncChangeCallback(EncChangeCallBackFunc callback, long userData)
{
//    if (m_lPort >= 0) {
//        NAME(PlayM4_SetEncTypeChangeCallBack)(m_lPort, callback, userData);
//    }
}

qint64 MediaPlayerWrapper::position() const
{
    if (!m_bFileOpened) {
        return 0;
    }

    DWORD playedTime = NAME(PlayM4_GetPlayedTime)(m_lPort);
    return static_cast<qint64>(playedTime);
}

qint64 MediaPlayerWrapper::duration() const
{
    if (!m_bFileOpened) {
        return 0;
    }

    DWORD totalTime = NAME(PlayM4_GetFileTime)(m_lPort);
    return static_cast<qint64>(totalTime);
}

bool MediaPlayerWrapper::snapshot(const QString filePath, int nPicFormat)
{
    if (!m_bFileOpened) {
        return false;
    }

    LONG w = 0, h = 0;
    getPictureSize(&w,&h);

    DWORD bufSize = w * h * 5;
    QByteArray imageBuffer(bufSize, 0);
    DWORD imageSize = 0;
    QString snapshotPath = filePath;
    
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyyMMddhhmmsszzz");

    // nType: 0 = BMP format
    if(nPicFormat==Format_BMP)
    {
        snapshotPath += QString("/snapshot_%1.bmp").arg(timestamp);
        if (NAME(PlayM4_GetBMP)(m_lPort, reinterpret_cast<PBYTE>(imageBuffer.data()), bufSize, &imageSize) != TRUE) {
            DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
            qDebug() << "Failed to take snapshot:" << getErrorString(error);
            return false;
        }
    }
    else
    {
        snapshotPath += QString("/snapshot_%1.jpeg").arg(timestamp);
        if (NAME(PlayM4_GetJPEG)(m_lPort, reinterpret_cast<PBYTE>(imageBuffer.data()), bufSize, &imageSize) != TRUE) {
            DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
            qDebug() << "Failed to take snapshot:" << getErrorString(error);
            return false;
        }
    }

    QFile file(snapshotPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(imageBuffer.constData(), imageSize);
        file.close();
        m_lastSnapshotPath = snapshotPath;
        return true;
    } else {
        DWORD error = NAME(PlayM4_GetLastError)(m_lPort);
        qDebug() << "Failed to take snapshot:" << getErrorString(error);
        return false;
    }
}

QString MediaPlayerWrapper::getLastSnapshotPath() const
{
    return m_lastSnapshotPath;
}

QString MediaPlayerWrapper::getErrorString(DWORD errorCode)
{
    switch (errorCode) {
        case PLAYM4_NOERROR: return "No error";
        case PLAYM4_PARA_OVER: return "Invalid parameter";
        case PLAYM4_ORDER_ERROR: return "Function call order error";
        case PLAYM4_TIMER_ERROR: return "Timer error";
        case PLAYM4_DEC_VIDEO_ERROR: return "Video decode error";
        case PLAYM4_DEC_AUDIO_ERROR: return "Audio decode error";
        case PLAYM4_ALLOC_MEMORY_ERROR: return "Memory allocation error";
        case PLAYM4_OPEN_FILE_ERROR: return "Open file error";
        case PLAYM4_CREATE_OBJ_ERROR: return "Create object error";
        case PLAYM4_BUF_OVER: return "Buffer overflow";
        case PLAYM4_SUPPORT_FILE_ONLY: return "Function only supports file playback";
        case PLAYM4_SUPPORT_STREAM_ONLY: return "Function only supports stream playback";
        case PLAYM4_SYS_NOT_SUPPORT: return "System not supported";
        case PLAYM4_FILEHEADER_UNKNOWN: return "Unknown file header";
        case PLAYM4_VERSION_INCORRECT: return "Version mismatch";
        case PLAYM4_INVALID_PORT: return "Invalid port";
        default: return QString("Unknown error: %1").arg(errorCode);
    }
}
