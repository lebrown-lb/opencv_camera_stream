#ifndef UDPCLIENT_H
#define UDPCLIENT_H

#include <QObject>
#include <string>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>


class UdpClient : public QObject
{
    Q_OBJECT
public:
    explicit UdpClient(QObject *parent = nullptr);
    void setPort(unsigned int x)
    {
        m_port = x;
    }
    void setNetworkAddress(std::string str)
    {
        m_ipAddress = str;
    }
    void freeData()
    {
        if(m_data != NULL)
            delete[] m_data;
    }


public slots:
    void runClient();

signals:
    void clientClosed();
    void updateFrame();
    void ctrlMessageSent(bool x);

private:
    unsigned char* parse_packet(unsigned char * buffer, size_t buffer_len, size_t tx_len, size_t* data_len, uint16_t * packet_id, bool * success);
    void insert_frame_data(unsigned char * data_ptr, size_t data_len, uint16_t packet_id, unsigned char * frame, size_t framesize);
    bool compare_sockaddr_in(const sockaddr_in& sa1, const sockaddr_in& sa2);
    bool substringCheck(std::string a, std::string b, size_t *idx);
    std::string m_ipAddress;
    unsigned int m_port;
    unsigned char * m_data = NULL;
};

#endif // UDPCLIENT_H
