#ifndef MEDIAPLAYERWRAPPER_H
#define MEDIAPLAYERWRAPPER_H

#include <QObject>
#include <QString>
#include <QTimer>
#include <QPointer>
#include <QMutex>
#include <Windows.h>

// Include PlayM4 SDK headers
extern "C" {
#include "SDK/WindowsPlayM4.h"
}

// PlayM4 SDK configuration
#ifdef _FOR_HIKPLAYM4_DLL_
    #define NAME(x) Hik_##x
#else
    #define NAME(x) x
#endif

struct WatermarkData{
    DWORD globalTime;
    DWORD deviceSN;
    QString mac;
    unsigned char deviceType;
    unsigned char deviceInfo;
    unsigned char channelNum;
};

// Forward declarations - these are now defined in WindowsPlayM4.h
typedef void (CALLBACK* FileRefDone)(DWORD nPort, void* nUser);
typedef void (CALLBACK* DecCBFun)(long nPort, char* pBuf, long nSize, FRAME_INFO* pFrameInfo, DWORD_PTR nUser, DWORD_PTR nReserved2);
typedef void (CALLBACK* DisplayCBFun)(long nPort, char* pBuf, long nSize, long nWidth, long nHeight, long nStamp, long nType, DWORD_PTR nReserved);
typedef void (CALLBACK* VerifyCallBackFun)(long nPort, FRAME_POS* pFilePos, DWORD bIsVideo, DWORD_PTR nUser);
// Fixed callback signatures to match SDK
typedef void (CALLBACK* CheckWatermarkCallBackFunc)(long nPort, WATERMARK_INFO* pWatermarkInfo, void* nUser);
typedef void (CALLBACK* EncChangeCallBackFunc)(long nPort, long nUser);

class MediaPlayerWrapper : public QObject
{
    Q_OBJECT

public:
    enum PlayState {
        Stopped = 0,
        Playing = 1,
        Paused = 2,
        Step = 3
    };

    enum PicFormat{
        Format_BMP=0,
        Format_JPEG=1
    };

    explicit MediaPlayerWrapper(QObject *parent = nullptr);
    ~MediaPlayerWrapper();

    // Initialize SDK
    bool initialize();
    void cleanup();

    // File operations
    bool openFile(const QString &filePath);
    void closeFile();

    // Playback control
    bool play(HWND displayWnd = nullptr);
    bool pause();
    bool resume();
    bool stop();
    bool seek(float fRelativePos);
    bool seekMs(qint64 ms);
    bool stepForward();
    bool stepBackward();
    bool stepFrame(int direction); // +1 for forward, -1 for backward

    // Speed control
    bool setPlaySpeed(float speed);
    float getPlaySpeed() const;
    bool setSpeedMultiplier(float multiplier); // 0.75, 1.0, 1.25, etc.

    // Volume control
    bool playSound();
    bool stopSound();
    bool setVolume(WORD volume);
    WORD getVolume() const;

    // Get information
    bool isFileOpened() const { return m_bFileOpened; }
    bool isPlaying() const { return m_playState == Playing; }
    bool isPaused() const { return m_playState == Paused; }
    bool isStop() const { return m_playState == Stopped; }
    bool isStep() const { return m_playState == Step; }

    // Position and time
    float getPlayPos() const;
    DWORD getFileTime() const;
    DWORD getPlayedTime() const;
    
    // Position in milliseconds
    qint64 position() const;
    qint64 duration() const;
    
    // Frame information
    DWORD getCurrentFrameNum() const;
    DWORD getTotalFrames() const;
    
    // Snapshot functionality
    bool snapshot(const QString filePath, int nPicFormat);
    QString getLastSnapshotPath() const;
    
    // Picture size
    bool getPictureSize(LONG *pWidth, LONG *pHeight) const;

    // Callbacks setup
    void setFileRefDoneCallback(FileRefDone callback, void* userData);
    void setCheckWatermarkCallback(CheckWatermarkCallBackFunc callback, void* userData);
    void setEncChangeCallback(EncChangeCallBackFunc callback, long userData);

    static void CALLBACK fileRefCallBack(DWORD nPort, void* nUser);
    static void CALLBACK watermarkCallBack(long nPort, WATERMARK_INFO* pInfo, void *nUser);
    QMutex m_watermarkMutex;
    QSharedPointer<WatermarkData> m_watermarkData;

    QSharedPointer<WatermarkData> getWatermarkData();


signals:
    void statusChanged(int state);
    void fileEnded();
    void errorOccurred(const QString &error);
    void positionChanged(float position);
    void fileRefCreated();

public slots:
    void onFileRefCreated();

private:
    LONG m_lPort;
    PlayState m_playState;
    bool m_bInitialized;
    bool m_bFileOpened;
    QString m_currentFile;
    HWND m_displayWnd;
    float m_currentSpeed;
    QString m_lastSnapshotPath;

    // Helper functions
    bool getPort();
    void releasePort();
    QString getErrorString(DWORD errorCode);
};

#endif // MEDIAPLAYERWRAPPER_H
