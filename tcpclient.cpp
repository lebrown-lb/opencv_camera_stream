#include "tcpclient.h"

#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>

#include <QMutex>
#include <QThread>

extern QMutex  frame_mutex;
extern cv::Mat frame;

extern QMutex clientOnFlag_mutex;
extern bool clientOnFlag;

extern QMutex clientCtrlFlag_mutex;
extern bool clientCtrlFlag;


TcpClient::TcpClient(QObject *parent)
    : QObject{parent}
{}

void TcpClient::runClient()
{
    std::cout << "IP ADDRESS: " << m_ipAddress << std::endl;
    std::cout << "PORT: " << m_port << std::endl;

    int sock = 0;
    struct sockaddr_in serv_addr;
    unsigned char buffer[1024] = {0};
    // Creating socket file descriptor
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cout << "Socket creation error" << std::endl;
        emit clientClosed();
        return;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(m_port);
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, m_ipAddress.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cout << "Invalid address/ Address not supported" << std::endl;
        emit clientClosed();
        return;
    }
    // Connect to the server
    if (::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cout << "Connection Failed" << std::endl;
        emit clientClosed();
        return;
    }

    int rows, cols, type;
    size_t step;
    ssize_t valread;

    std::string ack = "ACK!";
    std::string fin = "FIN!";
    std::string pause = "PAUSE";
    std::string play = "PLAY";

    while(true)
    {
        clientOnFlag_mutex.lock();
        if(!clientOnFlag)
        {
            clientOnFlag_mutex.unlock();
            break;
        }
        clientOnFlag_mutex.unlock();


        valread = read(sock, buffer, 1024);
        std::cout << "Received: " << buffer << std::endl;

        if((buffer[0] == 'H') && (buffer[1] == 'D') && (buffer[2] == 'R') && (buffer[3] == ':') && (valread == 20) )
            break;
        else
            memset(buffer, 0, sizeof(buffer));

    }

    cols = static_cast<int>((buffer[4] << 24) | (buffer[5] << 16) | (buffer[6] << 8) | buffer[7]);
    rows = static_cast<int>((buffer[8] << 24) | (buffer[9] << 16) | (buffer[10] << 8) |  buffer[11]);
    step = static_cast<int>((buffer[12] << 24) | (buffer[13] << 16) | (buffer[14] << 8) | buffer[15]);
    type = static_cast<int>((buffer[16] << 24) | (buffer[17] << 16) | (buffer[18] << 8) | buffer[19]);


    std::cout << "cols: " << std::hex << cols << std::endl;
    std::cout << "rows: " << std::hex << rows << std::endl;
    std::cout << "step: " << std::hex << step << std::endl;
    std::cout << "type: " << std::hex << type << std::endl;

    size_t frameSize = (cols * rows * 3);

    std::cout << "s: " << std::dec << frameSize << std::endl;

    while(true)
    {

        clientOnFlag_mutex.lock();
        if(!clientOnFlag)
        {
            clientOnFlag_mutex.unlock();
            break;
        }
        clientOnFlag_mutex.unlock();


        send(sock, ack.c_str(), ack.size(), 0);

        valread = read(sock, buffer, 1024);

        if((buffer[0] == 'A') && (buffer[1] == 'C') && (buffer[2] == 'K') && (buffer[3] == '!'))
            break;

    }


    if(m_data != NULL)
        delete[] m_data;
    m_data = new unsigned char[frameSize];
    bool local_flg = false;
    bool stream_flg = true;

    while(true)
    {
        if(stream_flg)
        {
            unsigned char* ptr = m_data;
            size_t count = 0;
            while(count < frameSize)
            {
                valread = read(sock, ptr, 1500 );

                ptr += valread;
                count += valread;

            }

            frame_mutex.lock();
            frame = cv::Mat(rows,cols,type,m_data,step);
            frame_mutex.unlock();
            emit updateFrame();

            clientOnFlag_mutex.lock();
            clientCtrlFlag_mutex.lock();
            if(!clientCtrlFlag && clientOnFlag)
                send(sock, ack.c_str(), ack.size(), 0);
            clientCtrlFlag_mutex.unlock();
            clientOnFlag_mutex.unlock();
        }

        clientCtrlFlag_mutex.lock();
        if(clientCtrlFlag)
        {

            if(stream_flg)
            {
                stream_flg = false;
                send(sock, pause.c_str(), pause.size(), 0);

            }
            else
            {
                stream_flg = true;
                send(sock, play.c_str(), play.size(), 0);
            }
            emit ctrlMessageSent(stream_flg);
            clientCtrlFlag = false;
        }
        clientCtrlFlag_mutex.unlock();

        clientOnFlag_mutex.lock();
        if(!clientOnFlag)
        {
            send(sock, fin.c_str(), fin.size(), 0);
            local_flg = true;
        }
        clientOnFlag_mutex.unlock();

        if(local_flg)
            break;


    }

    close(sock);


    emit clientClosed();


}
