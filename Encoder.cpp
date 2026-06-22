/*
Top Level video encoding processor
takes image data and encodes into video format using settings
*/
#include "pipeline/ImageAdjuster.cpp"
#include "pipeline/SceneSplitter.cpp"
#include "pipeline/PaletteBuilder.cpp"
#include "pipeline/Quantizer.cpp"


#include "types/DataTypes.h"

class Encoder{
    private:
    ImageAdjuster imageAdjuster;
    SceneSplitter<16> sceneSplitter;
    PaletteBuilder paletteBuilder;
    Quantizer quantizer;

    


    public:
    //images need to be properly resized to match desired proportions with desired interpolation function
    //Also replace a==0 with alpha color replacement (maybe lerp based on alpha percent)
    


    void Encode(
        const std::vector<cv::Mat>& originalFrames,
        const DataTypes::EncodingSettings settings,
        const std::filesystem::path& exportFilePath,
        std::vector<DataTypes::EncodingProgress>* encodeProgressList = nullptr){
        
        //variables
        std::vector<cv::Mat> frames = originalFrames; //keep original frames intact
        std::vector<uint32_t> sceneFrameIndices; //list of indices pointing to the first frame of each scene
        std::unordered_map<uint32_t, uint32_t> colorFrequency; //list of colors with frequency count for an individual scene
        std::vector<DataTypes::Scene> sceneList;

        std::string message = "";
        
        // 0) Check that all images fit user-specified width and height and fill in for alpha
        //    → ImageAdjuster.cpp
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::ADJUSTING_IMAGES,DataTypes::IN_PROGRESS);
        if(!settings.supportsAlpha) imageAdjuster.replaceAlpha(frames, settings.alphaReplacement, encodeProgressList);
        imageAdjuster.resizeImages(frames, settings.width, settings.height, settings.interpolationMode, encodeProgressList);
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::ADJUSTING_IMAGES,DataTypes::FINISHED);

        // 1) Scene cutoff detection (histogram difference)
        //    → SceneSplitter.cpp
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::SPLITTING_SCENES,DataTypes::IN_PROGRESS);
        sceneFrameIndices = sceneSplitter.SplitImagesIntoScenes(frames,0.5f, encodeProgressList);//need to create settings options for cutoff
        sceneSplitter.populateSceneList(sceneList, sceneFrameIndices, frames.size(), encodeProgressList);// resizes sceneList and fills with frame data (start and end)
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::SPLITTING_SCENES,DataTypes::FINISHED);


        message = "Scene cuts: ";
        std::cout << message << "\n"; 
        for (const auto& num : sceneFrameIndices) {
            std::cout << (num+1) << " ";
        }
        std::cout << std::endl;

        message = "Starting Scene per Scene Processing";
        std::cout << message << "\n";
        
        //steps 2,3,4, and 5 need to be done on a scene per scene basis so that memory is not used for all frames simultaneously
        //colorFrequency will be replaced so all relevant calculations need to be done here
        for (int i = 0; i < static_cast<int>(sceneList.size()); i++) {
            std::cout << "Scene " << i+1 << " Frame " << sceneFrameIndices[i] << "\n\n";
        
            
            // 2) Per-scene color collection → raw palette
            //    → PaletteBuilder.cpp
            //    Scan all frames in scene, union their color sets into one palette
            if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::BUILDING_PALETTES,DataTypes::IN_PROGRESS);
            
            

            // 3) Quantize/cluster palette to fit index bit depth
            //    → Quantizer.cpp
            //    This is where clustering, frequency protection, LAB-space ops happen
            //    Output: finalized palette with <= 2^indexBitDepth entries
            if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::QUANTIZING_PALETTES,DataTypes::IN_PROGRESS);
            

            // 4) [OPTIONAL] Dither frames against quantized palette (frames might need to be index first)
            //    → Ditherer.cpp
            //    Must happen on original color frames before index conversion
            //    Bayer matrix or spatio-temporal
            if(settings.dithered) if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::DITHERING_FRAMES,DataTypes::IN_PROGRESS);
            if(!settings.dithered) if (encodeProgressList) DataTypes::skipStep(*encodeProgressList,DataTypes::DITHERING_FRAMES);
            

            // 5) Convert frames to index frames using finalized palette
            //    → FrameIndexer.cpp
            //    Each pixel RGBA → index into palette
            if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::FRAME_INDEXING,DataTypes::IN_PROGRESS);
            

        }
        //[REDUNDANCY] should already be set to finished during percent check
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::BUILDING_PALETTES,DataTypes::FINISHED);
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::QUANTIZING_PALETTES,DataTypes::FINISHED);
        if(settings.dithered) if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::DITHERING_FRAMES,DataTypes::FINISHED);
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::FRAME_INDEXING,DataTypes::FINISHED);

        std::cout << "\n\n";

        // 6) Sort and split palettes into I-palettes and P-palettes
        //    → PaletteSorter.cpp 
        //    I-palette: sorted for spatial prediction (luminance/LAB/Hilbert)
        //    P-palette: sorted to minimize delta from previous palette (Auction/Hungarian)
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::SORTING_PALETTES,DataTypes::IN_PROGRESS);
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::SORTING_PALETTES,DataTypes::FINISHED);

        // 7) Arrange frames into I/P/B order
        //    → FrameSequencer.cpp 
        //    Builds IBBPBBP sequences, inserts I-frames at scene cutoffs
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::SEQUENCING_FRAMES,DataTypes::IN_PROGRESS);
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::SEQUENCING_FRAMES,DataTypes::FINISHED);

        // 8) [OPTIONAL] Motion vector calculation on index frames
        //    → MotionEstimator.cpp
        //    Block matching on recolored index frames
        //    Outputs per-block MVs, residuals, skip flags
        if(settings.motionVectorPrediction){
            if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::ESTIMATING_MOTION,DataTypes::IN_PROGRESS);
            if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::ESTIMATING_MOTION,DataTypes::FINISHED);
        }
        else{
            if (encodeProgressList) DataTypes::skipStep(*encodeProgressList,DataTypes::ESTIMATING_MOTION);
        }

        // 9) [OPTIONAL] Spatial prediction on I-frames
        //    → SpatialPredictor.cpp
        //    PNG-style filter passes on index frames
        if(settings.filteringPrediction){
            if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::FILTERING_INDICES,DataTypes::IN_PROGRESS);
            if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::FILTERING_INDICES,DataTypes::FINISHED);
        }
        else{
            if (encodeProgressList) DataTypes::skipStep(*encodeProgressList,DataTypes::FILTERING_INDICES);
        }

        // 10) Entropy encode everything
        //     → RLE.cpp then ANSCoder.cpp
        //     Index streams, MV deltas, skip flags, palette data
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::ENTROPY_ENCODING,DataTypes::IN_PROGRESS);
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::ENTROPY_ENCODING,DataTypes::FINISHED);

        // 11) Stream write to disk
        //     → FileWriter.cpp
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::EXPORTING_FILE,DataTypes::IN_PROGRESS);
        if (encodeProgressList) DataTypes::setProgressState(*encodeProgressList,DataTypes::EXPORTING_FILE,DataTypes::FINISHED);

    }
    
};