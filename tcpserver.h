#ifndef TCPSERVER_H
#define TCPSERVER_H

#include <QObject>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

enum SOCKET_STATUS
{
    CLIENT_CONNECTED,
    CLOSE_SIGNAL_RECEIVED,
    UNINITALIZED,
    SELECT_ERROR,
    ACCEPT_ERROR
};

class TcpServer : public QObject
{
    Q_OBJECT
public:
    explicit TcpServer(QObject *parent = nullptr);

public slots:
    void runServer();

signals:
    void serverClosed();

private:
    void buildMatHeader(cv::Mat & src, uint8_t* data);
    void printSocketStatus(SOCKET_STATUS s);
    unsigned int m_port = 8080;
};

#endif // TCPSERVER_H
