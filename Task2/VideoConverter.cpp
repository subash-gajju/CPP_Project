#include "VideoConverter.h"
#include "../task1/ImageResizer.h"
#include <iostream>
#include <fstream>
extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libswscale/swscale.h>
    #include <libavutil/imgutils.h>
}

bool VideoConverter::convertToMP4(const std::string &inputFile, const std::string &outputFile) {
    // Initialize FFmpeg
    av_register_all();

    // Open input file
    AVFormatContext *inputFormatContext = nullptr;
    if (avformat_open_input(&inputFormatContext, inputFile.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open input video file: " << inputFile << std::endl;
        return false;
    }

    // Find the stream info
    if (avformat_find_stream_info(inputFormatContext, nullptr) < 0) {
        std::cerr << "Could not find stream info" << std::endl;
        avformat_close_input(&inputFormatContext);
        return false;
    }

    // Allocate output context
    AVFormatContext *outputFormatContext = nullptr;
    avformat_alloc_output_context2(&outputFormatContext, nullptr, "mp4", outputFile.c_str());
    if (!outputFormatContext) {
        std::cerr << "Could not create output context" << std::endl;
        avformat_close_input(&inputFormatContext);
        return false;
    }

    // Copy the input streams to the output context
    for (unsigned int i = 0; i < inputFormatContext->nb_streams; i++) {
        AVStream *inputStream = inputFormatContext->streams[i];
        AVStream *outputStream = avformat_new_stream(outputFormatContext, nullptr);
        if (!outputStream) {
            std::cerr << "Failed to allocate output stream" << std::endl;
            avformat_close_input(&inputFormatContext);
            avformat_free_context(outputFormatContext);
            return false;
        }

        // Copy codec parameters from input to output stream
        avcodec_parameters_copy(outputStream->codecpar, inputStream->codecpar);
        outputStream->codecpar->codec_tag = 0;  // Clear codec tag for output
    }

    // Open output file
    if (!(outputFormatContext->oformat->flags & AVFMT_NOFILE)) {
        if (avio_open(&outputFormatContext->pb, outputFile.c_str(), AVIO_FLAG_WRITE) < 0) {
            std::cerr << "Could not open output file: " << outputFile << std::endl;
            avformat_close_input(&inputFormatContext);
            avformat_free_context(outputFormatContext);
            return false;
        }
    }

    // Write the header for the output file
    if (avformat_write_header(outputFormatContext, nullptr) < 0) {
        std::cerr << "Could not write output file header" << std::endl;
        avio_closep(&outputFormatContext->pb);
        avformat_close_input(&inputFormatContext);
        avformat_free_context(outputFormatContext);
        return false;
    }

    // Read frames from the input file and write them to the output file
    AVPacket packet;
    while (av_read_frame(inputFormatContext, &packet) >= 0) {
        AVStream *inputStream = inputFormatContext->streams[packet.stream_index];
        AVStream *outputStream = outputFormatContext->streams[packet.stream_index];

        // Rescale packet timestamps to output stream
        av_packet_rescale_ts(&packet, inputStream->time_base, outputStream->time_base);

        // Write packet to output file
        if (av_interleaved_write_frame(outputFormatContext, &packet) < 0) {
            std::cerr << "Error while writing frame" << std::endl;
            break;
        }
        av_packet_unref(&packet);
    }

    // Write the trailer for the output file
    av_write_trailer(outputFormatContext);

    // Cleanup
    avio_closep(&outputFormatContext->pb);
    avformat_close_input(&inputFormatContext);
    avformat_free_context(outputFormatContext);

    return true;
}

bool VideoConverter::extractThumbnail(const std::string &videoFile, const std::string &thumbnailFile) {
    // Initialize FFmpeg
    av_register_all();

    // Open input file
    AVFormatContext *formatContext = avformat_alloc_context();
    if (avformat_open_input(&formatContext, videoFile.c_str(), nullptr, nullptr) != 0) {
        std::cerr << "Could not open input video file: " << videoFile << std::endl;
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

    // Seek to the middle of the video
    int64_t midTimestamp = formatContext->duration / 2;
    if (av_seek_frame(formatContext, videoStreamIndex, midTimestamp, AVSEEK_FLAG_BACKWARD) < 0) {
        std::cerr << "Could not seek to the middle of the video" << std::endl;
        avformat_close_input(&formatContext);
        avcodec_free_context(&codecContext);
        return false;
    }

    // Read frame from the video
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();
    AVFrame *frameRGB = av_frame_alloc();

    int numBytes = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);
    uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
    av_image_fill_arrays(frameRGB->data, frameRGB->linesize, buffer, AV_PIX_FMT_RGB24, codecContext->width, codecContext->height, 1);

    SwsContext *swsContext = sws_getContext(
        codecContext->width, codecContext->height, codecContext->pix_fmt,
        codecContext->width, codecContext->height, AV_PIX_FMT_RGB24,
        SWS_BILINEAR, nullptr, nullptr, nullptr
    );

    bool success = false;
    av_init_packet(&packet);
    while (av_read_frame(formatContext, &packet) >= 0) {
        if (packet.stream_index == videoStreamIndex) {
            int frameFinished;
            avcodec_decode_video2(codecContext, frame, &frameFinished, &packet);
            if (frameFinished) {
                sws_scale(swsContext, frame->data, frame->linesize, 0, codecContext->height,
                          frameRGB->data, frameRGB->linesize);

                // Save the frame as a JPEG image
                AVCodec *jpegCodec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
                if (!jpegCodec) {
                    std::cerr << "JPEG codec not found" << std::endl;
                    break;
                }

                AVCodecContext *jpegContext = avcodec_alloc_context3(jpegCodec);
                jpegContext->bit_rate = codecContext->bit_rate;
                jpegContext->width = codecContext->width;
                jpegContext->height = codecContext->height;
                jpegContext->pix_fmt = AV_PIX_FMT_YUVJ420P;
                jpegContext->time_base = {1, 25};

                if (avcodec_open2(jpegContext, jpegCodec, nullptr) < 0) {
                    std::cerr << "Could not open JPEG codec" << std::endl;
                    avcodec_free_context(&jpegContext);
                    break;
                }

                AVPacket jpgPacket;
                av_init_packet(&jpgPacket);
                jpgPacket.data = nullptr;
                jpgPacket.size = 0;

                int ret = avcodec_send_frame(jpegContext, frameRGB);
                if (ret < 0) {
                    std::cerr << "Error sending frame to JPEG encoder" << std::endl;
                    avcodec_free_context(&jpegContext);
                    break;
                }

                ret = avcodec_receive_packet(jpegContext, &jpgPacket);
                if (ret == 0) {
                    // Write the JPEG data to a file
                    std::ofstream outFile(thumbnailFile, std::ios::out | std::ios::binary);
                    outFile.write(reinterpret_cast<const char *>(jpgPacket.data), jpgPacket.size);
                    outFile.close();

                    av_packet_unref(&jpgPacket);
                    success = true;
                    avcodec_free_context(&jpegContext);
                    break;
                } else {
                    std::cerr << "Error receiving packet from JPEG encoder" << std::endl;
                }

                avcodec_free_context(&jpegContext);
            }
        }
        av_packet_unref(&packet);
    }

    // Clean up
    av_free(buffer);
    av_frame_free(&frame);
    av_frame_free(&frameRGB);
    sws_freeContext(swsContext);
    avcodec_free_context(&codecContext);
    avformat_close_input(&formatContext);

    return success;
}

bool VideoConverter::generateResizedThumbnails(const std::string &thumbnailFile) {
    ImageResizer resizer;
    return resizer.resizeImage(thumbnailFile, "thumbnail_small.jpg", 250) &&
           resizer.resizeImage(thumbnailFile, "thumbnail_medium.jpg", 350) &&
           resizer.resizeImage(thumbnailFile, "thumbnail_large.jpg", 650);
}
