#include "udpclient.h"

#include <iostream>
#include <chrono>

#include <QMutex>
#include <QThread>

#define MAXTXLEN        1425
#define NETWORK_MTU     1400

extern QMutex  frame_mutex;
extern cv::Mat frame;

extern QMutex clientOnFlag_mutex;
extern bool clientOnFlag;

extern QMutex clientCtrlFlag_mutex;
extern bool clientCtrlFlag;


UdpClient::UdpClient(QObject *parent)
    : QObject{parent}
{}

void UdpClient::runClient()
{
    std::cout << "IP ADDRESS: " << m_ipAddress << std::endl;
    std::cout << "PORT: " << m_port << std::endl;

    int sock = 0;
    struct sockaddr_in serv_addr;
    unsigned char buffer[MAXTXLEN] = {0};
    // Creating socket file descriptor
    if ((sock = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        std::cout << "Socket creation error" << std::endl;
        emit clientClosed();
        return;
    }

    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) {
        std::cout << "FCNTL ERROR 1" << std::endl;
        close(sock);
        emit clientClosed();
        return;
    }
    if (fcntl(sock, F_SETFL, flags | O_NONBLOCK) == -1) {
        std::cout << "FCNTL ERROR 2" << std::endl;
        close(sock);
        emit clientClosed();
        return;
    }


    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(m_port);
    // Convert IPv4 and IPv6 addresses from text to binary form
    if (inet_pton(AF_INET, m_ipAddress.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cout << "Invalid address/ Address not supported" << std::endl;
        close(sock);
        emit clientClosed();
        return;
    }
    // Connect to the server
    std::string rsp;
    std::string con = "CONNECT!";
    socklen_t len = sizeof(serv_addr);
    sendto(sock,con.c_str(),con.size(),0,( struct sockaddr *)&serv_addr,len);
    auto start = std::chrono::high_resolution_clock::now();

    int rows, cols, type;
    size_t step, idx;
    ssize_t valread;
    sockaddr_in tmp_address;


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


        valread = recvfrom(sock, (char *)buffer, MAXTXLEN-1, 0, ( struct sockaddr *) &tmp_address, &len);

        auto end0 = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> duration = end0 - start;

        if(duration.count() > 4)
        {
            std::cout << "Connection Timeout" << std::endl;
            close(sock);
            emit clientClosed();
            return;
        }

        if ((valread > 0) && (compare_sockaddr_in(serv_addr,tmp_address)))
        {
            buffer[valread] = '\0';
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
        else
            buffer[0] = '\0';
    }


    if((idx + 20) > 1023)
    {
        std::cout << "HDR FRAME CLIPPED" << std::endl;
        close(sock);
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
    size_t last_packet_len = !(frameSize % NETWORK_MTU) ? NETWORK_MTU : (frameSize % NETWORK_MTU) ;
    size_t packet_count;

    if(last_packet_len == NETWORK_MTU)
        packet_count = frameSize / NETWORK_MTU;
    else
        packet_count = (frameSize / NETWORK_MTU) + 1;

    std::cout << "frameSize: " << std::dec << frameSize << std::endl;

    len = sizeof(serv_addr);
    sendto(sock,ack.c_str(),ack.size(),0,( struct sockaddr *)&serv_addr,len);

    if(m_data != NULL)
        delete[] m_data;
    m_data = new unsigned char[frameSize];
    bool local_flg = false;
    bool stream_flg = true;
    bool frameEnd;

    std::cout << "STREAM DATA" << std::endl;

    while(true)
    {
        if(stream_flg)
        {

            unsigned char* data_ptr = nullptr;
            size_t data_len = 0;
            uint16_t packet_id;
            long time = 0;
            bool parse_success = false;

            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

            frameEnd = false;
            while(!frameEnd)
            {
                valread = recvfrom(sock, (char *)buffer, MAXTXLEN-1, 0, ( struct sockaddr *) &tmp_address, &len);
                if ((valread > 0) && (compare_sockaddr_in(serv_addr,tmp_address)))
                {
                    data_ptr = parse_packet(buffer,MAXTXLEN,valread,&data_len,&packet_id,&parse_success);
                    if(parse_success)
                    {
                        if((packet_id == packet_count) && (data_len > last_packet_len))
                            data_len = last_packet_len;
                        insert_frame_data(data_ptr,data_len,packet_id,m_data,frameSize);

                    }
                }



                end = std::chrono::steady_clock::now();
                time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

                //std::cout << "[" << time << "]count:" << count << std::endl;

                if(time > 1000000)
                {
                    std::cout << "TIMEOUT" << std::endl;
                    break;
                }
            }




            frame_mutex.lock();
            frame = cv::Mat(rows,cols,type,m_data,step);
            frame_mutex.unlock();
            emit updateFrame();

            socklen_t len = sizeof(serv_addr);

            clientOnFlag_mutex.lock();
            clientCtrlFlag_mutex.lock();
            if(!clientCtrlFlag && clientOnFlag)
                sendto(sock,ack.c_str(),ack.size(),0,( struct sockaddr *)&serv_addr,len);
            clientCtrlFlag_mutex.unlock();
            clientOnFlag_mutex.unlock();
        }

        clientCtrlFlag_mutex.lock();
        if(clientCtrlFlag)
        {

            if(stream_flg)
            {
                stream_flg = false;
                sendto(sock,pause.c_str(),pause.size(),0,( struct sockaddr *)&serv_addr,len);

            }
            else
            {
                stream_flg = true;
                sendto(sock,play.c_str(),play.size(),0,( struct sockaddr *)&serv_addr,len);
            }
            emit ctrlMessageSent(stream_flg);
            clientCtrlFlag = false;
        }
        clientCtrlFlag_mutex.unlock();

        clientOnFlag_mutex.lock();
        if(!clientOnFlag)
        {
            sendto(sock,fin.c_str(),fin.size(),0,( struct sockaddr *)&serv_addr,len);
            local_flg = true;
        }
        clientOnFlag_mutex.unlock();

        if(local_flg)
            break;


    }

    close(sock);
    emit clientClosed();
}

unsigned char *UdpClient::parse_packet(unsigned char *buffer, size_t buffer_len, size_t tx_len, size_t *data_len, uint16_t *packet_id, bool *success)
{

    if ((tx_len == 0) ||(tx_len < 4))
    {
        *success = false;
        return nullptr;
    }

    bool start = false;
    bool end = false;
    unsigned char* data_ptr = nullptr;
    for(size_t i = 0; i <= buffer_len - 4; i++)
    {
        if((buffer[i] == 'F') && (buffer[i+1] == 'R') && (buffer[i+2] == 'M') && (buffer[i+3] == ':'))
        {
            if((i < (buffer_len - 7)) && !start)
            {
                *packet_id = ((uint16_t)buffer[i+4] << 8) | (uint16_t)buffer[i+5];
                start = true;
                data_ptr = buffer + i + 6;
            }
        }

        if((buffer[i] == 'E') && (buffer[i+1] == 'N') && (buffer[i+2] == 'D') && (buffer[i+3] == ';') && start)
        {
            *data_len = (buffer + i) - data_ptr;
            *success = true;
            end = true;
        }

        if(start && !end && (i == (buffer_len - 4)))
        {
            *data_len = (buffer + buffer_len) - data_ptr;
            if(*data_len > NETWORK_MTU)
                *data_len = NETWORK_MTU;
            *success = true;

        }


    }

    return data_ptr;
}

void UdpClient::insert_frame_data(unsigned char *data_ptr, size_t data_len, uint16_t packet_id, unsigned char *frame, size_t framesize)
{
    size_t ofst = (packet_id - 1) * NETWORK_MTU;

    if((ofst + data_len) > framesize)
    {
        std::cout << "FRAME OVERRUN" << std::endl;
        return;
    }

    memcpy(frame + ofst,data_ptr,data_len);
    return;
}

bool UdpClient::compare_sockaddr_in(const sockaddr_in &sa1, const sockaddr_in &sa2)
{
    return (sa1.sin_family == sa2.sin_family &&
            sa1.sin_port == sa2.sin_port &&
            sa1.sin_addr.s_addr == sa2.sin_addr.s_addr);
}

bool UdpClient::substringCheck(std::string a, std::string b, size_t *idx)
{
    size_t pos;

    pos = a.find(b);
    *idx = pos;
    if(pos != std::string::npos)
        return true;
    else
        return false;
}
