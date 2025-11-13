#include "udpserver.h"

#include <QMutex>
#include <iostream>



extern QMutex  frame_mutex;
extern cv::Mat frame;

extern QMutex serverOnFlag_mutex;
extern bool serverOnFlag;

extern QMutex newDataFlag_mutex;
extern bool newDataFlg;

UdpServer::UdpServer(QObject *parent)
    : QObject{parent}
{}

void UdpServer::runServer()
{
    int server_fd;
    struct sockaddr_in s_address, c_address;
    int opt = 1;

    char buffer[1024] = {0};
    // Creating socket file descriptor
    if ((server_fd = socket(AF_INET, SOCK_DGRAM, 0)) == 0) {
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
    struct timeval read_timeout;
    read_timeout.tv_sec = 0;
    read_timeout.tv_usec = 50;
    setsockopt(server_fd, SOL_SOCKET, SO_RCVTIMEO, &read_timeout, sizeof read_timeout);


    s_address.sin_family = AF_INET;
    s_address.sin_addr.s_addr = INADDR_ANY;
    s_address.sin_port = htons(m_port);
    // Bind the socket to the network address and port
    if (bind(server_fd, (struct sockaddr*)&s_address, sizeof(s_address)) < 0) {
        std::cout << "bind failed" << std::endl;
        emit serverClosed();
        return;
    }

    std::cout << "Server listening on port " << m_port << std::endl;
    // Accept incoming connection


    SOCKET_STATUS status = UNINITALIZED;

    socklen_t len = sizeof(c_address);
    int n = 0;
    std::string rsp;


    while (true)
    {
        n = recvfrom(server_fd, (char *)buffer, 1023, 0, ( struct sockaddr *) &c_address, &len);
        if (n > 0)
        {
            buffer[n] = '\0';
            rsp = std::string(buffer);
            if (rsp == "CONNECT!")
            {
                status = CLIENT_CONNECTED;
                break;
            }
        }

        serverOnFlag_mutex.lock();
        if(!serverOnFlag)
            status = CLOSE_SIGNAL_RECEIVED;
        serverOnFlag_mutex.unlock();

        if(status == CLOSE_SIGNAL_RECEIVED)
            break;

    }

    if(status == CLIENT_CONNECTED)
    {
        bool local_newDataFlg = false;
        bool hdr_flg = false;
        bool clientStreamFlag = true;
        size_t idx;
        std::string ack = "ACK!";
        std::string fin = "FIN!";
        std::string pause = "PAUSE";
        std::string play = "PLAY";

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
                    sendto(server_fd, data, 20, 0, (const struct sockaddr *) &c_address, len);

                    rsp = clientRead(server_fd, buffer, c_address);
                    if (rsp == "ACK!")
                    {
                        hdr_flg = true;
                        //sendto(server_fd, ack.c_str(), ack.size(), 0, (const struct sockaddr *) &c_address, len);
                        buffer[0] = '\0';
                        buffer[1] = '\0';
                        rsp = "";
                    }
                }

                if(hdr_flg)
                {

                    if(clientStreamFlag)
                    {

                        std::cout << "[SEND FRAME]" << std::endl;
                        // frame_mutex.lock();
                        // bool dataSent = false;
                        // u_char * ptr = frame.data;
                        // size_t dataToSend;
                        // while(!dataSent)
                        // {
                        //     dataToSend = (frame.dataend - ptr);
                        //     //std::cout << "dataToSend:" << dataToSend << std::endl;
                        //     if(dataToSend > 1500)
                        //         dataToSend = 1500;
                        //     else
                        //         dataSent = true;

                        //     send(new_socket, ptr, dataToSend, 0);
                        //     ptr += dataToSend;
                        // }
                        // frame_mutex.unlock();

                    }

                    while (true)
                    {
                        serverOnFlag_mutex.lock();
                        if(!serverOnFlag)
                        {
                            serverOnFlag_mutex.unlock();
                            break;
                        }
                        serverOnFlag_mutex.unlock();


                        rsp = clientRead(server_fd, buffer, c_address);
                        if(rsp != "" && rsp != "ACK!")
                            std::cout << "rsp:" << rsp << std::endl;

                        if(substringCheck(rsp,fin,&idx))
                        {
                            serverOnFlag_mutex.lock();
                            serverOnFlag = false;
                            serverOnFlag_mutex.unlock();
                            break;
                        }
                        else if(substringCheck(rsp,pause,&idx))
                        {
                            std::cout << "PAUSE" << std::endl;
                            clientStreamFlag = false;
                            rsp = "";
                            break;
                        }
                        else if(substringCheck(rsp,play,&idx))
                        {
                            std::cout << "PLAY" << std::endl;
                            clientStreamFlag = true;
                            rsp = "";
                            break;
                        }
                        else if(substringCheck(rsp,ack,&idx))
                        {
                            rsp = "";
                            break;
                        }
                        memset(buffer, 0, 1024);

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

    close(server_fd);
    emit serverClosed();

}

std::string UdpServer::clientRead(int sock_fd,char *buffer, sockaddr_in c_address)
{
    std::string rsp = "";
    int valread = 0;
    sockaddr_in tmp_address;
    socklen_t len = sizeof(tmp_address);
    valread = recvfrom(sock_fd, (char *)buffer, 1023, 0, ( struct sockaddr *) &tmp_address, &len);
    if ((valread > 0) && (compare_sockaddr_in(c_address,tmp_address)))
    {
        buffer[valread] = '\0';
        rsp = std::string(buffer);
    }
    else
        buffer[0] = '\0';

    return rsp;
}

bool UdpServer::compare_sockaddr_in(const sockaddr_in &sa1, const sockaddr_in &sa2)
{
    return (sa1.sin_family == sa2.sin_family &&
            sa1.sin_port == sa2.sin_port &&
            sa1.sin_addr.s_addr == sa2.sin_addr.s_addr);
}

void UdpServer::removeUnseenCharacters(std::string &s)
{
    // Remove characters that are not printable (e.g., control characters, non-ASCII)
    s.erase(std::remove_if(s.begin(), s.end(), [](unsigned char c) {
                return !std::isprint(c); // Keep only printable characters
            }), s.end());

}

bool UdpServer::substringCheck(std::string& a, std::string& b, size_t *idx)
{
    size_t pos;

    removeUnseenCharacters(a);
    removeUnseenCharacters(b);

    //std::cout << "[" << a << "=" << b << "]:" << (a == b) << std::endl;
    if(a == b)
        return true;

    pos = a.find(b);
    *idx = pos;
    if(pos != std::string::npos)
        return true;
    else
        return false;

}

void UdpServer::buildMatHeader(cv::Mat &src, uint8_t* data)
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

void UdpServer::printSocketStatus(SOCKET_STATUS s)
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
