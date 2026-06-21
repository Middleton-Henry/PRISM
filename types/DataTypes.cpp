/*
Custom structs and enums
Palette
IndexFrame
MotionVector
EncodedBlock
*/
#pragma once
#include <cstdint>
#include <array>

class DataTypes{
    public:
    enum InterpolationModes{
        NEAREST_NEIGHBOR,
        BILINEAR,
        CUBIC
    };

    enum PPaletteSortingSolvers{
        HUNGARIAN,
        AUCTION,
        NEAREST_NEIGHBOR_SNAPPING
    };

    enum IPaletteSortingSystems{
        GREATEST_FREQUENCY, 
        GREATEST_LUMINANCE, 
        HILBERT_CURVE
    };

    struct EncodingSettings {
        uint32_t             width;
        uint32_t             height;
        uint32_t             frameRate; //might use map for adaptive framerate per frame insetad of fixed
        int                  indexBitDepth;
        bool                 supportsAlpha;
        std::array<float, 3> alphaReplacement;
        bool                 dithered;
        bool                 motionVectorPrediction;
        bool                 filteringPrediction;

        InterpolationModes     interpolationMode;
        IPaletteSortingSystems iPaletteSorter;
        PPaletteSortingSolvers pPaletteSolver;
    };
    //might incorporate option for premultiplied vs non-premultiplied alpha

    struct RGB {
        uint8_t r, g, b;
    };
    struct RGBA {
        uint8_t r, g, b, a;
    };

    struct Lab {
        double L; // 0 to 100
        double a; // -128 to 127
        double b; // -128 to 127
    };

    struct LabA {
        double L;
        double a;
        double b;
        double A; 
    };

    

    
};

