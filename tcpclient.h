#ifndef TCPCLIENT_H
#define TCPCLIENT_H

#include <QObject>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>

class TcpClient : public QObject
{
    Q_OBJECT
public:
    explicit TcpClient(QObject *parent = nullptr);
    void setPort(unsigned int x)
    {
        m_port = x;
    }
    void setNetworkAddress(std::string str)
    {
        m_ipAddress = str;
    }


public slots:
    void runClient();

signals:
    void clientClosed();

private:
    std::string m_ipAddress;
    unsigned int m_port;
};

#endif // TCPCLIENT_H
