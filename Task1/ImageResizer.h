#ifndef IMAGE_RESIZER_H
#define IMAGE_RESIZER_H

#include <string>

extern "C" {
    struct AVFormatContext;
    struct AVCodecContext;
    struct AVFrame;
    struct SwsContext;
}

class ImageResizer {
public:
    // Resizes the input image to the target width, preserving the aspect ratio, and saves it to the output file
    static bool resizeImage(const std::string &inputFile, const std::string &outputFile, int targetWidth);

private:
    // Calculates the target height to preserve the aspect ratio
    static int calculateTargetHeight(int originalWidth, int originalHeight, int targetWidth);
};

#endif