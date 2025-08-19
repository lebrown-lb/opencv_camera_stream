#include "tcpclient.h"

#include <QMutex>
#include <iostream>

extern QMutex  frame_mutex;
extern cv::Mat frame;

extern QMutex clientOnFlag_mutex;
extern bool clientOnFlag;

TcpClient::TcpClient(QObject *parent)
    : QObject{parent}
{}

void TcpClient::runClient()
{
    std::cout << "IP ADDRESS: " << m_ipAddress << std::endl;
    std::cout << "PORT: " << m_port << std::endl;

    emit clientClosed();


}
