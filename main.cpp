// Dear ImGui - macOS Example
// Backend: GLFW + OpenGL3 (Metal-compatible via OpenGL)
//
// Requirements (install via Homebrew):
//   brew install glfw
//
// Dear ImGui source (clone into ./imgui/):
//   git clone https://github.com/ocornut/imgui.git
//
// Build: see tasks.json in .vscode/

#include "imgui/imgui.h"
#include "imgui/backends/imgui_impl_glfw.h"
#include "imgui/backends/imgui_impl_opengl3.h"

#include <GLFW/glfw3.h>
#include <cstdio>
#include <string>
#include <vector>
#include <locale>
#include <sstream>
#include <thread>

#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "image_processor.cpp"
#include "types/DataTypes.h"
#include "Encoder.cpp"


// ── helpers ──────────────────────────────────────────────────────────────────

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "GLFW Error %d: %s\n", error, description);
}

std::string formatWithCommas(uint64_t value) {
    std::string s = std::to_string(value);
    int insertPos = (int)s.length() - 3;
    while (insertPos > 0) {
        s.insert(insertPos, ",");
        insertPos -= 3;
    }
    return s;
}

// ── style ─────────────────────────────────────────────────────────────────────
void myStyle(){
    ImGuiStyle& style = ImGui::GetStyle();
    //style.FrameRounding = 0.0f;
    //style.Colors[ImGuiCol_Button] = ImVec4(0.8f,0.2f,0.2f,255.0f);
}

// ── main ─────────────────────────────────────────────────────────────────────

int main(int argc, char* argv[])
{
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    // macOS requires forward-compatible core profile
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1280, 720, "Application Name", nullptr, nullptr);
    if (!window) { glfwTerminate(); return 1; }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // vsync

    // ── ImGui setup ──────────────────────────────────────────────────────────
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150"); // GLSL 1.50 = GL 3.2


    // ── Variables ──────────────────────────────────────────────────────────
    Encoder encoder;
    ImageProcessor imageProcessor;
    DataTypes::EncodingSettings cachedSettings; //most recent encoding settings; use for presenting relevant progress bars
    std::vector<DataTypes::EncodingProgress> encodeProgressList = DataTypes::makeProgressTracker();
    std::map<std::string, std::string> selectedFiles;
    unsigned int width = 1080; //set to first image upon upload
    unsigned int height = 1920; //set to first image upon upload

    const char* interpolationModes[] = { "Nearest Neighbor (Pixelated)", "Bilinear (Blurred)", "Cubic (Sharp)"};
    DataTypes::InterpolationModes currentInterpolationMode = DataTypes::NEAREST_NEIGHBOR;

    unsigned int frameRate = 12;
    int indexBitDepth = 24; //max 32 if including all alpha variations
    uint64_t maxColors = imageProcessor.calcMaxColors(indexBitDepth);

    int currentPage = 0;
    const int itemsPerPage = 25;
    int currentPageDisplay = 1;

    const char* sortingSystems[] = { "Upload Order (First to Last)", "Name (A to Z)", "Date Modified (Oldest to Newest)", "Date Created (Oldest to Newest)"};
    

    ImageProcessor::ImageSortingSystems currentSortingOrder = ImageProcessor::UPLOAD_ORDER;

    bool supportsAlpha = true;

    static float alphaReplacement[3] = { 1.0f, 0.0f, 1.0f };

    bool isDithered = false;

    const char* pPaletteSortingSolvers[] = { "Hungarian Solver (High)", "Auction Solver (Mid)", "Nearest Neighbor Snapping (Low)"};
    DataTypes::PPaletteSortingSolvers currentPPaletteSolver = DataTypes::AUCTION;

    const char* iPaletteSortingSystems[] = {"Greatest Frequency", "Greatest Luminance", "Hilbert Curve"};
    DataTypes::IPaletteSortingSystems currentIPaletteSorter = DataTypes::GREATEST_LUMINANCE;

    GLuint hoveredTex = 0;
    int hoveredIndex = -1;
    bool previewUsedThisFrame = false;
    bool showCheckerboard = false;

    ImageProcessor::FullImageData mostUniqueColorImageData;

    double indexPercentCoverage = 0;

    bool motionVectors = false;
    bool filtering = false; //PNG spacial prediction algorithm


    std::filesystem::path exportFolderPath = std::filesystem::canonical(
    std::filesystem::path(argv[0]).parent_path());
    std::string exportFolderPathDisplay = "";
    bool encodeDialogOpen = false;
    std::vector<cv::Mat> pendingEncodeFrames;
    DataTypes::EncodingSettings pendingEncodeSettings;
    bool isEncoding = false;
    bool encodingFinished = false;

    

    // ── UI Graphics Init ──────────────────────────

    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    GLuint checkerboardTex = imageProcessor.createCheckerboardTexture(128, 8);

    // ── Render loop ──────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start a new frame context
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ── UI Graphics Loop ──────────────────────────
        previewUsedThisFrame = false;
        ImVec2 screenSize = ImGui::GetIO().DisplaySize;
        

        myStyle();
        ImGui::SetNextWindowSize(ImVec2(screenSize.x/3.0f,screenSize.y), ImGuiCond_Once);
        ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Once);
        if(ImGui::Begin("Encoding Settings")){
            if (ImGui::Button("Open Images")) {
                IGFD::FileDialogConfig config;
                config.path = ".";
                config.countSelectionMax = 0;
                ImGuiFileDialog::Instance()->OpenDialog(
                    "ChooseFileDlg",
                    "Choose Images",
                    ".png,.jpg,.jpeg,.bmp,.tga",
                    config
                );
            }
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Supported File Types:\n.png (recommended)\n.jpg\n.jpeg\n.bmp\n.tga");
                ImGui::EndTooltip();
            }

            float availableWidth = ImGui::GetContentRegionAvail().x;
            float spacing = ImGui::GetStyle().ItemSpacing.x;
            float entryWidth = (availableWidth - spacing) / 6.0f;
            float dropDownWidth = (availableWidth - spacing) * 0.62f;
            if (ImGui::CollapsingHeader("Basic Settings"))
            {
                //Sorting option dropdown
                ImGui::PushItemWidth(dropDownWidth);
                if(ImGui::BeginCombo("Frame Sorter", sortingSystems[currentSortingOrder])){
                    for (int i = 0; i < std::size(sortingSystems); i++) 
                    {
                        bool isSelected = (currentSortingOrder == i);

                        if (ImGui::Selectable(sortingSystems[i], isSelected)) 
                        {
                            ImageProcessor::ImageSortingSystems newSortingOrder = static_cast<ImageProcessor::ImageSortingSystems>(i);
                            if(currentSortingOrder != newSortingOrder) imageProcessor.resortImages(newSortingOrder);
                            currentSortingOrder = newSortingOrder;
                        }
                        if (isSelected) 
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Default image encoding order");
                    ImGui::EndTooltip();
                }
                ImGui::PopItemWidth();

                
                

                //Resolution Entry
                ImGui::PushItemWidth(entryWidth);
                
                if(ImGui::InputScalar("##px_width", ImGuiDataType_U32, &width)){

                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Width in pixels");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                ImGui::Text(" X ");
                ImGui::SameLine();
                if(ImGui::InputScalar("##px_height", ImGuiDataType_U32, &height)){

                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Height in pixels");
                    ImGui::EndTooltip();
                }
                ImGui::SameLine();
                ImGui::Text("Resolution");
                
                ImGui::PopItemWidth();

                

                //Interpolation mode dropdown
                ImGui::PushItemWidth(dropDownWidth);
                if(ImGui::BeginCombo("Interpolation Mode", interpolationModes[currentInterpolationMode])){
                    for (int i = 0; i < std::size(interpolationModes); i++) 
                    {
                        bool isSelected = (currentInterpolationMode == i);

                        if (ImGui::Selectable(interpolationModes[i], isSelected)) 
                        {
                            DataTypes::InterpolationModes newInterpolationMode = static_cast<DataTypes::InterpolationModes>(i);
                            currentInterpolationMode = newInterpolationMode;
                        }
                        if (isSelected) 
                        {
                            ImGui::SetItemDefaultFocus();
                        }
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Only reinterpolates if frame does not match desired resolution\nNearest - Recommended for pixel art\nBilinear - Recommended for soft shading and cel shading with anti-aliasing");
                    ImGui::EndTooltip();
                }
                ImGui::PopItemWidth();

                ImGui::PushItemWidth(entryWidth/2.0f);
                if(ImGui::InputScalar("Frames per Second", ImGuiDataType_U32, &frameRate)){

                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("On Ones - 24 fps\nOn Twos - 12 fps\nOn Threes - 6 fps");
                    ImGui::EndTooltip();
                }
                ImGui::PopItemWidth();

                ImGui::PushItemWidth(dropDownWidth);
                if(ImGui::SliderInt("Index Bit Depth", &indexBitDepth, 1, 32)){
                    maxColors = imageProcessor.calcMaxColors(indexBitDepth);
                    indexPercentCoverage = (double)maxColors / (double)mostUniqueColorImageData.imageTraits.uniqueColors * 100.0;
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Limits the number of unique colors per individual frame\nLower Index Bit Depth is more compressed but less accurate\nFull RGB - 24 bit depth\nFull RGBA - 32 bit depth");
                    ImGui::EndTooltip();
                }
                ImGui::PopItemWidth();
                
                std::string colorVariants = "Max Color Variations per Frame: " + formatWithCommas(maxColors);
                ImGui::Text("%s", colorVariants.c_str());

                ImGui::Checkbox("Alpha Support", &supportsAlpha);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Checks frames for alpha");
                    ImGui::EndTooltip();
                }

                if (!supportsAlpha)
                {
                    ImGui::ColorEdit3("Alpha Replacement", alphaReplacement);
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::Text("Replacement Color for solid alpha");
                        ImGui::EndTooltip();
                    }
                }
            }


            

            if (ImGui::CollapsingHeader("Advanced Settings"))
            {
                ImGui::Checkbox("Dither Image", &isDithered);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Dithered image is perceptually more accurate but increases file size");
                    ImGui::EndTooltip();
                }

                ImGui::PushItemWidth(dropDownWidth);
                if (ImGui::BeginCombo("P-Palette Sorting", pPaletteSortingSolvers[currentPPaletteSolver]))
                {
                    for (int i = 0; i < std::size(pPaletteSortingSolvers); i++)
                    {
                        bool isSelected = (currentPPaletteSolver == i);

                        if (ImGui::Selectable(pPaletteSortingSolvers[i], isSelected))
                            currentPPaletteSolver =
                                static_cast<DataTypes::PPaletteSortingSolvers>(i);

                        if (isSelected)
                            ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Algorithm for sorting color palettes\nHungarian - High Accuracy / High Memory and Performance Cost\nAuction - Mid Accuracy / Mid Memory and Performance Cost\nNearest Neighbor - Low Accuracy / Low Memory and Performance Cost");
                    ImGui::EndTooltip();
                }


                if (ImGui::BeginCombo("I-Palette Sorting", iPaletteSortingSystems[currentIPaletteSorter]))
                {
                    for (int i = 0; i < std::size(iPaletteSortingSystems); i++)
                    {
                        bool isSelected = (currentIPaletteSorter == i);

                        if (ImGui::Selectable(iPaletteSortingSystems[i], isSelected)) currentIPaletteSorter = static_cast<DataTypes::IPaletteSortingSystems>(i);

                        if (isSelected) ImGui::SetItemDefaultFocus();
                    }
                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ImGui::Checkbox("Motion Vector Prediction", &motionVectors);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Reduce file size by predicting movement of indices\nNot recommended for use with Filtering Prediction");
                    ImGui::EndTooltip();
                }

                ImGui::Checkbox("Filtering Prediction", &filtering);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Reduce file size by predicting neighboring indices\nNot recommended with Motion Vector Prediction");
                    ImGui::EndTooltip();
                }
            }
            if (ImGui::CollapsingHeader("Encoding Setup"))
            {
                // ── Output file selector ──────────────────────────────────────────────────
                if (ImGui::Button("Select Output File")) {
                    IGFD::FileDialogConfig exportConfig;
                    exportConfig.path     = exportFolderPath.parent_path().string();
                    exportConfig.fileName = exportFolderPath.filename().string();
                    ImGuiFileDialog::Instance()->OpenDialog(
                        "ExportFileDlg",
                        "Save Encoded File",
                        nullptr,
                        exportConfig
                    );
                }
                
                ImGui::SameLine();
                if (exportFolderPathDisplay.empty()) {
                    ImGui::TextDisabled("No output folder selected");
                } else {
                    ImGui::TextUnformatted(exportFolderPath.filename().string().c_str());
                    if (ImGui::IsItemHovered()) {
                        ImGui::BeginTooltip();
                        ImGui::TextUnformatted(exportFolderPathDisplay.c_str());
                        ImGui::EndTooltip();
                    }
                }
                

                ImGui::Spacing();

                // ── Encode button ─────────────────────────────────────────────────────────
                bool canEncode = !imageProcessor.imageDataArray.empty() && !exportFolderPathDisplay.empty() && !isEncoding;

                ImGui::BeginDisabled(!canEncode);
                if (ImGui::Button("Encode Images")) {
                    cachedSettings.width                  = width;
                    cachedSettings.height                 = height;
                    cachedSettings.frameRate              = frameRate;
                    cachedSettings.indexBitDepth          = indexBitDepth;
                    cachedSettings.supportsAlpha          = supportsAlpha;
                    cachedSettings.alphaReplacement       = { alphaReplacement[0], alphaReplacement[1], alphaReplacement[2] };
                    cachedSettings.dithered               = isDithered;
                    cachedSettings.motionVectorPrediction = motionVectors;
                    cachedSettings.filteringPrediction    = filtering;
                    cachedSettings.interpolationMode      = currentInterpolationMode;
                    cachedSettings.iPaletteSorter         = currentIPaletteSorter;
                    cachedSettings.pPaletteSolver         = currentPPaletteSolver;

                    std::vector<cv::Mat> frames;
                    frames.reserve(imageProcessor.imageArray.size());
                    for (const auto& img : imageProcessor.imageArray)
                        frames.push_back(img);

                    isEncoding     = true;
                    std::thread([&, frames, cachedSettings, exportFolderPath]() mutable {
                        encoder.Encode(
                            frames,
                            cachedSettings,
                            exportFolderPath,
                            &encodeProgressList
                        );
                        isEncoding = false;
                        encodingFinished = true;
                    }).detach();
                }
                ImGui::EndDisabled();

                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
                    ImGui::BeginTooltip();
                    if (imageProcessor.imageDataArray.empty())
                        ImGui::Text("No images loaded");
                    else if (exportFolderPathDisplay.empty())
                        ImGui::Text("No output folder selected");
                    else if (isEncoding)
                        ImGui::Text("Encoding in progress...");
                    else
                        ImGui::Text("Begin encoding with current settings");
                    ImGui::EndTooltip();
                }

                
            }
        }
        ImGui::End();

        // ── File list window ─────────────
        if (!selectedFiles.empty()) {
            ImGui::SetNextWindowSize(ImVec2(screenSize.x/2.0f, screenSize.y*(1.0f/3.0f)), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(screenSize.x/3.0f, 0), ImGuiCond_Once);
            if (ImGui::Begin("Selected Files")) {
                int totalItems = (int)imageProcessor.imageDataArray.size();
                int totalPages = (totalItems + itemsPerPage - 1) / itemsPerPage;

                if (currentPage >= totalPages) {
                    currentPage = totalPages - 1;
                    currentPageDisplay = currentPage + 1;
                }

                int pageStart = currentPage * itemsPerPage;
                int pageEnd   = std::min(pageStart + itemsPerPage, totalItems);
                // ── Fixed pagination bar ──────────────────────────────
                ImGui::BeginDisabled(currentPage == 0);
                if (ImGui::Button("< Prev")){
                    --currentPage;
                    currentPageDisplay = currentPage + 1;
                }
                ImGui::EndDisabled();
                ImGui::SameLine();
                ImGui::Text("Page");
                ImGui::SameLine();
                ImGui::SetNextItemWidth(50.0f);
                if(ImGui::InputScalar("##myScalar", ImGuiDataType_S32, &currentPageDisplay)){
                    currentPage = currentPageDisplay - 1;
                    if (currentPage >= totalPages) currentPage = totalPages - 1;
                    if (currentPage < 0) currentPage = 0;
                    currentPageDisplay = currentPage + 1;
                }
                ImGui::SameLine();
                ImGui::Text("/ %d  (%d files)", totalPages, totalItems);
                ImGui::SameLine();
                ImGui::BeginDisabled(currentPage >= totalPages - 1);
                if (ImGui::Button("Next >")){
                    ++currentPage;
                    currentPageDisplay = currentPage + 1;
                }
                ImGui::EndDisabled();

                ImGui::Separator();

                // ── Column headers ───────────────────────────────────────────────────────────
                ImGui::Columns(8, "file_columns", false);
                ImGui::SetColumnWidth(0, 70.0f);   // index input
                ImGui::SetColumnWidth(1, 70.0f);   // up/down buttons
                ImGui::SetColumnWidth(2, 48.0f);   // thumbnail
                ImGui::SetColumnWidth(3, 150.0f);  // filename
                ImGui::SetColumnWidth(4, 100.0f);  // dimensions
                ImGui::SetColumnWidth(5, 100.0f);  // unique colors
                ImGui::SetColumnWidth(6, 160.0f);  // date created
                ImGui::SetColumnWidth(7, 80.0f);   // file size

                ImGui::TextDisabled("Index");         ImGui::NextColumn();
                ImGui::TextDisabled("");              ImGui::NextColumn();
                ImGui::TextDisabled("");              ImGui::NextColumn();
                ImGui::TextDisabled("Filename");      ImGui::NextColumn();
                ImGui::TextDisabled("Dimensions");    ImGui::NextColumn();
                ImGui::TextDisabled("Uniq. Colors");  ImGui::NextColumn();
                ImGui::TextDisabled("Date Created");  ImGui::NextColumn();
                ImGui::TextDisabled("Size");          ImGui::NextColumn();
                ImGui::Separator();
                ImGui::Columns(1);

                // ── Scrollable rows ──
                ImGui::BeginChild("##scrollable_rows", ImVec2(0, 0), false);

                    ImGui::Columns(8, "file_columns_scroll");
                    ImGui::SetColumnWidth(0, 70.0f);
                    ImGui::SetColumnWidth(1, 70.0f);
                    ImGui::SetColumnWidth(2, 48.0f);
                    ImGui::SetColumnWidth(3, 150.0f);
                    ImGui::SetColumnWidth(4, 100.0f);
                    ImGui::SetColumnWidth(5, 100.0f);
                    ImGui::SetColumnWidth(6, 160.0f);
                    ImGui::SetColumnWidth(7, 80.0f);

                    for (int i = pageStart; i < pageEnd; ++i) {
                        const auto& data = imageProcessor.imageDataArray[i];

                        // Col 0 — index input
                        int displayIndex = i + 1;
                        ImGui::SetNextItemWidth(50.0f);
                        if (ImGui::InputScalar(("##idx" + std::to_string(i)).c_str(), ImGuiDataType_S32, &displayIndex, nullptr, nullptr, "%d", ImGuiInputTextFlags_EnterReturnsTrue)) {
                            int targetIndex = displayIndex - 1;
                            if (targetIndex >= 0 && targetIndex < (int)imageProcessor.imageDataArray.size() && targetIndex != i) {
                                imageProcessor.moveImage(i, targetIndex);
                            }
                        }
                        ImGui::NextColumn();

                        // Col 1 — up/down buttons
                        if (ImGui::Button(("/\\##" + std::to_string(i)).c_str())) {
                            if (i > 0) imageProcessor.swapImages(i, i - 1);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button(("\\/##" + std::to_string(i)).c_str())) {
                            if (i < (int)imageProcessor.imageDataArray.size() - 1)
                                imageProcessor.swapImages(i, i + 1);
                        }
                        ImGui::NextColumn();

                        // Col 2 — thumbnail
                        if (data.metaData.thumbnailTextureID != 0) {
                            ImVec2 thumbPos = ImGui::GetCursorScreenPos();
                            if (showCheckerboard) {
                                ImGui::GetWindowDrawList()->AddImage(
                                    (ImTextureID)(intptr_t)checkerboardTex,
                                    thumbPos, ImVec2(thumbPos.x + 32, thumbPos.y + 32),
                                    ImVec2(0, 0), ImVec2(1.0f,1.0f)  // tiled UVs
                                );
                            }
                            ImGui::Image((ImTextureID)(intptr_t)data.metaData.thumbnailTextureID, ImVec2(32, 32));

                            if (ImGui::IsItemHovered())
                            {
                                previewUsedThisFrame = true;

                                if (hoveredIndex != i)
                                {
                                    if (hoveredTex != 0)
                                    {
                                        glDeleteTextures(1, &hoveredTex);
                                        hoveredTex = 0;
                                    }

                                    hoveredTex = imageProcessor.uploadFullImage(imageProcessor.imageArray[i]);
                                    hoveredIndex = i;
                                }

                                ImGui::BeginTooltip();

                                float previewMax = 512.0f;
                                float w = (float)data.imageTraits.width;
                                float h = (float)data.imageTraits.height;
                                float scale = std::min(previewMax / w, previewMax / h);
                                ImVec2 previewSize = ImVec2(w * scale, h * scale);

                                if (showCheckerboard) {
                                    ImVec2 tooltipPos = ImGui::GetCursorScreenPos();
                                    ImGui::GetWindowDrawList()->AddImage(
                                        (ImTextureID)(intptr_t)checkerboardTex,
                                        tooltipPos, ImVec2(tooltipPos.x + previewSize.x, tooltipPos.y + previewSize.y),
                                        ImVec2(0, 0), ImVec2(previewSize.x / 512.0f, previewSize.y / 512.0f)  // tiled UVs
                                    );
                                }
                                ImGui::Image((ImTextureID)(intptr_t)hoveredTex, previewSize);

                                ImGui::EndTooltip();
                            }
                        }
                        ImGui::NextColumn();

                        // Col 3 — filename
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
                        ImGui::Text("%s", data.metaData.name.c_str());
                        ImGui::NextColumn();

                        // Col 4 — dimensions
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
                        std::string dims = std::to_string(data.imageTraits.width) + "x"
                                        + std::to_string(data.imageTraits.height);
                        ImGui::Text("%s", dims.c_str());
                        ImGui::NextColumn();

                        // Col 5 — unique colors
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
                        ImGui::Text("%s", formatWithCommas(data.imageTraits.uniqueColors).c_str());
                        ImGui::NextColumn();

                        // Col 6 — date created
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
                        ImGui::Text("%s", imageProcessor.fileTimeToString(data.metaData.dateCreated).c_str());
                        ImGui::NextColumn();

                        // Col 7 — file size
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
                        ImGui::Text("%s", imageProcessor.formatFileSize(data.metaData.size).c_str());
                        ImGui::NextColumn();
                    }

                    ImGui::Columns(1);

                ImGui::EndChild();
            }
            ImGui::End();
        }

        if (!selectedFiles.empty()) {
            ImGui::SetNextWindowSize(ImVec2(screenSize.x*(1.0f/6.0f), screenSize.y*(1.0f/5.0f)), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(screenSize.x*(5.0f/6.0f), 0), ImGuiCond_Once);
            if (ImGui::Begin("Image Sequence Properties")) { 
                //for summary information and viewing options
                ImGui::Checkbox("Checkerboard", &showCheckerboard);
                if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("Displays transparent regions as a checkerboard pattern\nDoes not calculate into final render");
                    ImGui::EndTooltip();
                }
            }

            //getMaxUniqueColors()
            ImGui::Text("Highest unique color count");
            std::string uniqueColorVariants = "Color Variations: " + formatWithCommas(mostUniqueColorImageData.imageTraits.uniqueColors);
            ImGui::Text("File name: %s", mostUniqueColorImageData.metaData.name.c_str());
            ImGui::Text("%s", uniqueColorVariants.c_str());
            ImGui::Text("Index coverage: %.2f%%",indexPercentCoverage);
            if (ImGui::IsItemHovered()) {
                    ImGui::BeginTooltip();
                    ImGui::Text("if greater than or equal to 100%%, then all colors are represented\nReduce Index Bit Depth for higher video compression");
                    ImGui::EndTooltip();
                }


            ImGui::End();
        }

        if(isEncoding || encodingFinished){
            ImGui::SetNextWindowSize(ImVec2(screenSize.x*(1.0f/6.0f), screenSize.y*(4.0f/5.0f)), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(screenSize.x*(5.0f/6.0f), screenSize.y*(1.0f/5.0f)), ImGuiCond_Once);
            if (ImGui::Begin("Encoding Progress")) {

                // Overall progress bar
                float totalProgress = DataTypes::getTotalProgress(encodeProgressList);
                ImGui::Text("Overall");
                ImGui::SameLine();
                ImGui::ProgressBar(totalProgress, ImVec2(-1.0f, 0.0f));
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // Per-step progress bars
                for (const auto& entry : encodeProgressList) {
                    const char* stepName = DataTypes::stepToString(entry.EncodingPipelineStep);

                    // State badge color
                    ImVec4 stateColor;
                    const char* stateLabel;
                    switch (entry.state) {
                        case DataTypes::NOT_STARTED:
                            stateColor = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
                            stateLabel = "[   ]";
                            break;
                        case DataTypes::IN_PROGRESS:
                            stateColor = ImVec4(1.0f, 0.75f, 0.0f, 1.0f);
                            stateLabel = "[ > ]";
                            break;
                        case DataTypes::FINISHED:
                            stateColor = ImVec4(0.2f, 0.8f, 0.3f, 1.0f);
                            stateLabel = "[ + ]";
                            break;
                        case DataTypes::SKIPPED:
                            stateColor = ImVec4(0.5f, 0.5f, 0.5f, 0.6f);
                            stateLabel = "[ - ]";
                            break;
                        default:
                            stateColor = ImVec4(1.0f, 1.0f, 1.0f, 1.0f);
                            stateLabel = "[   ]";
                    }

                    ImGui::TextColored(stateColor, "%s", stateLabel);
                    ImGui::SameLine();
                    ImGui::Text("%s", stepName);
                    float barHeight = 14.0f;

                    // Show progress bar for active or finished steps; greyed out otherwise
                    float displayProgress = entry.percentFinished;
                    if (entry.state == DataTypes::SKIPPED) {
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.4f, 0.4f, 0.4f, 0.4f));
                        ImGui::ProgressBar(0.0f, ImVec2(-1.0f, barHeight), "Skipped");
                        ImGui::PopStyleColor();
                    } else if (entry.state == DataTypes::NOT_STARTED) {
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
                        ImGui::ProgressBar(0.0f, ImVec2(-1.0f, barHeight), "");
                        ImGui::PopStyleColor();
                    } else if (entry.state == DataTypes::FINISHED) {
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.2f, 0.75f, 0.3f, 1.0f));
                        ImGui::ProgressBar(1.0f, ImVec2(-1.0f, barHeight), "");
                        ImGui::PopStyleColor();
                    } else {
                        // IN_PROGRESS — amber/yellow
                        ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4(0.9f, 0.65f, 0.0f, 1.0f));
                        ImGui::ProgressBar(displayProgress, ImVec2(-1.0f, barHeight), "");
                        ImGui::PopStyleColor();
                    }

                    ImGui::Spacing();
                }
            }
        ImGui::End();
    }
        

        // ── File dialog ───────────────────────────────────────────────────────
        if (ImGuiFileDialog::Instance()->Display("ExportFileDlg")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                exportFolderPath        = ImGuiFileDialog::Instance()->GetCurrentPath();
                exportFolderPathDisplay = exportFolderPath.string();
            }
            ImGuiFileDialog::Instance()->Close();
        }

        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlg")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                selectedFiles = ImGuiFileDialog::Instance()->GetSelection();
                imageProcessor.initImageDataArray(selectedFiles.size());

                int i = 0;
                for (auto& [filename, filepath] : selectedFiles) {

                    imageProcessor.setImageData(filepath,i);
                    i++;
                }

                //set variables based on image data
                mostUniqueColorImageData = imageProcessor.getMaxUniqueColors();
                indexBitDepth = imageProcessor.calcRequiredBitDepth(mostUniqueColorImageData.imageTraits.uniqueColors);

                maxColors = imageProcessor.calcMaxColors(indexBitDepth);

                indexPercentCoverage = (double)maxColors / (double)mostUniqueColorImageData.imageTraits.uniqueColors * 100.0;

                width = imageProcessor.imageDataArray[0].imageTraits.width;
                height = imageProcessor.imageDataArray[0].imageTraits.height;

                supportsAlpha = imageProcessor.hasTransparency(0);
            }
            ImGuiFileDialog::Instance()->Close();
        }

        if (!previewUsedThisFrame && hoveredTex != 0)
        {
            glDeleteTextures(1, &hoveredTex);
            hoveredTex = 0;
            hoveredIndex = -1;
        }

        
        
        // ───────────────────────────────────────────────────────────────────

        // Finalize graphics generation
        ImGui::Render();
        
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.12f, 0.12f, 0.14f, 1.00f);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }


    // ── Cleanup ───────────────────────────────────────────────────────────────
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}


