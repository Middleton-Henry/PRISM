/*
1) Cluster colors (count color frequency)
2) Create palettes
*/


class Quantizer {
    // placeholder — implement in pipeline/Quantizer.cpp
};

//Legacy code from ImageProcessor class
/*
void buildColorFrequency(int index) {
        if (index < 0 || index >= static_cast<int>(imageArray.size()))
            throw std::out_of_range("Index out of range");

        const cv::Mat& image = imageArray[index];
        if (image.empty())
            throw std::runtime_error("Image at index is empty");

        // Normalize to 4-channel BGRA so alpha is always available
        cv::Mat img4;
        if (image.channels() == 4)
            img4 = image;
        else if (image.channels() == 3)
            cv::cvtColor(image, img4, cv::COLOR_BGR2BGRA);
        else if (image.channels() == 1)
            cv::cvtColor(image, img4, cv::COLOR_GRAY2BGRA);

        auto& freq = imageDataArray[index].imageStructure.colorFrequency;
        freq.clear();

        for (int y = 0; y < img4.rows; ++y) {
            const uint8_t* row = img4.ptr<uint8_t>(y);
            for (int x = 0; x < img4.cols; ++x) {
                uint8_t b = row[x * 4 + 0];
                uint8_t g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2];
                uint8_t a = row[x * 4 + 3];
                freq[packRGBA(r, g, b, a)]++;
            }
        }

        imageDataArray[index].imageTraits.uniqueColors =
            static_cast<uint32_t>(freq.size());
    }

    const std::unordered_map<uint32_t, int>& getColorFrequency(int index) const {
            if (index < 0 || index >= static_cast<int>(imageDataArray.size()))
                throw std::out_of_range("Index out of range");
            return imageDataArray[index].imageStructure.colorFrequency;
        }

    uint32_t countUniqueColors(int index) {
        if (index < 0 || index >= static_cast<int>(imageArray.size()))
            throw std::out_of_range("Index out of range");
        return countUniqueColors(imageArray[index]);
    }
*/