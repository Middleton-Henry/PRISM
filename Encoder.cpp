/*
Top Level video encoding processor
takes image data and encodes into video format using settings
*/
#include "pipeline/ImageAdjuster.cpp"
#include "pipeline/SceneSplitter.cpp"
#include "pipeline/PaletteMapper.cpp"
#include "pipeline/Quantizer.cpp"


#include "types/DataTypes.cpp"

class Encoder{
    private:
    ImageAdjuster imageAdjuster;
    SceneSplitter sceneSplitter;

    Quantizer quantizer;



    public:
    //images need to be properly resized to match desired proportions with desired interpolation function
    //Also replace a==0 with alpha color replacement (maybe lerp based on alpha percent)


    

    std::vector<uint32_t> sceneFrameIndices; //list of indices pointing to the first frame of each scene
    

    int getNumberOfScenes(){
        return sceneFrameIndices.size();
    }

    void Encode(
        const std::vector<cv::Mat>& frames,
        const DataTypes::EncodingSettings settings,
        const std::filesystem::path& exportFilePath,
        std::function<void(float progress, std::string stage)> onProgress = nullptr){
        
        std::string message = "";
        
        // 0) Check that all images fit user-specified width and height
        //    → ImageAdjuster.cpp
        message = "Starting Resizing";
        std::cout << message << "\n"; 
        imageAdjuster.resizeImages(frames, settings.width, settings.height, settings.interpolationMode, onProgress);

        // 1) Scene cutoff detection (histogram difference)
        //    → SceneSplitter.cpp
        message = "Starting Scene Cuts";
        std::cout << message << "\n"; 
        sceneFrameIndices = sceneSplitter.SplitImagesIntoScenes(frames,onProgress);//need to create settings options for cutoff

        message = "Scene cuts: ";
        std::cout << message << "\n"; 
        for (const auto& num : sceneFrameIndices) {
            std::cout << (num+1) << " ";
        }
        std::cout << std::endl;
        

        // 2) Per-scene color collection → raw palette
        //    → PaletteMapper.cpp
        //    Scan all frames in scene, union their color sets into one palette
        message = "Starting Palette Generation";
        std::cout << message << "\n"; 

        // 3) Quantize/cluster palette to fit index bit depth
        //    → Quantizer.cpp
        //    This is where clustering, frequency protection, LAB-space ops happen
        //    Output: finalized palette with <= 2^indexBitDepth entries
        message = "Starting Palette Quantization";
        std::cout << message << "\n"; 

        // 4) [OPTIONAL] Dither frames against quantized palette (frames might need to be index first)
        //    → Ditherer.cpp
        //    Must happen on original color frames before index conversion
        //    Bayer matrix or spatio-temporal

        // 5) Convert frames to index frames using finalized palette
        //    → PaletteMapper.cpp
        //    Each pixel RGBA → index into palette
        message = "Starting Frame indexing";
        std::cout << message << "\n"; 

        // 6) Sort and split palettes into I-palettes and P-palettes
        //    → PaletteSorter.cpp 
        //    I-palette: sorted for spatial prediction (luminance/LAB/Hilbert)
        //    P-palette: sorted to minimize delta from previous palette (Auction/Hungarian)
        message = "Starting Palette sorting";
        std::cout << message << "\n"; 

        // 7) Arrange frames into I/P/B order
        //    → FrameSequencer.cpp 
        //    Builds IBBPBBP sequences, inserts I-frames at scene cutoffs
        message = "Starting Frame sorting";
        std::cout << message << "\n"; 

        // 8) [OPTIONAL] Motion vector calculation on index frames
        //    → MotionEstimator.cpp
        //    Block matching on recolored index frames
        //    Outputs per-block MVs, residuals, skip flags

        // 9) [OPTIONAL] Spatial prediction on I-frames
        //    → SpatialPredictor.cpp
        //    PNG-style filter passes on index frames

        // 10) Entropy encode everything
        //     → RLE.cpp then ANSCoder.cpp
        //     Index streams, MV deltas, skip flags, palette data
        message = "Starting Entropy Encoding";
        std::cout << message << "\n";

        // 11) Stream write to disk
        //     → FileWriter.cpp
        message = "Starting Write File to Disk";
        std::cout << message << "\n";

    }
    
};