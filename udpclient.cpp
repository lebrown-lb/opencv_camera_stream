#include "udpclient.h"

#include <iostream>
#include <chrono>

#include <QMutex>
#include <QThread>

#define HDR_LEN         18
#define NETWORK_MTU     1400
#define MAXTXLEN        NETWORK_MTU + HDR_LEN

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


    if((idx + 20) > MAXTXLEN)
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

    std::cout << "frameSize: " << std::dec << frameSize << std::endl;

    len = sizeof(serv_addr);
    sendto(sock,ack.c_str(),ack.size(),0,( struct sockaddr *)&serv_addr,len);

    if(m_data != NULL)
        delete[] m_data;
    m_data = new unsigned char[frameSize];
    bool local_flg = false;
    bool stream_flg = true;
    size_t pkt_ofst;
    bool frameEnd;
    CODEC decoder(true,cols,rows);

    if(!decoder.m_open)
    {
        std::cout << "DECODER NOT OPEN" << std::endl;
        close(sock);
        emit clientClosed();
        return;

    }

    AVPacket * pkt = av_packet_alloc();

    if(pkt == nullptr)
    {
        std::cout << "AVPacket ALLOC FAILED" << std::endl;
        close(sock);
        emit clientClosed();
        return;
    }

     AVFrame* av_frame = av_frame_alloc();

    if(av_frame == nullptr)
    {
        std::cout << "AVFrame ALLOC FAILED" << std::endl;
        av_packet_free(&pkt);
        close(sock);
        emit clientClosed();
        return;
    }


    std::cout << "STREAM DATA" << std::endl;

    while(true)
    {
        if(stream_flg)
        {
            long time = 0;
            int ret;

            std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
            std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();

            pkt_ofst = 0;
            frameEnd = false;
            while(!frameEnd)
            {
                valread = recvfrom(sock, (char *)buffer, MAXTXLEN, 0, ( struct sockaddr *) &tmp_address, &len);

                if ((valread > 0) && (compare_sockaddr_in(serv_addr,tmp_address)))
                {
                    frameEnd = process_data(buffer, MAXTXLEN, pkt, &pkt_ofst);
                    if(frameEnd)
                    {

                        ret = decoder.send_packet(pkt);
                        if(ret < 0)
                            std::cout << "ERROR SENDING PACKET" << std::endl;
                        else
                        {
                            while ((ret = decoder.recieve_frame(av_frame)) >= 0) {
                                // Process the decoded AVFrame (e.g., display, save, further processing)
                                //std::cout << "[" << av_frame->pts << "]:" << av_frame->width << std::endl;
                                decoder.AVFrame_to_RGB(m_data,av_frame);
                                av_frame_unref(av_frame); // Unreference the frame for reuse
                            }
                            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
                                std::cout << "FFMPEG ERROR" << std::endl;
                        }

                    }
                }
                end = std::chrono::steady_clock::now();
                time = std::chrono::duration_cast<std::chrono::microseconds>(end - begin).count();

                //std::cout << "[" << time << "]count:" << count << std::endl;

                if(time > 150000)
                {
                    std::cout << "TIMEOUT" << std::endl;
                    break;
                }
            }

            frame_mutex.lock();
            frame = cv::Mat(rows,cols,type,m_data,step);
            frame_mutex.unlock();
            emit updateFrame();

            len = sizeof(serv_addr);

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

    av_packet_free(&pkt);
    av_frame_free(&av_frame);
    close(sock);
    emit clientClosed();
}

bool UdpClient::process_data(unsigned char *buffer, size_t buffer_len, AVPacket *pkt, size_t *pkt_ofst)
{
    uint16_t data_size;
    size_t pts;
    size_t dts;
    size_t pkt_size;
    //read HEADER

    if((buffer[0] == 'F') && (buffer[1] == 'R') && (buffer[2] == 'M') && (buffer[3] == ':'))
    {
        data_size = static_cast<uint16_t>((buffer[4] << 8) | buffer[5]);
        pts = static_cast<size_t>((buffer[6] << 24) | (buffer[7] << 16) | (buffer[8] << 8) | buffer[9]);
        dts = static_cast<size_t>((buffer[10] << 24) | (buffer[11] << 16) | (buffer[12] << 8) | buffer[13]);
        pkt_size = static_cast<size_t>((buffer[14] << 24) | (buffer[15] << 16) | (buffer[16] << 8) | buffer[17]);


        if(!*pkt_ofst)
        {
            if(pkt->data != nullptr)
                av_packet_unref(pkt);

            if (av_new_packet(pkt, pkt_size) < 0) {
                std::cout << "Error allocating new packet payload" << std::endl;
                return false;
            }

            pkt->pts = pts;
            pkt->dts = dts;

            if(HDR_LEN + data_size > buffer_len)
            {
                std::cout << "BUFFER OVERRUN" << std::endl;
                return false;
            }

            memcpy(pkt->data,buffer+HDR_LEN,data_size);
            *pkt_ofst += data_size;

            if(*pkt_ofst == pkt_size)
            {
                *pkt_ofst = 0;
                return true;
            }
            else
                return false;
        }
        else
        {
            if(pkt->pts != (int64_t)pts)
            {
                std::cout << "PACKET MISMATCH" << std::endl;
                *pkt_ofst = 0;
                return false;
            }

            if((*pkt_ofst + data_size) > (size_t)pkt->size)
            {
                std::cout << "PACKET BUFFER OVERRUN" << std::endl;
                *pkt_ofst = 0;
                return false;
            }

            memcpy(pkt->data + *pkt_ofst,buffer + HDR_LEN,data_size);
            *pkt_ofst += data_size;

            if(*pkt_ofst == pkt_size)
            {
                *pkt_ofst = 0;
                return true;
            }
            else
                return false;
        }

    }
    else
        return false;

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
