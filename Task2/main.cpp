#include "VideoConverter.h"
#include <iostream>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <video file>" << std::endl;
        return 1;
    }

    std::string inputVideo = argv[1];
    std::string outputVideo = "converted_video.mp4";
    std::string thumbnailFile = "thumbnail.jpg";

    VideoConverter converter;
    if (!converter.convertToMP4(inputVideo, outputVideo)) {
        std::cerr << "Failed to convert video to MP4" << std::endl;
        return 1;
    }

    if (!converter.extractThumbnail(outputVideo, thumbnailFile)) {
        std::cerr << "Failed to extract thumbnail" << std::endl;
        return 1;
    }

    if (!converter.generateResizedThumbnails(thumbnailFile)) {
        std::cerr << "Failed to generate resized thumbnails" << std::endl;
        return 1;
    }

    std::cout << "Conversion and thumbnail extraction completed successfully." << std::endl;
    return 0;
}
