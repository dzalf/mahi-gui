#define MAHI_GUI_NO_CONSOLE

#include <mahi/gui.hpp>
#include <random>

using namespace mahi::gui;

class DrawingDemo : public Application {
public:
    DrawingDemo() :
        Application("Drawing")
    { 
        // disable viewports
        ImGui::GetIO().ConfigFlags &= ~ImGuiConfigFlags_ViewportsEnable;
        backgroundColor = Grays::Black;
        generateDots({250,250}, 50, 20000);
        generateDots({500,250}, 50, 20000);
        generateDots({750,250}, 50, 20000);
        generateDots({1000,250}, 50, 20000);
        generateDots({1250,250}, 50, 20000);
        setVSync(false);
    }

    virtual void update() override {
        for (auto& group : dotGroups) {
            for (auto& dot : group) {
                ImGui::GetBackgroundDrawList()->AddRectFilled(dot,dot + ImVec2(1,1), dotColor);
            }
        }
        ImGui::Begin("My Window");
        ImGui::Text("FPS: %.2f", ImGui::GetIO().Framerate);
        ImGui::End();
    }    

    void generateDots(ImVec2 center, float radius, int n) {
        std::vector<ImVec2> dots;
        dots.reserve(n);
        for (int i = 0; i < n; ++i) {
            auto loc = center + ImVec2(dist(gen) * radius, dist(gen) * radius);
            dots.push_back(loc);
        }
        dotGroups.push_back(dots);
    }

    std::vector<std::vector<ImVec2>> dotGroups;
    std::default_random_engine gen;
    std::normal_distribution<float> dist;
    ImU32 dotColor = ImGui::GetColorU32({1,1,1,0.25f});
};

int main(int argc, char const *argv[])
{
    DrawingDemo demo;
    demo.run();
    return 0;
}