#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include "tcpserver.h"
#include "tcpclient.h"

#include <string>
#include <vector>

#include <QMainWindow>
#include <QThread>
#include <QTimer>
#include <QCloseEvent>
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
    void displayFrame();
    void handleServerStop();
    void handleClientStop();
    void ctrlMessageSentHandler(bool x);
    void modeChangeHandler(int index);

signals:
    void startServer();
    void startClient();

private slots:
    void targetAddressChange();
    void targetPortChange();
    void streamCtrlFunction();
    void serverCtrlFunction();
    void ctrlFunction();
    void clientCtrlFunction();

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    std::string getNetworkAddress();
    bool validateAddressFormat(std::string s);
    void frameCapture();
    QImage matToQImage(const cv::Mat& src,uint8_t fmt);
    void printMatRow(const cv::Mat& src,size_t row);
    bool numeric(std::string str);
    std::vector<std::string> splitString(const std::string& str, char delimiter);
    Ui::MainWindow *ui;
    cv::VideoCapture m_cap;
    bool m_streamFlg = false;
    bool m_exitFlag = false;
    QTimer * m_timer = NULL;
    unsigned int m_prd = 1000/30;
    uint8_t m_mode;
    QThread * m_serverThread = NULL;
    TcpServer * m_server = NULL;
    QThread * m_clientThread = NULL;
    TcpClient * m_client = NULL;


};
#endif // MAINWINDOW_H
