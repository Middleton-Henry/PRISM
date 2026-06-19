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

class ImageProcessor{
    public:
    struct ImageTraits {
        uint16_t width = 0;
        uint16_t height = 0;
        uint32_t uniqueColors = 0;
    };

    struct ImageStructure {
        std::unordered_map<std::string, int> colorFrequency;
    };

    struct MetaData {
        std::string name;
        std::filesystem::file_time_type dateCreated;
        std::filesystem::file_time_type dateLastModified;
        uint32_t uploadIndex = 0;
        uint32_t size = 0; //file size
        //std::vector<unsigned char> imageThumbnail; //keep at very low resolution for ui display
        GLuint thumbnailTextureID = 0;
    };

    struct FullImageData {
        ImageTraits imageTraits; //low cost information for basic analysis
        ImageStructure imageStructure; //high cost information for operations
        MetaData metaData; //information for non operational needs
    };

    struct AnalysisImageData{
        ImageTraits imageTraits; //low cost information for basic analysis
        MetaData metaData; //information for non operational needs
    };

    struct ComputationalImageData{
        ImageTraits imageTraits; //low cost information for basic analysis
        ImageStructure imageStructure; //high cost information for operations
    };

    std::vector<FullImageData> imageDataArray;
    std::vector<cv::Mat> imageArray;
    uint32_t uploadIndex = 0; 

    enum SortingSystems{
        UPLOAD_ORDER,
        NAME,
        DATE_MODIFIED,
        DATE_CREATED
    };

    enum InterpolationModes{
        NEAREST_NEIGHBOR,
        BILINEAR,
        CUBIC
    };

    enum PaletteSortingSolvers{
        HUNGARIAN,
        AUCTION,
        NEAREST_NEIGHBOR_SNAPPING
    };

    FullImageData processImage(const cv::Mat& image) {
        FullImageData imageData;
        imageData.imageTraits.width  = static_cast<uint16_t>(image.cols);
        imageData.imageTraits.height = static_cast<uint16_t>(image.rows);
        return imageData;
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

        cv::Mat image = cv::imread(std::string(filePath), cv::IMREAD_COLOR);
        if (image.empty())
            throw std::runtime_error("Failed to load image: " + std::string(filePath));

        namespace fs = std::filesystem;
        fs::path path(filePath);

        imageDataArray[index] = processImage(image);
        imageDataArray[index].metaData.name           = path.filename().string();
        imageDataArray[index].metaData.dateLastModified    = fs::last_write_time(path);
        imageDataArray[index].metaData.dateCreated    = fs::last_write_time(path);

        std::error_code ec;
        imageDataArray[index].metaData.size = std::filesystem::file_size(filePath, ec);
        
        //imageDataArray[index].metaData.imageThumbnail = createThumbnail(image);
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
        cv::imencode(".jpg", thumbnail, buffer);
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

    void resortImages(SortingSystems sortOrder) {
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

    GLuint uploadThumbnail(const std::vector<unsigned char>& jpegBytes) {
        cv::Mat img = cv::imdecode(jpegBytes, cv::IMREAD_COLOR);
        if (img.empty()) return 0;

        cv::cvtColor(img, img, cv::COLOR_BGR2RGBA);  // OpenGL expects RGBA

        GLuint texID = 0;
        glGenTextures(1, &texID);
        glBindTexture(GL_TEXTURE_2D, texID);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                    img.cols, img.rows, 0,
                    GL_RGBA, GL_UNSIGNED_BYTE, img.data);
        return texID;
    }

    
};