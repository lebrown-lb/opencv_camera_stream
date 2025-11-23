#include "codec.h"
#include <iostream>

CODEC::CODEC(bool decoder, size_t width, size_t height)
    : m_decoder(decoder), m_width(width), m_height(height)
{


    if(m_decoder)
    {
        m_codec = avcodec_find_decoder(AV_CODEC_ID_H264);
        if(!m_codec)
        {
            std::cout << "DECODER NOT FOUND" << std::endl;
            return;
        }

    }
    else
    {
        m_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
        if(!m_codec)
        {
            std::cout << "ENCODER NOT FOUND" << std::endl;
            return;
        }
    }


    if(m_codec != nullptr)
    {
        m_codecContext = avcodec_alloc_context3(m_codec);

        m_codecContext->width = m_width;
        m_codecContext->height = m_height;
        m_codecContext->time_base = {1, 30}; // 30 fps
        m_codecContext->pix_fmt = AV_PIX_FMT_YUV420P;
        m_codecContext->bit_rate = 3000000; // Example bitrate
        m_codecContext->max_b_frames = 0;
        m_codecContext->flags |= AV_CODEC_FLAG_LOW_DELAY;

        if(!m_decoder)
            av_opt_set(m_codecContext->priv_data, "preset", "ultrafast", 0);

        AVDictionary *codec_opts = nullptr;
        av_dict_set(&codec_opts, "preset", "ultrafast", 0);
        av_dict_set(&codec_opts, "tune", "zerolatency", 0);

        if (avcodec_open2(m_codecContext, m_codec,&codec_opts) < 0) {
            std::cout << "Could not open codec." << std::endl;
            return;
        }
        m_open = true;
    }

}

CODEC::~CODEC()
{
    if(m_codecContext != nullptr)
        avcodec_free_context(&m_codecContext);

}

bool CODEC::mat_to_AVFrame(AVFrame *dst, cv::Mat &src)
{
    SwsContext* sws_ctx = nullptr;
    sws_ctx = sws_getCachedContext(
        sws_ctx,
        m_width, m_height, AV_PIX_FMT_RGB24, // Source format (OpenCV BGR is often AV_PIX_FMT_BGR24 in memory)
        m_width, m_height, AV_PIX_FMT_YUV420P, // Destination format
        SWS_BICUBIC, // Scaling algorithm (choose as needed)
        nullptr, nullptr, nullptr
        );

    if (!sws_ctx)
    {
        std::cout << "Failed to initialize SwsContext" << std::endl;
        return false;
    }

    uint8_t* src_data[AV_NUM_DATA_POINTERS] = { src.data };
    int src_linesize[AV_NUM_DATA_POINTERS] = { static_cast<int>(src.step[0]) };

    sws_scale(sws_ctx, src_data, src_linesize, 0, m_height, dst->data, dst->linesize);

    sws_freeContext(sws_ctx);
    return true;

}

bool CODEC::AVFrame_to_RGB(uint8_t *dst, AVFrame *src)
{
    struct SwsContext *sws_ctx = sws_getContext(
        m_codecContext->width, m_codecContext->height, m_codecContext->pix_fmt,
        m_codecContext->width, m_codecContext->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, NULL, NULL, NULL
        );

    uint8_t *dst_data[4] = {dst, NULL, NULL, NULL};
    int dst_linesize[4] = {m_codecContext->width * 3, 0, 0, 0}; // For RGB24, linesize is width * 3

    sws_scale(
        sws_ctx,
        (const uint8_t *const *)src->data, src->linesize,
        0, m_codecContext->height,
        dst_data, dst_linesize
        );

    sws_freeContext(sws_ctx);

    return true;
}

int CODEC::send_frame(AVFrame *frm)
{
    return avcodec_send_frame(m_codecContext,frm);

}

int CODEC::recieve_packet(AVPacket *pkt)
{
    return avcodec_receive_packet(m_codecContext, pkt);

}

int CODEC::send_packet(AVPacket *pkt)
{
    return avcodec_send_packet(m_codecContext, pkt);
}

int CODEC::recieve_frame(AVFrame *frm)
{
    return avcodec_receive_frame(m_codecContext, frm);
}
