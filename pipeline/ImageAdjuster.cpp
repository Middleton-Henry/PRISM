#pragma once
#include "../types/DataTypes.h"
#include <array>
#include <vector>
#include <opencv2/core.hpp>
#include <opencv2/opencv.hpp>


class ImageAdjuster{
    public:

    void replaceAlpha(const std::vector<cv::Mat>& frames, std::array<float, 3> alphaReplacement, std::vector<DataTypes::EncodingProgress>* encodeProgressList = nullptr, float maxPercentIncrease = 0.5f){
        int i = 0;
        //iterate through every pixel and replace full transparent (a=0) with alphaReplacement
        if (encodeProgressList){
            DataTypes::increaseProgressPercent(*encodeProgressList,DataTypes::ADJUSTING_IMAGES,i + 1,frames.size(),maxPercentIncrease);
            i++;
        }
    }

    void resizeImages(const std::vector<cv::Mat>& frames, uint32_t width, uint32_t height, DataTypes::InterpolationModes interpolationMode = DataTypes::BILINEAR, std::vector<DataTypes::EncodingProgress>* encodeProgressList = nullptr, float maxPercentIncrease = 0.5f) {
        std::vector<cv::Mat> resizedFrames;
        resizedFrames.reserve(frames.size());

        // Map your custom interpolation mode to OpenCV flags
        int cvInterpolationFlag = cv::INTER_LINEAR; // Default fallback
        switch (interpolationMode) {
            case DataTypes::NEAREST_NEIGHBOR:
                cvInterpolationFlag = cv::INTER_NEAREST;
                break;
            case DataTypes::BILINEAR:
                cvInterpolationFlag = cv::INTER_LINEAR;
                break;
            case DataTypes::CUBIC:
                cvInterpolationFlag = cv::INTER_CUBIC;
                break;
        }
        int i = 0;
        for (const auto& frame : frames) {
            if (frame.empty()) continue;

            cv::Mat resizedFrame;
            cv::resize(frame, resizedFrame, cv::Size(width, height), 0, 0, cvInterpolationFlag);
            
            resizedFrames.push_back(resizedFrame);

            if (encodeProgressList){
                DataTypes::increaseProgressPercent(*encodeProgressList,DataTypes::ADJUSTING_IMAGES,i + 1,frames.size(),maxPercentIncrease);
                i++;
            }
                
        }
        
        // resizedFrames now contains all the successfully resized images
    }

};