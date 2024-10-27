#include <iostream>
#include <fstream>
#include <cstdlib>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

#include "ImageResizer.h"

int ImageResizer::calculateTargetHeight(int originalWidth, int originalHeight, int targetWidth) {
    return (targetWidth * originalHeight) / originalWidth;
}

bool ImageResizer::resizeImage(const std::string &inputFile, const std::string &outputFile, int targetWidth) {
    // Initialize FFmpeg
    av_register_all();

    // Open input file and initialize the format context
    AVFormatContext *formatContext = nullptr;
    if (avformat_open_input(&formatContext, inputFile.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open input file: " << inputFile << std::endl;
        return false;
    }

    // Find the video stream
    if (avformat_find_stream_info(formatContext, nullptr) < 0) {
        std::cerr << "Could not find stream info" << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    AVCodecContext *codecContext = nullptr;
    int videoStreamIndex = -1;
    for (unsigned int i = 0; i < formatContext->nb_streams; i++) {
        if (formatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            videoStreamIndex = i;
            AVCodec *codec = avcodec_find_decoder(formatContext->streams[i]->codecpar->codec_id);
            codecContext = avcodec_alloc_context3(codec);
            avcodec_parameters_to_context(codecContext, formatContext->streams[i]->codecpar);
            if (avcodec_open2(codecContext, codec, nullptr) < 0) {
                std::cerr << "Could not open codec" << std::endl;
                avformat_close_input(&formatContext);
                return false;
            }
            break;
        }
    }

    if (videoStreamIndex == -1) {
        std::cerr << "No video stream found" << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    // Calculate the target height to preserve the aspect ratio
    int targetHeight = calculateTargetHeight(codecContext->width, codecContext->height, targetWidth);

    // Initialize the scaling context
    SwsContext *swsContext = sws_getContext(
        codecContext->width, codecContext->height, codecContext->pix_fmt,
        targetWidth, targetHeight, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    // Allocate memory for the frames
    AVFrame *frame = av_frame_alloc();
    AVFrame *frameResized = av_frame_alloc();
    if (!frame || !frameResized) {
        std::cerr << "Could not allocate frames" << std::endl;
        avformat_close_input(&formatContext);
        return false;
    }

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, targetWidth, targetHeight, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameResized->data, frameResized->linesize, buffer, AV_PIX_FMT_RGB24, targetWidth, targetHeight, 1);

    // Read frames and scale
    AVPacket packet;
    av_init_packet(&packet);
    bool success = false;
    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            int frameFinished;
            avcodec_decode_video2(codecContext, frame, &frameFinished, &packet);
            if (frameFinished) {
                // Scale the image
                sws_scale(swsContext, frame->data, frame->linesize, 0, codecContext->height,
                          frameResized->data, frameResized->linesize);

                // Encoding and saving the resized image
                AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
                if (!jpegCodec) {
                    std::cerr << "JPEG codec not found" << std::endl;
                    break;
                }

                AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec);
                jpegContext->bit_rate = 400000; // Adjust as necessary
                jpegContext->width = targetWidth;
                jpegContext->height = targetHeight;
                jpegContext->pix_fmt = AV_PIX_FMT_RGB24;
                jpegContext->time_base = {1, 1};
                jpegContext->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
                jpegContext->quality = 90; // Set quality for JPEG compression

                // Open JPEG encoder
                if (avcodec_open2(jpegContext, jpegCodec, nullptr) < 0) {
                    std::cerr << "Could not open JPEG codec" << std::endl;
                    avcodec_free_context(&jpegContext);
                    break;
                }

                AVPacket jpgPacket;
                av_init_packet(&jpgPacket);
                jpgPacket.data = nullptr;
                jpgPacket.size = 0;

                // Send frame to JPEG encoder
                int ret = avcodec_send_frame(jpegContext, frameResized);
                if (ret < 0) {
                    std::cerr << "Error sending frame to JPEG encoder" << std::endl;
                    avcodec_free_context(&jpegContext);
                    break;
                }

                // Receive packet from JPEG encoder
                ret = avcodec_receive_packet(jpegContext, &jpgPacket);
                if (ret == 0) {
                    // Save the resized image
                    std::ofstream outFile(outputFile, std::ios::out | std::ios::binary);
                    if (!outFile) {
                        std::cerr << "Could not open output file: " << outputFile << std::endl;
                        av_packet_unref(&jpgPacket);
                        avcodec_free_context(&jpegContext);
                        break;
                    }
                    outFile.write(reinterpret_cast<const char *>(jpgPacket.data), jpgPacket.size);
                    outFile.close();
                    success = true;
                } else {
                    std::cerr << "Error receiving packet from JPEG encoder" << std::endl;
                }

                av_packet_unref(&jpgPacket);
                avcodec_free_context(&jpegContext);

                // Break after processing the first frame
                break;
            }
        }
        av_packet_unref(&packet);
    }

    // Clean up
    av_free(buffer);
    av_frame_free(&frame);
    av_frame_free(&frameResized);
    sws_freeContext(swsContext);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);

    return success;
}
