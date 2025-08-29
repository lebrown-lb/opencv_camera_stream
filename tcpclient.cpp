#include "tcpclient.h"

#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <chrono>

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

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        std::cout << "FCNTL ERROR 1" << std::endl;
        emit clientClosed();
        return;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cout << "FCNTL ERROR 2" << std::endl;
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

    int connect_status = ::connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr));
    if (connect_status < 0 && errno != EINPROGRESS)
    {
        std::cout << "Connection Failed" << std::endl;
        emit clientClosed();
        return;
    }


    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);

    struct timeval tv;
    tv.tv_sec = 10; // Timeout after 10 seconds
    tv.tv_usec = 0;

    int sel_ret = select(sock + 1, NULL, &write_fds, NULL, &tv);
    if (sel_ret < 0)
    {
        std::cout << "select Failed" << std::endl;
        emit clientClosed();
        return;
    }
    else if (sel_ret == 0)
    {
        std::cout << "CONNECTION TIMEOUT" << std::endl;
        emit clientClosed();
        return;
    }
    else
    {
        // Check if the socket is writable (connection established)
        if (FD_ISSET(sock, &write_fds))
        {
            int error = 0;
            socklen_t len = sizeof(error);
            if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) < 0)
            {
                std::cout << "GETSOCKOPT ERROR" << std::endl;
                emit clientClosed();
                return;
            }
            if (error == 0)
            {
                std::cout << "CONNECTION ESTABLISHED" << std::endl;
            }
            else
            {
                std::cout << "CONNECTION FAILED" << std::endl;
                emit clientClosed();
                return;
            }
        }
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

        if(valread > 0)
        {
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

    std::cout << "frameSize: " << std::dec << frameSize << std::endl;

    send(sock, ack.c_str(), ack.size(), 0);

    // size_t count = 10;

    // while(true)
    // {

    //     clientOnFlag_mutex.lock();
    //     if(!clientOnFlag)
    //     {
    //         clientOnFlag_mutex.unlock();
    //         break;
    //     }
    //     clientOnFlag_mutex.unlock();

    //     if(count)
    //         send(sock, ack.c_str(), ack.size(), 0);


    //     valread = read(sock, buffer, 1024);

    //     if(count && valread)
    //     {
    //         std::cout << "buffer:" << buffer << std::endl;
    //         count--;
    //     }
    //     rsp = std::string((char*)buffer);

    //     if(substringCheck(rsp,ack,&idx))
    //         break;

    // }


    if(m_data != NULL)
        delete[] m_data;
    m_data = new unsigned char[frameSize];
    bool local_flg = false;
    bool stream_flg = true;
    bool validFrame;

    std::cout << "STREAM DATA" << std::endl;

    while(true)
    {
        if(stream_flg)
        {

            unsigned char* ptr = m_data;
            size_t count = 0;
            long time = 0;

            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

            validFrame = true;
            while(count < frameSize)
            {
                valread = read(sock, ptr, 1500 );

                if((ptr + valread) > (m_data + frameSize))
                {
                    //invalid frame
                    validFrame = false;
                    break;
                }
                if(valread > 0)
                {
                    ptr += valread;
                    count += valread;
                }



                end = std::chrono::steady_clock::now();
                time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

                //std::cout << "[" << time << "]count:" << count << std::endl;

                if(time > 1000000)
                {
                    validFrame = false;
                    std::cout << "TIMEOUT" << std::endl;
                    break;
                }
            }


            if(validFrame)
            {

                frame_mutex.lock();
                frame = cv::Mat(rows,cols,type,m_data,step);
                frame_mutex.unlock();
                emit updateFrame();
            }
            else
                std::cout << "INVALID FRAME" << std::endl;

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
