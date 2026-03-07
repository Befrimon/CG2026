#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include "Types.h"
#include "Window.h"
#include <vector>
#include <array>
#include <optional>
#include <string>

class Renderer {
public:
    explicit Renderer(Window& window);
    ~Renderer();
    Renderer(const Renderer&) = delete;
    Renderer& operator=(const Renderer&) = delete;

    void uploadMesh(const MeshData& mesh);
    void clearMeshes();

    void addLight(const Light& light);
    void clearLights();

    void queueDraw(int meshIdx, const glm::mat4& model);
    void clearDraws();

    void renderFrame(glm::vec3 camPos, glm::mat4 view, glm::mat4 proj);

private:
    struct DrawCmd {
        int meshIdx;
        glm::mat4 model;
    };

    struct QFI {
        std::optional<uint32_t> gfx, present;
        bool ok() const { return gfx && present; }
    };

    struct SCSupport {
        VkSurfaceCapabilitiesKHR caps{};
        std::vector<VkSurfaceFormatKHR> fmts;
        std::vector<VkPresentModeKHR> modes;
    };

    struct GpuMesh {
        VkBuffer vb = VK_NULL_HANDLE, ib = VK_NULL_HANDLE;
        VkDeviceMemory vm = VK_NULL_HANDLE, im = VK_NULL_HANDLE;
        uint32_t count = 0;
        VkImage texImg = VK_NULL_HANDLE;
        VkDeviceMemory texMem = VK_NULL_HANDLE;
        VkImageView texView = VK_NULL_HANDLE;
        VkSampler texSampler = VK_NULL_HANDLE;
        bool hasTex = false;
        VkImage tex2Img = VK_NULL_HANDLE;
        VkDeviceMemory tex2Mem = VK_NULL_HANDLE;
        VkImageView tex2View = VK_NULL_HANDLE;
        VkSampler tex2Sampler = VK_NULL_HANDLE;
        bool hasTex2 = false;
        Material mat;
        std::array<VkDescriptorSet, 2> ds{};
    };

    static constexpr int FRAMES = 2;
    static constexpr int MAX_MESHES = 64;

    void initInstance();
    void initSurface();
    void initDevice();
    void initSwapchain();
    void initImageViews();
    void initShadowMap();
    void initGBuffers();
    void initRenderPasses();
    void initFramebuffers();
    void initCmdPool();
    void initDescLayouts();
    void initPipelines();
    void initWhiteTex();
    void initUBOs();
    void initDescPool();
    void allocLightDescSets();
    void updateLightDescSets();
    void initCmdBuffers();
    void initSync();
    void recreateSwapchain();
    void destroySwapchain();

    QFI findQFI(VkPhysicalDevice);
    SCSupport querySC(VkPhysicalDevice);
    bool deviceOk(VkPhysicalDevice);
    VkSurfaceFormatKHR pickFmt(const std::vector<VkSurfaceFormatKHR>&);
    VkPresentModeKHR pickMode(const std::vector<VkPresentModeKHR>&);
    VkExtent2D pickExtent(const VkSurfaceCapabilitiesKHR&);
    VkFormat depthFmt();
    VkFormat findFmt(const std::vector<VkFormat>&, VkImageTiling, VkFormatFeatureFlags);
    uint32_t memType(uint32_t filter, VkMemoryPropertyFlags);
    VkShaderModule makeShader(const std::vector<char>&);
    std::vector<char> readFile(const std::string&);

    void mkBuf(VkDeviceSize, VkBufferUsageFlags, VkMemoryPropertyFlags, VkBuffer&, VkDeviceMemory&);
    void cpBuf(VkBuffer, VkBuffer, VkDeviceSize);
    void mkImg(uint32_t, uint32_t, VkFormat, VkImageTiling, VkImageUsageFlags, VkMemoryPropertyFlags, VkImage&, VkDeviceMemory&);
    VkImageView mkView(VkImage, VkFormat, VkImageAspectFlags);
    void transitionImg(VkImage, VkFormat, VkImageLayout, VkImageLayout);
    void cpBufToImg(VkBuffer, VkImage, uint32_t, uint32_t);
    VkCommandBuffer beginOnce();
    void endOnce(VkCommandBuffer);
    void allocMeshDescSets(GpuMesh& gm);
    void writeMeshDescSets(GpuMesh& gm);
    void uploadTextureToMesh(GpuMesh& gm, const std::string& path, int slot);

    Window& m_win;
    VkInstance m_inst = VK_NULL_HANDLE;
    VkSurfaceKHR m_surf = VK_NULL_HANDLE;
    VkPhysicalDevice m_gpu = VK_NULL_HANDLE;
    VkDevice m_dev = VK_NULL_HANDLE;
    VkQueue m_gfxQ = VK_NULL_HANDLE;
    VkQueue m_presQ = VK_NULL_HANDLE;
    uint32_t m_gfxFam = 0;
    uint32_t m_presFam = 0;

    VkSwapchainKHR m_sc = VK_NULL_HANDLE;
    std::vector<VkImage> m_scImgs;
    VkFormat m_scFmt = VK_FORMAT_UNDEFINED;
    VkExtent2D m_scExt{};
    std::vector<VkImageView> m_scViews;

    VkRenderPass m_mainRp = VK_NULL_HANDLE;
    VkRenderPass m_shadowRp = VK_NULL_HANDLE;

    VkImage m_di = VK_NULL_HANDLE;
    VkDeviceMemory m_dm = VK_NULL_HANDLE;
    VkImageView m_dv = VK_NULL_HANDLE;

    VkImage m_gPosImg = VK_NULL_HANDLE;
    VkDeviceMemory m_gPosMem = VK_NULL_HANDLE;
    VkImageView m_gPosView = VK_NULL_HANDLE;

    VkImage m_gNormImg = VK_NULL_HANDLE;
    VkDeviceMemory m_gNormMem = VK_NULL_HANDLE;
    VkImageView m_gNormView = VK_NULL_HANDLE;

    VkImage m_gAlbedoImg = VK_NULL_HANDLE;
    VkDeviceMemory m_gAlbedoMem = VK_NULL_HANDLE;
    VkImageView m_gAlbedoView = VK_NULL_HANDLE;

    std::array<VkImage, FRAMES> m_shadowImg{};
    std::array<VkDeviceMemory, FRAMES> m_shadowMem{};
    std::array<VkImageView, FRAMES> m_shadowView{};
    std::array<VkFramebuffer, FRAMES> m_shadowFb{};
    VkSampler m_shadowSampler = VK_NULL_HANDLE;

    std::vector<VkFramebuffer> m_fbs;

    VkDescriptorSetLayout m_geomDsl = VK_NULL_HANDLE;
    VkDescriptorSetLayout m_lightDsl = VK_NULL_HANDLE;
    VkPipelineLayout m_geomPl = VK_NULL_HANDLE;
    VkPipeline m_geomPipe = VK_NULL_HANDLE;
    VkPipelineLayout m_lightPl = VK_NULL_HANDLE;
    VkPipeline m_lightPipe = VK_NULL_HANDLE;
    VkPipelineLayout m_shadowPl = VK_NULL_HANDLE;
    VkPipeline m_shadowPipe = VK_NULL_HANDLE;

    VkCommandPool m_cp = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> m_cmds;

    std::array<VkBuffer, FRAMES> m_geomUb{};
    std::array<VkDeviceMemory, FRAMES> m_geomUbm{};
    std::array<void*, FRAMES> m_geomUbp{};

    std::array<VkBuffer, FRAMES> m_globalUb{};
    std::array<VkDeviceMemory, FRAMES> m_globalUbm{};
    std::array<void*, FRAMES> m_globalUbp{};

    VkDescriptorPool m_dp = VK_NULL_HANDLE;
    std::array<VkDescriptorSet, FRAMES> m_lightDs{};

    VkImage m_wImg = VK_NULL_HANDLE;
    VkDeviceMemory m_wMem = VK_NULL_HANDLE;
    VkImageView m_wV = VK_NULL_HANDLE;
    VkSampler m_wS = VK_NULL_HANDLE;

    std::vector<GpuMesh> m_meshes;
    std::vector<Light> m_lights;
    std::vector<DrawCmd> m_draws;

    std::array<VkSemaphore, FRAMES> m_imgReady{};
    std::array<VkSemaphore, FRAMES> m_renDone{};
    std::array<VkFence, FRAMES> m_fence{};

    uint32_t m_frame = 0;
    uint32_t m_imgIdx = 0;
};
