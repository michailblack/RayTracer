#include "UserInterface.hpp"
#include "SceneList.hpp"
#include "UserSettings.hpp"
#include "Utilities/Exception.hpp"
#include "Vulkan/DescriptorPool.hpp"
#include "Vulkan/Device.hpp"
#include "Vulkan/FrameBuffer.hpp"
#include "Vulkan/Instance.hpp"
#include "Vulkan/RenderPass.hpp"
#include "Vulkan/SingleTimeCommands.hpp"
#include "Vulkan/Surface.hpp"
#include "Vulkan/SwapChain.hpp"
#include "Vulkan/Window.hpp"

#include <imgui.h>
#include <imgui_freetype.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_vulkan.h>

#include <array>

namespace
{
	void CheckVulkanResultCallback(const VkResult err)
	{
		if (err != VK_SUCCESS)
		{
			Throw(std::runtime_error(std::string("ImGui Vulkan error (") + Vulkan::ToString(err) + ")"));
		}
	}
}

UserInterface::UserInterface(
	Vulkan::CommandPool& commandPool, 
	const Vulkan::SwapChain& swapChain, 
	const Vulkan::DepthBuffer& depthBuffer,
	UserSettings& userSettings) :
	userSettings_(userSettings)
{
	const auto& device = swapChain.Device();
	const auto& window = device.Surface().Instance().Window();

	// Initialise descriptor pool and render pass for ImGui.
	const std::vector<Vulkan::DescriptorBinding> descriptorBindings =
	{
		{0, 1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 0},
	};
	descriptorPool_.reset(new Vulkan::DescriptorPool(device, descriptorBindings, 1));
	renderPass_.reset(new Vulkan::RenderPass(swapChain, depthBuffer, VK_ATTACHMENT_LOAD_OP_LOAD, VK_ATTACHMENT_LOAD_OP_LOAD));

	// Initialise ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	// Initialise ImGui GLFW adapter
	if (!ImGui_ImplGlfw_InitForVulkan(window.Handle(), true))
	{
		Throw(std::runtime_error("failed to initialise ImGui GLFW adapter"));
	}

	// Initialise ImGui Vulkan adapter
	ImGui_ImplVulkan_InitInfo vulkanInit = {};
	vulkanInit.Instance = device.Surface().Instance().Handle();
	vulkanInit.PhysicalDevice = device.PhysicalDevice();
	vulkanInit.Device = device.Handle();
	vulkanInit.QueueFamily = device.GraphicsFamilyIndex();
	vulkanInit.Queue = device.GraphicsQueue();
	vulkanInit.PipelineCache = nullptr;
	vulkanInit.DescriptorPool = descriptorPool_->Handle();
	vulkanInit.RenderPass = renderPass_->Handle();
	vulkanInit.MinImageCount = swapChain.MinImageCount();
	vulkanInit.ImageCount = static_cast<uint32_t>(swapChain.Images().size());
	vulkanInit.Allocator = nullptr;
	vulkanInit.CheckVkResultFn = CheckVulkanResultCallback;

	if (!ImGui_ImplVulkan_Init(&vulkanInit))
	{
		Throw(std::runtime_error("failed to initialise ImGui vulkan adapter"));
	}

	auto& io = ImGui::GetIO();

	// No ini file.
	io.IniFilename = nullptr;

	// Window scaling and style.
	const auto scaleFactor = window.ContentScale();

	ImGui::StyleColorsDark();
	ImGui::GetStyle().ScaleAllSizes(scaleFactor);

	// Upload ImGui fonts (use ImGuiFreeType for better font rendering, see https://github.com/ocornut/imgui/tree/master/misc/freetype).
	io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
	if (!io.Fonts->AddFontFromFileTTF("../assets/fonts/Cousine-Regular.ttf", 13 * scaleFactor))
	{
		Throw(std::runtime_error("failed to load ImGui font"));
	}

	Vulkan::SingleTimeCommands::Submit(commandPool, [] (VkCommandBuffer commandBuffer)
	{
		if (!ImGui_ImplVulkan_CreateFontsTexture())
		{
			Throw(std::runtime_error("failed to create ImGui font textures"));
		}
	});
}

UserInterface::~UserInterface()
{
	ImGui_ImplVulkan_Shutdown();
	ImGui_ImplGlfw_Shutdown();
	ImGui::DestroyContext();
}

void UserInterface::Render(VkCommandBuffer commandBuffer, const Vulkan::FrameBuffer& frameBuffer, const Statistics& statistics)
{
	ImGui_ImplGlfw_NewFrame();
	ImGui_ImplVulkan_NewFrame();
	ImGui::NewFrame();

	DrawSettings();
	DrawOverlay(statistics);
	//ImGui::ShowStyleEditor();
	ImGui::Render();

	VkRenderPassBeginInfo renderPassInfo = {};
	renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
	renderPassInfo.renderPass = renderPass_->Handle();
	renderPassInfo.framebuffer = frameBuffer.Handle();
	renderPassInfo.renderArea.offset = { 0, 0 };
	renderPassInfo.renderArea.extent = renderPass_->SwapChain().Extent();
	renderPassInfo.clearValueCount = 0;
	renderPassInfo.pClearValues = nullptr;

	vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), commandBuffer);
	vkCmdEndRenderPass(commandBuffer);
}

bool UserInterface::WantsToCaptureKeyboard() const
{
	return ImGui::GetIO().WantCaptureKeyboard;
}

bool UserInterface::WantsToCaptureMouse() const
{
	return ImGui::GetIO().WantCaptureMouse;
}

void UserInterface::DrawSettings()
{
    if (!Settings().ShowSettings)
    {
        return;
    }

    // Custom styling to make it look like a different application
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(16.0f, 16.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 12.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ScrollbarRounding, 6.0f);

    // Custom colors for a different look
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.14f, 0.18f, 0.95f));
    ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.25f, 0.3f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.3f, 0.35f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.3f, 0.35f, 0.4f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.25f, 0.3f, 0.6f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.3f, 0.35f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.35f, 0.4f, 0.8f));

    const auto& io = ImGui::GetIO();
    const float distance = 10.0f;
    const ImVec2 pos = ImVec2(io.DisplaySize.x - distance, distance);
    const ImVec2 posPivot = ImVec2(1.0f, 0.0f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, posPivot);

    const auto flags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("⚙ Configuration Panel", &Settings().ShowSettings, flags))
    {
        std::vector<const char*> scenes;
        for (const auto& scene : SceneList::AllScenes)
        {
            scenes.push_back(scene.first.c_str());
        }

        const auto& window = descriptorPool_->Device().Surface().Instance().Window();

        // Help Section with collapsing header
        if (ImGui::CollapsingHeader("📖 Controls & Navigation", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(12.0f);
            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "🔧 F1:");
            ImGui::SameLine(); ImGui::Text("Toggle Configuration Panel");

            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "📊 F2:");
            ImGui::SameLine(); ImGui::Text("Toggle Performance Statistics");

            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "🎮 %c%c%c%c/SHIFT/CTRL:",
                std::toupper(window.GetKeyName(GLFW_KEY_W, 0)[0]),
                std::toupper(window.GetKeyName(GLFW_KEY_A, 0)[0]),
                std::toupper(window.GetKeyName(GLFW_KEY_S, 0)[0]),
                std::toupper(window.GetKeyName(GLFW_KEY_D, 0)[0]));
            ImGui::SameLine(); ImGui::Text("Camera Movement");

            ImGui::TextColored(ImVec4(0.7f, 0.9f, 1.0f, 1.0f), "🖱 L/R Mouse:");
            ImGui::SameLine(); ImGui::Text("Rotate Camera/Scene");
            ImGui::Unindent(12.0f);
            ImGui::Spacing();
        }

        // Scene Selection with modern styling
        if (ImGui::CollapsingHeader("🌍 Scene Management", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(12.0f);
            ImGui::Text("Active Scene:");
            ImGui::PushItemWidth(-1);
            if (ImGui::BeginCombo("##SceneList", scenes[Settings().SceneIndex]))
            {
                for (int i = 0; i < scenes.size(); i++)
                {
                    bool is_selected = (Settings().SceneIndex == i);
                    if (ImGui::Selectable(scenes[i], is_selected))
                        Settings().SceneIndex = i;
                    if (is_selected)
                        ImGui::SetItemDefaultFocus();
                }
                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();
            ImGui::Unindent(12.0f);
            ImGui::Spacing();
        }

        // Ray Tracing with enhanced layout
        if (ImGui::CollapsingHeader("✨ Ray Tracing Engine", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(12.0f);

            // Toggle switches
            ImGui::AlignTextToFramePadding();
            ImGui::Text("Enable Ray Tracing:");
            ImGui::SameLine(180);
            ImGui::Checkbox("##EnableRT", &Settings().IsRayTraced);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Frame Accumulation:");
            ImGui::SameLine(180);
            ImGui::Checkbox("##AccumulateRays", &Settings().AccumulateRays);

            ImGui::Separator();

            // Sliders with better spacing
            ImGui::Text("Quality Settings:");
            uint32_t min = 1, max = 128;
            ImGui::Text("Sample Count:");
            ImGui::SameLine(120);
            ImGui::PushItemWidth(160);
            ImGui::SliderScalar("##Samples", ImGuiDataType_U32, &Settings().NumberOfSamples, &min, &max);
            ImGui::PopItemWidth();

            min = 1, max = 32;
            ImGui::Text("Bounce Limit:");
            ImGui::SameLine(120);
            ImGui::PushItemWidth(160);
            ImGui::SliderScalar("##Bounces", ImGuiDataType_U32, &Settings().NumberOfBounces, &min, &max);
            ImGui::PopItemWidth();

            ImGui::Unindent(12.0f);
            ImGui::Spacing();
        }

        // Camera settings with icons
        if (ImGui::CollapsingHeader("📷 Camera Properties", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(12.0f);

            ImGui::Text("Field of View:");
            ImGui::SameLine(120);
            ImGui::PushItemWidth(160);
            ImGui::SliderFloat("##FoV", &Settings().FieldOfView, UserSettings::FieldOfViewMinValue, UserSettings::FieldOfViewMaxValue, "%.0f°");
            ImGui::PopItemWidth();

            ImGui::Text("Aperture:");
            ImGui::SameLine(120);
            ImGui::PushItemWidth(160);
            ImGui::SliderFloat("##Aperture", &Settings().Aperture, 0.0f, 1.0f, "f/%.2f");
            ImGui::PopItemWidth();

            ImGui::Text("Focus Distance:");
            ImGui::SameLine(120);
            ImGui::PushItemWidth(160);
            ImGui::SliderFloat("##Focus", &Settings().FocusDistance, 0.1f, 20.0f, "%.1fm");
            ImGui::PopItemWidth();

            ImGui::Unindent(12.0f);
            ImGui::Spacing();
        }

        // Performance profiler with better visualization
        if (ImGui::CollapsingHeader("📈 Performance Analysis", ImGuiTreeNodeFlags_DefaultOpen))
        {
            ImGui::Indent(12.0f);

            ImGui::AlignTextToFramePadding();
            ImGui::Text("Heat Map Overlay:");
            ImGui::SameLine(180);
            ImGui::Checkbox("##ShowHeatmap", &Settings().ShowHeatmap);

            if (Settings().ShowHeatmap)
            {
                ImGui::Text("Intensity Scale:");
                ImGui::SameLine(120);
                ImGui::PushItemWidth(160);
                ImGui::SliderFloat("##HeatmapScale", &Settings().HeatmapScale, 0.10f, 10.0f, "%.2fx", ImGuiSliderFlags_Logarithmic);
                ImGui::PopItemWidth();
            }

            ImGui::Unindent(12.0f);
        }
    }
    ImGui::End();

    // Pop all style modifications
    ImGui::PopStyleColor(7);
    ImGui::PopStyleVar(5);
}

void UserInterface::DrawOverlay(const Statistics& statistics)
{
    if (!Settings().ShowOverlay)
    {
        return;
    }

    // Enhanced styling for the overlay (avoiding size-affecting changes)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 8.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 4.0f));

    // Modern overlay color scheme
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.12f, 0.16f, 0.85f));
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.95f, 1.0f, 1.0f));

    const auto& io = ImGui::GetIO();
    const float distance = 10.0f;
    const ImVec2 pos = ImVec2(distance, distance);
    const ImVec2 posPivot = ImVec2(0.0f, 0.0f);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always, posPivot);

    const auto flags =
        ImGuiWindowFlags_AlwaysAutoResize |
        ImGuiWindowFlags_NoDecoration |
        ImGuiWindowFlags_NoFocusOnAppearing |
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoNav |
        ImGuiWindowFlags_NoSavedSettings;

    if (ImGui::Begin("##PerformanceMonitor", &Settings().ShowOverlay, flags))
    {
        // Add manual spacing for better layout
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
        ImGui::Indent(8.0f);

        // Header with resolution info
        ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), "⚡ PERFORMANCE MONITOR");

        ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f), "Resolution: %dx%d",
                          statistics.FramebufferSize.width, statistics.FramebufferSize.height);

        // Thin separator line
        ImGui::PushStyleColor(ImGuiCol_Separator, ImVec4(0.4f, 0.6f, 0.8f, 0.5f));
        ImGui::Separator();
        ImGui::PopStyleColor();

        // Performance metrics with consistent formatting to prevent sliding
        // Frame rate with color based on performance
        ImVec4 fpsColor;
        if (statistics.FrameRate >= 60.0f)
            fpsColor = ImVec4(0.3f, 0.9f, 0.3f, 1.0f); // Green for good fps
        else if (statistics.FrameRate >= 30.0f)
            fpsColor = ImVec4(0.9f, 0.7f, 0.2f, 1.0f); // Yellow for medium fps
        else
            fpsColor = ImVec4(0.9f, 0.3f, 0.3f, 1.0f); // Red for low fps

        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "🖼 Frame Rate:");
        ImGui::SameLine(110);
        ImGui::TextColored(fpsColor, "%6.1f fps", statistics.FrameRate);

        // Ray rate with consistent width formatting
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "⚡ Ray Rate:");
        ImGui::SameLine(110);
        ImGui::TextColored(ImVec4(0.5f, 0.9f, 0.7f, 1.0f), "%6.2f Gr/s", statistics.RayRate);

        // Sample accumulation with consistent formatting
        ImGui::TextColored(ImVec4(0.8f, 0.9f, 1.0f, 1.0f), "📊 Samples:");
        ImGui::SameLine(110);
        ImGui::TextColored(ImVec4(0.6f, 0.8f, 1.0f, 1.0f), "%8u", statistics.TotalSamples);
        ImGui::Unindent(8.0f);
        ImGui::Dummy(ImVec2(0.0f, 4.0f));
    }
    ImGui::End();

    // Pop all style modifications
    ImGui::PopStyleColor(2);
    ImGui::PopStyleVar(2);
}
