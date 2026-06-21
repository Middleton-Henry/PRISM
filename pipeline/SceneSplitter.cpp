#pragma once
#include <array>
#include <vector>
#include <functional>
#include <opencv2/core.hpp>
#include "../types/DataTypes.cpp"
#include "../types/ColorUtils.h"
#include <iostream>

template<int BINS = 32>

class SceneSplitter{
    static_assert(BINS > 0 && (BINS & (BINS - 1)) == 0,
        "BINS must be a power of 2 (e.g. 8, 16, 32, 64)");
    public:
    struct FrameHistogram {
        std::array<float, BINS> L;
        std::array<float, BINS> a;
        std::array<float, BINS> b;
    };

    FrameHistogram ComputeHistogram(const cv::Mat& frame) {
        FrameHistogram hist{};
        hist.L.fill(0); hist.a.fill(0); hist.b.fill(0);

        int counted = 0;  // ← track non-transparent pixels only

        for (int y = 0; y < frame.rows; ++y) {
            for (int x = 0; x < frame.cols; ++x) {
                auto px = frame.at<cv::Vec4b>(y, x);
                if (px[3] == 0) continue;

                DataTypes::Lab lab = ColorUtils::RGBToLab({px[0], px[1], px[2]});
                int Lbin = std::clamp((int)(lab.L / 100.0 * BINS), 0, 31);
                int abin = std::clamp((int)((lab.a + 128.0) / 255.0 * BINS), 0, 31);
                int bbin = std::clamp((int)((lab.b + 128.0) / 255.0 * BINS), 0, 31);
                hist.L[Lbin]++;
                hist.a[abin]++;
                hist.b[bbin]++;
                counted++;  // ← only increment for visible pixels
            }
        }

        // Guard against fully transparent frames
        if (counted == 0) return hist;

        for (int i = 0; i < BINS; ++i) {
            hist.L[i] /= counted;  // ← divide by counted, not total
            hist.a[i] /= counted;
            hist.b[i] /= counted;
        }

        return hist;
    }

    float BhattacharyyaChannel(const std::array<float,BINS>& h1, const std::array<float,BINS>& h2) {
        float bc = 0.0f;
        for (int i = 0; i < BINS; ++i)
            bc += std::sqrt(h1[i] * h2[i]);
        // Returns distance in [0, 1]; 0 = identical, 1 = no overlap
        return std::sqrt(1.0f - bc);
    }

    float HistogramDistance(const FrameHistogram& h1, const FrameHistogram& h2) {
        float dL = BhattacharyyaChannel(h1.L, h2.L);
        float da = BhattacharyyaChannel(h1.a, h2.a);
        float db = BhattacharyyaChannel(h1.b, h2.b);

        // Weight L more heavily — luminance changes are the strongest scene cut signal
        return 0.50f * dL + 0.25f * da + 0.25f * db;
    }

    

    std::vector<uint32_t> SplitImagesIntoScenes(const std::vector<cv::Mat>& frames, float cutoff = 0.6f, std::function<void(float, std::string)> onProgress  = nullptr){
        FrameHistogram cachedHistogram; //either compare to previous histogram or from start of scene
        std::vector<uint32_t> sceneFrameIndices;
        sceneFrameIndices.push_back(0); // frame 0 is always a scene start

        cachedHistogram = ComputeHistogram(frames[0]);

        for (int i = 1; i < (int)frames.size(); ++i) {
            FrameHistogram hist = ComputeHistogram(frames[i]);
            float dist = HistogramDistance(cachedHistogram, hist);

            if (dist > cutoff){
                sceneFrameIndices.push_back(i);
                cachedHistogram = hist;
            }
                


            if (onProgress)
                onProgress((float)(i + 1) / frames.size(), "");
        }

        return sceneFrameIndices;
    }

};