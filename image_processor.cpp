#include <iostream>
#include <unordered_map>
#include <string>
#include <vector>
#include <cstdint>
#include <stdexcept>
#include <opencv2/opencv.hpp>
#include <filesystem>
#include <chrono>
#include <GLFW/glfw3.h>
#include <numeric>
#include <unordered_set>

#ifndef GL_TEXTURE_MAX_ANISOTROPY_EXT
#define GL_TEXTURE_MAX_ANISOTROPY_EXT 0x84FE
#endif

//organizes and parses image data
class ImageProcessor{
    public:
    struct RGBA {
        uint8_t r = 0;
        uint8_t g = 0;
        uint8_t b = 0;
        uint8_t a = 0;
    };

    struct ImageTraits {
        uint16_t width = 0;
        uint16_t height = 0;
        uint32_t uniqueColors = 0;
    };
    /*
    struct ImageStructure {
        std::unordered_map<uint32_t, int> colorFrequency;
    };
    */

    struct MetaData {
        std::string name;
        std::filesystem::file_time_type dateCreated;
        std::filesystem::file_time_type dateLastModified;
        uint32_t uploadIndex = 0;
        uint32_t size = 0; //file size
        GLuint thumbnailTextureID = 0;
    };

    struct FullImageData {
        ImageTraits imageTraits; //low cost information for basic analysis
        MetaData metaData; //information for non operational needs
    };

    std::vector<FullImageData> imageDataArray;
    std::vector<cv::Mat> imageArray;
    uint32_t uploadIndex = 0; 

    enum ImageSortingSystems{
        UPLOAD_ORDER,
        NAME,
        DATE_MODIFIED,
        DATE_CREATED
    };

    FullImageData processImage(const cv::Mat& image) {
        FullImageData imageData;
        imageData.imageTraits.width  = static_cast<uint16_t>(image.cols);
        imageData.imageTraits.height = static_cast<uint16_t>(image.rows);
        imageData.imageTraits.uniqueColors = countUniqueColors(image);
        return imageData;
    }

    uint32_t packRGBA(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
        if(a == 0) return 0; //rgb is unimportant and a is 0
        return (uint32_t(a) << 24) | (uint32_t(r) << 16) | (uint32_t(g) << 8) | uint32_t(b);
    }

    RGBA unpackRGBA(uint32_t packed) {
        return {
            static_cast<uint8_t>((packed >> 16) & 0xFF),  // r
            static_cast<uint8_t>((packed >>  8) & 0xFF),  // g
            static_cast<uint8_t>( packed        & 0xFF),  // b
            static_cast<uint8_t>((packed >> 24) & 0xFF)   // a
        };
    }

    

    uint32_t countUniqueColors(const cv::Mat& image) {
        if (image.empty())
            throw std::runtime_error("Image is empty");

        cv::Mat img4;
        if (image.channels() == 4)
            img4 = image;
        else if (image.channels() == 3)
            cv::cvtColor(image, img4, cv::COLOR_BGR2BGRA);
        else if (image.channels() == 1)
            cv::cvtColor(image, img4, cv::COLOR_GRAY2BGRA);

        std::unordered_set<uint32_t> seen;
        seen.reserve(img4.rows * img4.cols);

        for (int y = 0; y < img4.rows; ++y) {
            const uint8_t* row = img4.ptr<uint8_t>(y);
            for (int x = 0; x < img4.cols; ++x) {
                uint8_t b = row[x * 4 + 0], g = row[x * 4 + 1];
                uint8_t r = row[x * 4 + 2], a = row[x * 4 + 3];
                seen.insert(packRGBA(r, g, b, a));
            }
        }

        return static_cast<uint32_t>(seen.size());
    }

    




    void initImageDataArray(int imageCount) {
        if (imageCount < 0) throw std::invalid_argument("imageCount must be non-negative");
        imageDataArray.clear();
        imageDataArray.resize(imageCount);
        imageArray.clear();
        imageArray.resize(imageCount);
        uploadIndex = 0;
    }

    void setImageData(std::string_view filePath, int index) {
        if (index < 0 || index >= static_cast<int>(imageDataArray.size()))
            throw std::out_of_range("Index out of range");

        cv::Mat image = cv::imread(std::string(filePath), cv::IMREAD_UNCHANGED); // preserves alpha
        if (image.empty())
            throw std::runtime_error("Failed to load image: " + std::string(filePath));

        namespace fs = std::filesystem;
        fs::path path(filePath);

        imageDataArray[index] = processImage(image);
        imageDataArray[index].metaData.name              = path.filename().string();
        imageDataArray[index].metaData.dateLastModified  = fs::last_write_time(path);
        imageDataArray[index].metaData.dateCreated       = fs::last_write_time(path);

        std::error_code ec;
        imageDataArray[index].metaData.size = std::filesystem::file_size(filePath, ec);

        std::vector<unsigned char> imageThumbnail = createThumbnail(image);
        imageDataArray[index].metaData.thumbnailTextureID = uploadThumbnail(imageThumbnail);
        imageArray[index] = image.clone();

        imageDataArray[index].metaData.uploadIndex = uploadIndex;
        ++uploadIndex;
    }

    MetaData getImageData(int index){
        if (index < 0 || index >= static_cast<int>(imageDataArray.size()))
            throw std::out_of_range("Index out of range");
        
        return imageDataArray[index].metaData;
    }


    std::vector<unsigned char> createThumbnail(const cv::Mat& image, int width = 32, int height = 32) {
        cv::Mat thumbnail;
        cv::resize(image, thumbnail, cv::Size(width, height), 0, 0, cv::INTER_AREA);

        std::vector<unsigned char> buffer;
        cv::imencode(".png", thumbnail, buffer); // PNG supports alpha, JPEG does not
        return buffer;
    }

    std::string fileTimeToString(std::filesystem::file_time_type ftime) {
        auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
            ftime - std::filesystem::file_time_type::clock::now()
            + std::chrono::system_clock::now()
        );
        std::time_t tt = std::chrono::system_clock::to_time_t(sctp);
        std::tm* tm = std::localtime(&tt);

        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    void swapImages(int indexA, int indexB) {
        if (indexA < 0 || indexA >= (int)imageDataArray.size()) return;
        if (indexB < 0 || indexB >= (int)imageDataArray.size()) return;
        if (indexA == indexB) return;

        std::swap(imageDataArray[indexA], imageDataArray[indexB]);
        std::swap(imageArray[indexA], imageArray[indexB]);
    }

    void moveImage(int fromIndex, int toIndex){
        if (fromIndex < 0 || fromIndex >= (int)imageDataArray.size()) return;
        if (toIndex < 0 || toIndex >= (int)imageDataArray.size()) return;
        if (fromIndex == toIndex) return;

        FullImageData movingData  = std::move(imageDataArray[fromIndex]);
        cv::Mat       movingImage = std::move(imageArray[fromIndex]);

        if (fromIndex < toIndex) {
            for (int i = fromIndex; i < toIndex; ++i) {
                imageDataArray[i] = std::move(imageDataArray[i + 1]);
                imageArray[i]     = std::move(imageArray[i + 1]);
            }
        } else {
            for (int i = fromIndex; i > toIndex; --i) {
                imageDataArray[i] = std::move(imageDataArray[i - 1]);
                imageArray[i]     = std::move(imageArray[i - 1]);
            }
        }

        imageDataArray[toIndex] = std::move(movingData);
        imageArray[toIndex]     = std::move(movingImage);
    }

    void resortImages(ImageSortingSystems sortOrder) {
        // Build an index array and sort it according to the chosen criterion
        std::vector<int> indices(imageDataArray.size());
        std::iota(indices.begin(), indices.end(), 0);

        switch (sortOrder) {
            case UPLOAD_ORDER:
                std::sort(indices.begin(), indices.end(), [&](int a, int b) {
                    return imageDataArray[a].metaData.uploadIndex < imageDataArray[b].metaData.uploadIndex;
                });
                break;

            case NAME:
                std::sort(indices.begin(), indices.end(), [&](int a, int b) {
                    return imageDataArray[a].metaData.name < imageDataArray[b].metaData.name;
                });
                break;

            case DATE_MODIFIED:
                std::sort(indices.begin(), indices.end(), [&](int a, int b) {
                    return imageDataArray[a].metaData.dateLastModified < imageDataArray[b].metaData.dateLastModified;
                });
                break;

            case DATE_CREATED:
                std::sort(indices.begin(), indices.end(), [&](int a, int b) {
                    return imageDataArray[a].metaData.dateCreated < imageDataArray[b].metaData.dateCreated;
                });
                break;
        }

        // Apply the permutation to both parallel arrays
        std::vector<FullImageData> sortedDataArray(imageDataArray.size());
        std::vector<cv::Mat>       sortedImageArray(imageArray.size());

        for (int i = 0; i < static_cast<int>(indices.size()); ++i) {
            sortedDataArray[i]  = std::move(imageDataArray[indices[i]]);
            sortedImageArray[i] = std::move(imageArray[indices[i]]);
        }

        imageDataArray = std::move(sortedDataArray);
        imageArray     = std::move(sortedImageArray);
    }


    uint64_t calcMaxColors(int indexBitDepth) {
        if (indexBitDepth >= 64) return UINT64_MAX;
        return (uint64_t)1 << static_cast<unsigned int>(indexBitDepth);
    }

    bool hasTransparency(int index){
        return hasTransparency(imageArray[index]);
    }

    bool hasTransparency(const cv::Mat& image) {
        if (image.empty())
            throw std::runtime_error("Image is empty");

        cv::Mat img4;
        if (image.channels() == 4)
            img4 = image;
        else if (image.channels() == 3)
            cv::cvtColor(image, img4, cv::COLOR_BGR2BGRA);
        else if (image.channels() == 1)
            cv::cvtColor(image, img4, cv::COLOR_GRAY2BGRA);
        else
            return false;

        for (int y = 0; y < img4.rows; ++y) {
            const uint8_t* row = img4.ptr<uint8_t>(y);

            for (int x = 0; x < img4.cols; ++x) {
                if (row[x * 4 + 3] != 255) {
                    return true;
                }
            }
        }

        return false;
    }

    std::string formatFileSize(uint32_t bytes) {
        if (bytes < 1024)
            return std::to_string(bytes) + " B";
        else if (bytes < 1024 * 1024)
            return std::to_string(bytes / 1024) + " KB";
        else
            return std::to_string(bytes / (1024 * 1024)) + " MB";
    }

    GLuint uploadThumbnail(const std::vector<unsigned char>& pngBytes) {
        cv::Mat img = cv::imdecode(pngBytes, cv::IMREAD_UNCHANGED); // preserve alpha on decode too
        if (img.empty()) return 0;

        if (img.channels() == 4)
            cv::cvtColor(img, img, cv::COLOR_BGRA2RGBA);
        else if (img.channels() == 3)
            cv::cvtColor(img, img, cv::COLOR_BGR2RGBA);
        else if (img.channels() == 1)
            cv::cvtColor(img, img, cv::COLOR_GRAY2RGBA);

        GLuint texID = 0;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    img.cols, img.rows, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, img.data);
        return texID;
    }

    GLuint uploadFullImage(const cv::Mat& image) {
        cv::Mat img;

        if (image.channels() == 4)
            cv::cvtColor(image, img, cv::COLOR_BGRA2RGBA);
        else if (image.channels() == 3)
            cv::cvtColor(image, img, cv::COLOR_BGR2RGBA);
        else if (image.channels() == 1)
            cv::cvtColor(image, img, cv::COLOR_GRAY2RGBA);
        else
            img = image.clone();

        GLuint texID = 0;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    img.cols, img.rows, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, img.data);
        return texID;
    }

    GLuint createCheckerboardTexture(int tileSize = 16, int tiles = 4) {
        int size = tileSize * tiles * 2; // total texture size
        std::vector<unsigned char> pixels(size * size * 3);
        for (int y = 0; y < size; y++) {
            for (int x = 0; x < size; x++) {
                bool light = ((x / tileSize) + (y / tileSize)) % 2 == 0;
                unsigned char val = light ? 204 : 128;
                int idx = (y * size + x) * 3;
                pixels[idx] = pixels[idx+1] = pixels[idx+2] = val;
            }
        }
        GLuint texID = 0;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAX_ANISOTROPY_EXT, 1.0f);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, size, size, 0, GL_RGB, GL_UNSIGNED_BYTE, pixels.data());
        return texID;
    }

    //return the highest unique color count
    FullImageData getMaxUniqueColors(){
        if (imageDataArray.empty()) throw std::runtime_error("No images loaded");

        uint32_t highestColorCount = 0;
        FullImageData highestImageCount; 
        for (const auto& imageData : imageDataArray) {
            if(imageData.imageTraits.uniqueColors > highestColorCount){
                highestImageCount = FullImageData{imageData.imageTraits,imageData.metaData};
                highestColorCount = imageData.imageTraits.uniqueColors;
            }
        }
        return highestImageCount;
    }

    int calcRequiredBitDepth(uint32_t uniqueColors) {
        if (uniqueColors <= 1)
            return 1;

        int bits = 0;
        uint32_t value = uniqueColors - 1;

        while (value > 0) {
            ++bits;
            value >>= 1;
        }

        return bits;
    }

    
};