#include "tcpserver.h"

#include <QMutex>

#include <iostream>
#include <string>
#include <memory>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>


extern QMutex  frame_mutex;
extern cv::Mat frame;

extern QMutex serverOnFlag_mutex;
extern bool serverOnFlag;

extern QMutex newDataFlag_mutex;
extern bool newDataFlg;

TcpServer::TcpServer(QObject *parent)
    : QObject{parent}
{}

void TcpServer::runServer()
{
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    socklen_t addrlen = sizeof(address);
    char buffer[1024] = {0};
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        std::cout << "socket failed" << std::endl;
        emit serverClosed();
        return;
    }
    // Forcefully attaching socket to the port 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        std::cout << "setsockopt error" << std::endl;
        emit serverClosed();
        return;
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(m_port);
    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cout << "bind failed" << std::endl;
        emit serverClosed();
        return;
    }
    // Start listening for incoming connections
    if (listen(server_fd, 3) < 0) {
        std::cout << "listen error" << std::endl;
        emit serverClosed();
        return;
    }
    std::cout << "Server listening on port " << m_port << std::endl;
    // Accept incoming connection

    fd_set read_fds;
    struct timeval timeout;
    SOCKET_STATUS status = UNINITALIZED;
    new_socket = -1;

    while (true) {
        FD_ZERO(&read_fds);
        FD_SET(server_fd, &read_fds);

        timeout.tv_sec = 1; // 5-second timeout
        timeout.tv_usec = 0;

        int ret = select(server_fd + 1, &read_fds, NULL, NULL, &timeout);

        if (ret == -1)
        {
            status = SELECT_ERROR;
            break;
        }
        else if (ret == 0)
        {
            // Timeout occurred, no new connection
            // You can continue looping, or take other action
            serverOnFlag_mutex.lock();
            if(!serverOnFlag)
                status = CLOSE_SIGNAL_RECEIVED;
            serverOnFlag_mutex.unlock();

            if(status == CLOSE_SIGNAL_RECEIVED)
                break;
            continue;
        }
        else
        {
            // Connection ready
            if (FD_ISSET(server_fd, &read_fds))
            {
                new_socket = accept(server_fd, (struct sockaddr*)&address, &addrlen);
                if (new_socket == -1)
                {
                    status = ACCEPT_ERROR;
                    break;

                }
                else
                {
                    // Process client_socket_fd
                    status = CLIENT_CONNECTED;
                    break;
                }
            }
        }
    }

    if(status == CLIENT_CONNECTED)
    {
        bool local_newDataFlg = false;
        bool hdr_flg = false;
        bool clientStreamFlag = true;
        std::string rsp;
        std::string ack = "ACK!";

        while(true)
        {
            serverOnFlag_mutex.lock();
            if(!serverOnFlag)
            {
                serverOnFlag_mutex.unlock();
                break;
            }
            serverOnFlag_mutex.unlock();


            newDataFlag_mutex.lock();
            if(newDataFlg)
                local_newDataFlg = true;
            else
                local_newDataFlg = false;
            newDataFlag_mutex.unlock();

            if(local_newDataFlg)
            {

                if(!hdr_flg)
                {
                    uint8_t data[20];
                    frame_mutex.lock();
                    buildMatHeader(frame, data);
                    frame_mutex.unlock();
                    send(new_socket, data, 20, 0);

                    ssize_t valread = read(new_socket, buffer, 1024);
                    buffer[valread] = '\0';

                    rsp = std::string(buffer);
                    if (rsp == "ACK!")
                    {
                        hdr_flg = true;
                        send(new_socket, ack.c_str(), ack.size(), 0);
                        buffer[0] = '\0';
                        buffer[1] = '\0';
                        rsp = "";
                    }
                }

                if(hdr_flg)
                {

                    if(clientStreamFlag)
                    {
                        frame_mutex.lock();
                        bool dataSent = false;
                        u_char * ptr = frame.data;
                        size_t dataToSend;
                        while(!dataSent)
                        {
                            dataToSend = (frame.dataend - ptr);
                            //std::cout << "dataToSend:" << dataToSend << std::endl;
                            if(dataToSend > 1500)
                                dataToSend = 1500;
                            else
                                dataSent = true;

                            send(new_socket, ptr, dataToSend, 0);
                            ptr += dataToSend;
                        }
                        frame_mutex.unlock();

                    }

                    while (true)
                    {
                        ssize_t valread = read(new_socket, buffer, 1024);
                        buffer[valread] = '\0';
                        rsp = std::string(buffer);
                        if(rsp != "" && rsp != "ACK!")
                            std::cout << "rsp:" << rsp << std::endl;

                        if(rsp == "ACK!")
                        {
                            buffer[0] = '\0';
                            buffer[1] = '\0';
                            rsp = "";
                            break;
                        }
                        else if(rsp == "FIN!")
                        {
                            serverOnFlag_mutex.lock();
                            serverOnFlag = false;
                            serverOnFlag_mutex.unlock();
                            break;
                        }
                        else if(rsp == "PAUSE")
                        {
                            clientStreamFlag = false;
                            buffer[0] = '\0';
                            rsp = "";
                            break;
                        }
                        else if(rsp == "PLAY")
                        {
                            clientStreamFlag = true;
                            buffer[0] = '\0';
                            rsp = "";
                            break;
                        }

                    }



                }

                local_newDataFlg = false;
                newDataFlag_mutex.lock();
                newDataFlg = false;
                newDataFlag_mutex.unlock();
            }
        }

        newDataFlag_mutex.lock();
        newDataFlg = false;
        newDataFlag_mutex.unlock();
    }
    else
        printSocketStatus(status);

    close(new_socket);
    close(server_fd);

    emit serverClosed();

}

void TcpServer::buildMatHeader(cv::Mat &src, uint8_t* data)
{

    int cols = src.cols;
    int rows = src.rows;
    size_t step = src.step[0];
    int type = src.type();

    data[0] = 'H';
    data[1] = 'D';
    data[2] = 'R';
    data[3] = ':';

    // std::cout << "cols: " << std::hex << cols << std::endl;
    // std::cout << "rows: " << std::hex << rows << std::endl;
    // std::cout << "step: " << std::hex << step << std::endl;
    // std::cout << "type: " << std::hex << type << std::endl;

    //columns little endian
    data[4] = (static_cast<uint8_t>(cols >> 24));
    data[5] = (static_cast<uint8_t>(cols >> 16));
    data[6] = (static_cast<uint8_t>(cols >> 8));
    data[7] = (static_cast<uint8_t>(cols));

    //rows little endian
    data[8] = (static_cast<uint8_t>(rows >> 24));
    data[9] = (static_cast<uint8_t>(rows >> 16));
    data[10] = (static_cast<uint8_t>(rows >> 8));
    data[11] = (static_cast<uint8_t>(rows));

    //step little endian
    data[12] = (static_cast<uint8_t>(step >> 24));
    data[13] = (static_cast<uint8_t>(step >> 16));
    data[14] = (static_cast<uint8_t>(step >> 8));
    data[15] = (static_cast<uint8_t>(step));

    //type little endian
    data[16] = (static_cast<uint8_t>(type >> 24));
    data[17] = (static_cast<uint8_t>(type >> 16));
    data[18] = (static_cast<uint8_t>(type >> 8));
    data[19] = (static_cast<uint8_t>(type));

}

void TcpServer::printSocketStatus(SOCKET_STATUS s)
{
    switch (s) {
    case CLIENT_CONNECTED:
        std::cout << "CLIENT_CONNECTED" << std::endl;
        break;
    case CLOSE_SIGNAL_RECEIVED:
        std::cout << "CLOSE_SIGNAL_RECEIVED" << std::endl;
        break;
    case UNINITALIZED:
        std::cout << "UNINITALIZED" << std::endl;
        break;
    case SELECT_ERROR:
        std::cout << "SELECT_ERROR" << std::endl;
        break;
    case ACCEPT_ERROR:
        std::cout << "ACCEPT_ERROR" << std::endl;
        break;

    default:
        std::cout << "UNKNOWN" << std::endl;
        break;
    }

}
