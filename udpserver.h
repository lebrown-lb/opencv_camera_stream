#ifndef UDPSERVER_H
#define UDPSERVER_H

#include <QObject>
#include <opencv2/opencv.hpp>
#include <opencv2/videoio.hpp>


#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

enum SOCKET_STATUS
{
    CLIENT_CONNECTED,
    CLOSE_SIGNAL_RECEIVED,
    UNINITALIZED,
    SELECT_ERROR,
    ACCEPT_ERROR
};

class UdpServer : public QObject
{
    Q_OBJECT
public:
    explicit UdpServer(QObject *parent = nullptr, size_t width = 1280,size_t height = 720);

public slots:
    void runServer();

signals:
    void serverClosed();

private:
    std::string clientRead(int sock_fd,char * buffer, sockaddr_in c_address);
    size_t build_packet(char *buffer,uint8_t * data, uint16_t data_size, size_t pts, size_t dts, size_t avpkt_size, size_t buffer_len);
    bool compare_sockaddr_in(const sockaddr_in& sa1, const sockaddr_in& sa2);
    void removeUnseenCharacters(std::string& s);
    bool substringCheck(std::string& a, std::string& b, size_t *idx);
    void buildMatHeader(cv::Mat & src, uint8_t* data);
    void printSocketStatus(SOCKET_STATUS s);
    unsigned int m_port = 8080;
    size_t m_width;
    size_t m_height;
};

#endif // UDPSERVER_H
