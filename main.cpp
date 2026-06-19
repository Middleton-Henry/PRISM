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

#include "ImGuiFileDialog/ImGuiFileDialog.h"
#include "image_processor.cpp"


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

int main()
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
    ImageProcessor imageProcessor;
    bool opened = true;
    std::map<std::string, std::string> selectedFiles;
    unsigned int width = 1080; //set to first image upon upload
    unsigned int height = 1920; //set to first image upon upload

    const char* interpolationModes[] = { "Nearest Neighbor (Pixelated)", "Bilinear (Blurred)", "Cubic (Sharp)"};
    ImageProcessor::InterpolationModes currentInterpolationMode = ImageProcessor::NEAREST_NEIGHBOR;

    unsigned int frameRate = 12;
    int indexBitDepth = 24; //max 32 if including all alpha variations
    uint64_t maxColors = imageProcessor.calcMaxColors(indexBitDepth);

    int currentPage = 0;
    const int itemsPerPage = 25;
    int currentPageDisplay = 1;

    const char* sortingSystems[] = { "Upload Order (First to Last)", "Name (A to Z)", "Date Modified (Oldest to Newest)", "Date Created (Oldest to Newest)"};

    ImageProcessor::SortingSystems currentSortingOrder = ImageProcessor::UPLOAD_ORDER;

    bool supportsAlpha = true;

    static float alphaReplacement[3] = { 1.0f, 0.0f, 1.0f };

    bool isDithered = false;

    const char* paletteSortingSolvers[] = { "Hungarian Solver (High)", "Auction Solver (Mid)", "Nearest Neighbor Snapping (Low)"};
    ImageProcessor::PaletteSortingSolvers currentPaletteSolver = ImageProcessor::AUCTION;

    // ── UI Graphics Init ──────────────────────────


    // ── Render loop ──────────────────────────────────────────────────────────
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        // Start a new frame context
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ── UI Graphics Loop ──────────────────────────

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
            
            //Sorting option dropdown
            ImGui::PushItemWidth(dropDownWidth);
            if(ImGui::BeginCombo("Frame Sorter", sortingSystems[currentSortingOrder])){
                for (int i = 0; i < std::size(sortingSystems); i++) 
                {
                    bool isSelected = (currentSortingOrder == i);

                    if (ImGui::Selectable(sortingSystems[i], isSelected)) 
                    {
                        ImageProcessor::SortingSystems newSortingOrder = static_cast<ImageProcessor::SortingSystems>(i);
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
                        ImageProcessor::InterpolationModes newInterpolationMode = static_cast<ImageProcessor::InterpolationModes>(i);
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

            ImGui::Checkbox("Dither Image", &isDithered);
            if (ImGui::IsItemHovered()) {
                ImGui::BeginTooltip();
                ImGui::Text("Dithered image is perceptually more accurate but increases file size");
                ImGui::EndTooltip();
            }

            //Color Palette Sorting Algorithm
            ImGui::PushItemWidth(dropDownWidth);
            if(ImGui::BeginCombo("Color Palette Sorting", paletteSortingSolvers[currentPaletteSolver])){
                for (int i = 0; i < std::size(paletteSortingSolvers); i++) 
                {
                    bool isSelected = (currentPaletteSolver == i);

                    if (ImGui::Selectable(paletteSortingSolvers[i], isSelected)) 
                    {
                        ImageProcessor::PaletteSortingSolvers newPaletteSolver = static_cast<ImageProcessor::PaletteSortingSolvers>(i);
                        currentPaletteSolver = newPaletteSolver;
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
                ImGui::Text("Algorithm for sorting color palettes\nHungarian - High Accuracy / High Memory and Performance Cost\nAuction - Mid Accuracy / Mid Memory and Performance Cost\nNearest Neighbor - Low Accuracy / Low Memory and Performance Cost");
                ImGui::EndTooltip();
            }
            ImGui::PopItemWidth();
        }
        ImGui::End();

        // ── File list window ─────────────
        if (!selectedFiles.empty()) {
            ImGui::SetNextWindowSize(ImVec2(650, 400), ImGuiCond_Once);
            ImGui::SetNextWindowPos(ImVec2(400, 0), ImGuiCond_Once);
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
                ImGui::Columns(6, "file_columns", false);
                ImGui::SetColumnWidth(0, 70.0f);   // index input
                ImGui::SetColumnWidth(1, 70.0f);   // up/down buttons
                ImGui::SetColumnWidth(2, 48.0f);   // thumbnail
                ImGui::SetColumnWidth(3, 150.0f);  // filename
                ImGui::SetColumnWidth(4, 120.0f);  // dimensions
                ImGui::SetColumnWidth(5, 160.0f);  // created

                ImGui::TextDisabled("Index");       ImGui::NextColumn();
                ImGui::TextDisabled("");            ImGui::NextColumn();
                ImGui::TextDisabled("");            ImGui::NextColumn();
                ImGui::TextDisabled("Filename");    ImGui::NextColumn();
                ImGui::TextDisabled("Dimensions");  ImGui::NextColumn();
                ImGui::TextDisabled("Created");     ImGui::NextColumn();
                ImGui::Separator();
                ImGui::Columns(1);

                // ── Scrollable rows ───────────────────────────────────────────────────────────
                ImGui::BeginChild("##scrollable_rows", ImVec2(0, 0), false);

                    ImGui::Columns(6, "file_columns_scroll");
                    ImGui::SetColumnWidth(0, 70.0f);
                    ImGui::SetColumnWidth(1, 70.0f);
                    ImGui::SetColumnWidth(2, 48.0f);
                    ImGui::SetColumnWidth(3, 150.0f);
                    ImGui::SetColumnWidth(4, 120.0f);
                    ImGui::SetColumnWidth(5, 160.0f);

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
                            ImGui::Image((ImTextureID)(intptr_t)data.metaData.thumbnailTextureID, ImVec2(32, 32));
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

                        // Col 5 — created
                        ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8.0f);
                        ImGui::Text("%s", imageProcessor.fileTimeToString(data.metaData.dateCreated).c_str());
                        ImGui::NextColumn();
                    }

                    ImGui::Columns(1);

                ImGui::EndChild();
            }
            ImGui::End();
        }

        // ── File dialog ───────────────────────────────────────────────────────
        if (ImGuiFileDialog::Instance()->Display("ChooseFileDlg")) {
            if (ImGuiFileDialog::Instance()->IsOk()) {
                selectedFiles = ImGuiFileDialog::Instance()->GetSelection();
                imageProcessor.initImageDataArray(selectedFiles.size());

                int i = 0;
                for (auto& [filename, filepath] : selectedFiles) {

                    imageProcessor.setImageData(filepath,i);
                    i++;
                }
            }
            ImGuiFileDialog::Instance()->Close();
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


