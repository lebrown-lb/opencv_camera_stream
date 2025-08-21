#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>
#include <sstream>
#include <QLabel>
#include <QImage>
#include <QPixmap>
#include <QMutex>
#include <QComboBox>

#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>

QMutex  frame_mutex;
cv::Mat frame;

QMutex serverOnFlag_mutex;
bool serverOnFlag = false;

QMutex clientOnFlag_mutex;
bool clientOnFlag = false;

QMutex newDataFlag_mutex;
bool newDataFlg = false;

QMutex clientCtrlFlag_mutex;
bool clientCtrlFlag = false;

#define FORMAT_BGR  0xFF
#define FORMAT_RGB  0x11

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->stream_pb, &QPushButton::clicked, this, &MainWindow::streamCtrlFunction);
    connect(ui->server_pb, &QPushButton::clicked, this, &MainWindow::serverCtrlFunction);
    connect(ui->control_pb,&QPushButton::clicked, this, &MainWindow::ctrlFunction);
    connect(ui->client_pb,&QPushButton::clicked, this, &MainWindow::clientCtrlFunction);
    connect(ui->target_address_le, &QLineEdit::textChanged, this, &MainWindow::targetAddressChange);
    connect(ui->target_port_le, &QLineEdit::textChanged, this, &MainWindow::targetPortChange);
    connect(ui->mode_cb, SIGNAL(currentIndexChanged(int)), this, SLOT(modeChangeHandler(int)));
    m_cap.open(0);

    if(m_cap.isOpened())
    {
        std::cout << "OPENED CAMERA" << std::endl;

        m_cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280); // Set width to 1280 pixels
        m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720); // Set height to 720 pixels

        m_timer = new QTimer(this);
        connect(m_timer, SIGNAL(timeout()), this, SLOT(updateStream()));

        m_serverThread = new QThread(this);
        m_server = new TcpServer();
        m_server->moveToThread(m_serverThread);

        connect(this,SIGNAL(startServer()),m_server,SLOT(runServer()));
        connect(m_server,SIGNAL(serverClosed()),this,SLOT(handleServerStop()));
    }
    else
        std::cout << "ERROR OPENING CAMERA!" << std::endl;

    ui->address_lbl->setText(QString::fromStdString(getNetworkAddress()));

    m_clientThread = new QThread(this);
    m_client = new TcpClient();
    m_client->moveToThread(m_clientThread);

    connect(this,SIGNAL(startClient()),m_client,SLOT(runClient()));
    connect(m_client,SIGNAL(clientClosed()),this,SLOT(handleClientStop()));
    connect(m_client, SIGNAL(updateFrame()),this,SLOT(displayFrame()));
    connect(m_client, SIGNAL(ctrlMessageSent(bool)),this,SLOT(ctrlMessageSentHandler(bool)));


    ui->target_address_le->setText("127.0.0.1");
    ui->target_port_le->setText("8080");
}

MainWindow::~MainWindow()
{
    delete ui;
    if(m_cap.isOpened())
        m_cap.release();
    m_client->freeData();
}

void MainWindow::updateStream()
{
    if(m_streamFlg)
    {
        //std::cout << "FRAME UPDATE!" << std::endl;
        frameCapture();
    }

}

void MainWindow::displayFrame()
{
    frame_mutex.lock();
    QImage image = matToQImage(frame, FORMAT_RGB);
    frame_mutex.unlock();

    QPixmap myPixmap = QPixmap::fromImage(image);
    ui->display->setPixmap(myPixmap);


}

void MainWindow::handleServerStop()
{
    std::cout << "SERVER STOPPED" << std::endl;
    ui->server_pb->setText("OPEN");
    m_serverThread->exit();

    serverOnFlag_mutex.lock();
    serverOnFlag = false;
    serverOnFlag_mutex.unlock();

    if(m_exitFlag)
        this->close();

}

void MainWindow::handleClientStop()
{
    std::cout << "CLIENT STOPPED" << std::endl;
    ui->client_pb->setText("CONNECT");
    //m_clientThread->exit();

    clientOnFlag_mutex.lock();
    clientOnFlag = false;
    clientOnFlag_mutex.unlock();

    if(m_exitFlag)
        this->close();

}

void MainWindow::ctrlMessageSentHandler(bool x)
{
    if(x)
        ui->control_pb->setText("PAUSE");
    else
        ui->control_pb->setText("PLAY");

}



void MainWindow::modeChangeHandler(int index)
{
    m_mode = index;
    std::cout << "m_mode:" << +m_mode << std::endl;
    ui->stackedWidget->setCurrentIndex(m_mode);
    if((m_timer != NULL) && m_timer->isActive())
    {
        m_timer->stop();
        m_streamFlg = false;
        ui->stream_pb->setText("START");
    }
    if((m_serverThread != NULL) &&(m_server != NULL))
    {
        serverOnFlag_mutex.lock();
        if(serverOnFlag)
            serverOnFlag = false;
        serverOnFlag_mutex.unlock();
    }

}

void MainWindow::targetAddressChange()
{
    std::string tmp = ui->target_address_le->text().toStdString();
    bool flg = validateAddressFormat(tmp);
    if(flg)
    {
        //turn text green
        ui->target_address_le->setStyleSheet("QLineEdit { color: green; }");
        m_client->setNetworkAddress(tmp);
    }
    else
    {
        //turn text red
        ui->target_address_le->setStyleSheet("QLineEdit { color: red; }");
    }
}

void MainWindow::targetPortChange()
{
    std::string tmp = ui->target_port_le->text().toStdString();
    if(numeric(tmp))
    {
        //turn text green
        ui->target_port_le->setStyleSheet("QLineEdit { color: green; }");
        m_client->setPort(static_cast<unsigned int>(std::stoi(tmp)));
    }
    else
    {
        //turn text red
        ui->target_port_le->setStyleSheet("QLineEdit { color: red; }");
    }


}

void MainWindow::streamCtrlFunction()
{
    std::cout << "BUTTON PRESSED!" << std::endl;

    if(m_timer != NULL)
    {
        if(m_streamFlg)
        {
            m_streamFlg = false;
            m_timer->stop();
            ui->stream_pb->setText(QString("START"));
        }
        else
        {
            m_streamFlg = true;
            m_timer->start(m_prd);
             ui->stream_pb->setText(QString("STOP"));
        }
    }
    else
        std::cout << "STREAM CONTROL DISABED!" << std::endl;


}

void MainWindow::serverCtrlFunction()
{
    if((m_serverThread != NULL) && (m_server != NULL))
    {
        serverOnFlag_mutex.lock();
        if(serverOnFlag)
        {
            serverOnFlag  = false;

        }
        else
        {
            std::cout << "SERVER START" << std::endl;
            ui->server_pb->setText("CLOSE");
            m_serverThread->start();
            emit startServer();
            serverOnFlag = true;
        }
        serverOnFlag_mutex.unlock();
    }
    else
        std::cout << "SERVER CONTROL DISABED!" << std::endl;


}

void MainWindow::ctrlFunction()
{
    clientCtrlFlag_mutex.lock();
    clientCtrlFlag = true;
    clientCtrlFlag_mutex.unlock();

}

void MainWindow::clientCtrlFunction()
{
    clientOnFlag_mutex.lock();
    if(clientOnFlag)
    {
        clientOnFlag = false;
    }
    else
    {
        std::cout << "CLIENT START" << std::endl;
        ui->client_pb->setText("DISCONNECT");
        m_clientThread->start();
        emit startClient();
        clientOnFlag = true;
    }
    clientOnFlag_mutex.unlock();


}

void MainWindow::closeEvent(QCloseEvent *event)
{
    std::cout << "CLOSE EVENT" << std::endl;

    bool serverThreadFlag = false;
    bool clientThreadFlag = false;

    serverOnFlag_mutex.lock();
    if(serverOnFlag)
    {
        serverOnFlag = false;
        serverThreadFlag = true;
    }
    serverOnFlag_mutex.unlock();

    clientOnFlag_mutex.lock();
    if(clientOnFlag)
    {
        clientOnFlag = false;
        clientThreadFlag = true;
    }
    clientOnFlag_mutex.unlock();


    if(serverThreadFlag || clientThreadFlag)
    {
        m_exitFlag = true;
        event->ignore();
    }
    else
        event->accept(); // Allow the close event to proceed

}

std::string MainWindow::getNetworkAddress()
{
    struct ifaddrs *interfaces = nullptr;
    struct ifaddrs *ifa = nullptr;
    std::string res = "";

    if (getifaddrs(&interfaces) == -1) {
        std::cerr << "Could not retrieve network adapters." << std::endl;
        return std::string("");
    }

    for (ifa = interfaces; ifa != nullptr; ifa = ifa->ifa_next)
    {
        std::string tmp = "";

        if (ifa->ifa_addr && ifa->ifa_addr->sa_family == AF_INET)
        {
            // Check for IPv4 addresses
            char ipAddress[INET_ADDRSTRLEN];
            void* addr = &((struct sockaddr_in*)ifa->ifa_addr)->sin_addr;
            inet_ntop(AF_INET, addr, ipAddress, INET_ADDRSTRLEN);

            std::string ip = std::string(ipAddress);
            std::string interface = std::string(ifa->ifa_name);

            // Exclude loopback address (127.0.0.1) if desired
            if (std::string(ipAddress) != "127.0.0.1") {

                tmp += "Interface: ";
                tmp += interface;
                tmp += ", IP Address: ";
                tmp += ip;

            }
            res += tmp;
            res += "\n\r";

        }

    }
    freeifaddrs(interfaces);

    return res;

}

bool MainWindow::validateAddressFormat(std::string s)
{
    std::string::difference_type n = std::count(s.begin(), s.end(), '.');
    unsigned int count = static_cast<unsigned int>(n);

    if(count != 3)
        return false;

    std::vector<std::string> tokens = splitString(s,'.');

    if(tokens.size() != 4)
        return false;

    for(std::string &tmp : tokens)
    {
        if(!numeric(tmp))
            return false;
        int x = std::stoi(tmp);
        if((x < 0) || (x > 255))
            return false;
    }

    return true;

}

void MainWindow::frameCapture()
{
    frame_mutex.lock();
    m_cap >> frame;

    if (!frame.empty()) {

        serverOnFlag_mutex.lock();
        if(serverOnFlag)
        {
            newDataFlag_mutex.lock();
            newDataFlg = true;
            newDataFlag_mutex.unlock();
        }
        serverOnFlag_mutex.unlock();

        QImage image = matToQImage(frame,FORMAT_BGR);
        QPixmap myPixmap = QPixmap::fromImage(image);
        ui->display->setPixmap(myPixmap);
    }
    frame_mutex.unlock();


}

QImage MainWindow::matToQImage(const cv::Mat &src, uint8_t fmt)
{
    // Example for BGR to RGB conversion (common for OpenCV)
    //td::cout << "src.channels() = " << src.channels() <<" src.step = "<< src.step << " src.cols = " << src.cols << " src.rows = " << src.rows << std::endl;
    if (src.channels() == 3) {
        //printMatRow(src,0);
        if(fmt == FORMAT_BGR)
            cv::cvtColor(src, src, cv::COLOR_BGR2RGB);
        return QImage((const unsigned char*)(src.data), src.cols, src.rows, src.step, QImage::Format_RGB888);
    } else if (src.channels() == 1) {
        return QImage((const unsigned char*)(src.data), src.cols, src.rows, src.step, QImage::Format_Grayscale8);
    }
    // Handle other formats or return null QImage
    return QImage();

}

void MainWindow::printMatRow(const cv::Mat &src, size_t row)
{
    if(row >= (size_t)src.rows)
    {
        std::cout << "ROW OUT OF INDEX!" << std::endl;
        return;
    }
    std::cout << "******************************************Mat[" << row << "]******************************************" << std::endl;
    for(int j = 0;j < src.cols ;j++){
        uchar b = src.data[src.channels()*(src.cols*row + j) + 0];
        uchar g = src.data[src.channels()*(src.cols*row + j) + 1];
        uchar r = src.data[src.channels()*(src.cols*row + j) + 2];
        std::cout << "[" << +b << "," << +g << "," << +r << "] ";
    }
    std::cout << std::endl;


}

bool MainWindow::numeric(std::string str)
{
    bool test = true;

    for(char a : str)
    {
        if(std::isdigit(a))
            continue;
        else
        {
            test = false;
            break;
        }
    }
    return test;
}

std::vector<std::string> MainWindow::splitString(const std::string &str, char delimiter)
{
    std::vector<std::string> tokens;
    std::istringstream iss(str);
    std::string token;
    while (std::getline(iss, token, delimiter)) {
        tokens.push_back(token);
    }
    return tokens;
}
