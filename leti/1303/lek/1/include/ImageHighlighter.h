#include "ImageLineWorker.h"
#include <map>
class Image;

typedef std::map<std::pair<unsigned int, unsigned int>, unsigned char> ColorMap;
class ImageHighlighter : ImageLineWorker<Image*, unsigned char*, tuple<ColorMap, Image*> > {
public:
    Image *operator()(tuple<ColorMap, Image*> imageAndChanges);
};
