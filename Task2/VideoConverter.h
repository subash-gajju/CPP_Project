#ifndef VIDEOCONVERTER_H
#define VIDEOCONVERTER_H

#include <string>

class VideoConverter {
public:
    bool convertToMP4(const std::string &inputFile, const std::string &outputFile);
    bool extractThumbnail(const std::string &videoFile, const std::string &thumbnailFile);
    bool generateResizedThumbnails(const std::string &thumbnailFile);
};

#endif
