#include "ImageResizer.h"
#include <iostream>

int main() {
    std::string inputFile = "input.jpg";
    std::string outputFile = "output.jpg";

    int targetWidth = 250; // SMALL_WIDTH
    if (ImageResizer::resizeImage(inputFile, outputFile, targetWidth)) {
        std::cout << "Image resized successfully." << std::endl;
    } else {
        std::cerr << "Failed to resize image." << std::endl;
    }

    return 0;
}
