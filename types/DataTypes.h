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
#include <vector>
#include <iostream>

class DataTypes{
    public:
    //for organizing separate progess bars
    //set in encoder before swapping to new class function; might be confusing with for loop
    //mark unused optional steps as skipped
    //use string for higher specificity
    enum EncodingPipelineSteps{
        ADJUSTING_IMAGES,
        SPLITTING_SCENES,
        BUILDING_PALETTES,
        QUANTIZING_PALETTES,
        DITHERING_FRAMES,//OPTIONAL
        FRAME_INDEXING,
        SORTING_PALETTES,
        SEQUENCING_FRAMES,
        ESTIMATING_MOTION,//OPTIONAL
        FILTERING_INDICES,//OPTIONAL
        ENTROPY_ENCODING,
        EXPORTING_FILE
    };

    enum ProgressStates{
        NOT_STARTED,
        IN_PROGRESS,
        FINISHED,
        SKIPPED
    };

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

    enum PaletteTypes{
        I_PALETTE,
        P_PALETTE
    };

    enum FrameTypes{
        I_FRAME,
        P_FRAME,
        B_FRAME
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

    //bit depth should be global and determined by header data
    struct Palette{
        PaletteTypes paletteType;
        uint32_t sceneIndex;
        std::vector<RGBA> colors;

        int getColorCount(){
            return colors.size();
        }
    };


    struct Scene {
        uint32_t startFrame; //First scene starts with 0
        uint32_t endFrame; //last frame in scene; use end frame to signal index increase to next scene
        Palette palette;

        // checks whether next frame is in scene
        bool hasNext(int currentFrame){
            return (currentFrame + 1 <= endFrame) && (currentFrame + 1 >= startFrame);
        }
    };

    //Store as vector with the index correspoding to each sequential enum state
    struct EncodingProgress{
        EncodingPipelineSteps EncodingPipelineStep;
        ProgressStates state = NOT_STARTED;
        float percentFinished = 0.0f;
    };

    static std::vector<EncodingProgress> makeProgressTracker() {
        std::vector<EncodingProgress> p;
        for (int i = 0; i <= EXPORTING_FILE; ++i) {
            p.push_back({ static_cast<EncodingPipelineSteps>(i), NOT_STARTED, 0.0f });
        }
        return p;
    }

    static void setProgressPercent(std::vector<EncodingProgress>& tracker, EncodingPipelineSteps step, float percent = 0.0f) {
        if(percent >= 1.0f){
            percent = 1.0f;
            setProgressState(tracker, step, FINISHED);
        }
        tracker[step].percentFinished = percent;
        
    }

    static void setProgressState(std::vector<EncodingProgress>& tracker, EncodingPipelineSteps step, ProgressStates state)
    {
        if (tracker[step].state == state)
            return;
        
        tracker[step].state = state;
        if(state == FINISHED) tracker[step].percentFinished = 1.0f;

        std::cout
            << "[Progress] "
            << stepToString(step)
            << " -> "
            << stateToString(state)
            << '\n';
    }

    static void skipStep(std::vector<EncodingProgress>& tracker, EncodingPipelineSteps step) {
        if (tracker[step].state == FINISHED)
            return;
        tracker[step].percentFinished = 0.0f;
        setProgressState(tracker, step, SKIPPED);
    }

    static void increaseProgressPercent(std::vector<EncodingProgress>& tracker, EncodingPipelineSteps step, uint32_t currentItem, uint32_t totalItems, float maxPercentIncrease)
    {
        float targetProgress =
            ((float)currentItem / totalItems) * maxPercentIncrease;

        tracker[step].percentFinished = targetProgress;
    }

    static const char* stateToString(ProgressStates state)
    {
        switch (state)
        {
            case NOT_STARTED: return "Not Started";
            case IN_PROGRESS: return "In Progress";
            case FINISHED:    return "Finished";
            case SKIPPED:     return "Skipped";
            default:          return "Unknown";
        }
    }

    static const char* stepToString(EncodingPipelineSteps step)
    {
        switch (step)
        {
            case ADJUSTING_IMAGES:    return "Adjusting Images";
            case SPLITTING_SCENES:    return "Splitting Scenes";
            case BUILDING_PALETTES:   return "Building Palettes";
            case QUANTIZING_PALETTES: return "Quantizing Palettes";
            case DITHERING_FRAMES:    return "Dithering Frames";
            case FRAME_INDEXING:      return "Frame Indexing";
            case SORTING_PALETTES:    return "Sorting Palettes";
            case SEQUENCING_FRAMES:   return "Sequencing Frames";
            case ESTIMATING_MOTION:   return "Estimating Motion";
            case FILTERING_INDICES:   return "Filtering Indices";
            case ENTROPY_ENCODING:    return "Entropy Encoding";
            case EXPORTING_FILE:      return "Exporting File";
            default:                  return "Unknown Step";
        }
    }

    //weight of steps in calculating total percent
    static constexpr std::array<float, EXPORTING_FILE + 1> PROGRESS_WEIGHTS = {
        0.05f, // ADJUSTING_IMAGES
        0.05f, // SPLITTING_SCENES
        0.10f, // BUILDING_PALETTES
        0.10f, // QUANTIZING_PALETTES
        0.05f, // DITHERING_FRAMES
        0.10f, // FRAME_INDEXING
        0.05f, // SORTING_PALETTES
        0.05f, // SEQUENCING_FRAMES
        0.15f, // ESTIMATING_MOTION
        0.05f, // FILTERING_INDICES
        0.20f, // ENTROPY_ENCODING
        0.05f  // EXPORTING_FILE
    };

    static float getTotalProgress(const std::vector<EncodingProgress>& encodeProgressList)
    {
        float totalProgress = 0.0f;
        for (size_t i = 0; i < encodeProgressList.size(); i++) {
            const auto& entry = encodeProgressList[i];
            if (entry.state == DataTypes::FINISHED || entry.state == DataTypes::SKIPPED)
                totalProgress += DataTypes::PROGRESS_WEIGHTS[i];
            else if (entry.state == DataTypes::IN_PROGRESS)
                totalProgress += DataTypes::PROGRESS_WEIGHTS[i] * entry.percentFinished;
        }
        return totalProgress;
    }

};

