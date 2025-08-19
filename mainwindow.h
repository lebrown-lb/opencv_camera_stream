#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "tcpserver.h"

#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

public slots:
    void updateStream();
    void handleServerStop();
    void modeChangeHandler(int index);

signals:
    void startServer();

private:
    std::string getNetworkAddress();
    void streamCtrlFunction();
    void serverCtrlFunction();
    void frameCapture();
    QImage matToQImage(const cv::Mat& src);
    void printMatRow(const cv::Mat& src,size_t row);
    Ui::MainWindow *ui;
    cv::VideoCapture m_cap;
    bool m_streamFlg = false;
    QTimer * m_timer = NULL;
    unsigned int m_prd = 1000/30;
    uint8_t m_mode;
    QThread * m_serverThread = NULL;
    TcpServer * m_server = NULL;


};
#endif // MAINWINDOW_H
