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
    size_t step, idx;
    ssize_t valread;

    std::string rsp;
    std::string ack = "ACK!";
    std::string fin = "FIN!";
    std::string pause = "PAUSE";
    std::string play = "PLAY";
    std::string hdr = "HDR:";

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

        rsp = std::string((char*)buffer);



        if(substringCheck(rsp,hdr,&idx))
        {
            rsp = "";
            break;
        }
        else
        {
            memset(buffer, 0, sizeof(buffer));
            rsp = "";
        }
    }


    if((idx + 20) > 1023)
    {
        std::cout << "HDR FRAME CLIPPED" << std::endl;
        emit clientClosed();
        return;
    }

    cols = static_cast<int>((buffer[idx + 4] << 24) | (buffer[idx + 5] << 16) | (buffer[idx + 6] << 8) | buffer[7]);
    rows = static_cast<int>((buffer[idx + 8] << 24) | (buffer[idx + 9] << 16) | (buffer[idx + 10] << 8) |  buffer[idx + 11]);
    step = static_cast<int>((buffer[idx + 12] << 24) | (buffer[idx + 13] << 16) | (buffer[idx + 14] << 8) | buffer[idx + 15]);
    type = static_cast<int>((buffer[idx + 16] << 24) | (buffer[idx + 17] << 16) | (buffer[idx + 18] << 8) | buffer[idx + 19]);


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

        rsp = std::string((char*)buffer);

        if(substringCheck(rsp,ack,&idx))
            break;

    }


    if(m_data != NULL)
        delete[] m_data;
    m_data = new unsigned char[frameSize];
    bool local_flg = false;
    bool stream_flg = true;

    std::cout << "STREAM DATA" << std::endl;

    while(true)
    {
        if(stream_flg)
        {
            unsigned char* ptr = m_data;
            size_t count = 0;

            size_t zeroDataCount = 0;

            while((count < frameSize) && (zeroDataCount < 100))
            {
                valread = read(sock, ptr, 1500 );

                if(valread)
                    zeroDataCount = 0;
                else
                    zeroDataCount++;

                ptr += valread;
                count += valread;

            }


            if(zeroDataCount < 100)
            {

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
            else
                std::cout << "ZERO DATA COUNT" << std::endl;
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

bool TcpClient::substringCheck(std::string a, std::string b, size_t *idx)
{
    size_t pos;

    pos = a.find(b);
    *idx = pos;
    if(pos != std::string::npos)
        return true;
    else
        return false;
}
