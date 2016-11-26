#ifndef IMAGE_H
#define IMAGE_H

/// Class of image.
class Image {
    static unsigned long idCounter;
    unsigned long id;
    // Width of image.
    unsigned int width;
    // Height of image.
    unsigned int height;
    // Matrix with pixels.
    unsigned char **pixelMatrix;
public:
    Image(unsigned int width, unsigned int height);
    Image(const Image& image);
    ~Image();

    unsigned int getWidth() const;
    unsigned int getHeight() const;
    /// Get separate line of image by its number.
    unsigned char* getImageLine(unsigned int lineNumber);
    unsigned long getId() const;
};

#endif
