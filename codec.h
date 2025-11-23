#ifndef CODEC_H
#define CODEC_H

#include <cstddef>
#include <opencv2/opencv.hpp>
extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libavutil/opt.h>
}

class CODEC
{
public:
    CODEC(bool decoder, size_t width, size_t height);
    ~CODEC();
    bool mat_to_AVFrame(AVFrame* dst, cv::Mat & src);
    bool AVFrame_to_RGB(uint8_t* dst, AVFrame * src);
    int send_frame(AVFrame * frm);
    int recieve_packet(AVPacket * pkt);
    int send_packet(AVPacket * pkt);
    int recieve_frame(AVFrame * frm);
    bool m_decoder;
    size_t m_width;
    size_t m_height;
    const AVCodec * m_codec = nullptr;
    AVCodecContext * m_codecContext = nullptr;
    bool m_open = false;
};

#endif // CODEC_H
