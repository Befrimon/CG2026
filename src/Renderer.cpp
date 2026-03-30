#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include "Renderer.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <stdexcept>
#include <algorithm>
#include <cstring>
#include <iostream>

static const std::vector<const char*> kDevExts = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };

Renderer::Renderer(Window& window) : m_win(window) {
    initInstance();
    initSurface();
    initDevice();
    initSwapchain();
    initImageViews();
    initShadowMap();
    initRenderPasses();
    initDescLayouts();
    initPipelines();
    initGBuffers();
    initFramebuffers();
    initCmdPool();
    initDummyTextures();
    initUBOs();
    initDescPool();
    allocLightDescSets();
    updateLightDescSets();
    initCmdBuffers();
    initSync();
}

Renderer::~Renderer() {
    vkDeviceWaitIdle(m_dev);
    destroySwapchain();
    clearMeshes();

    if (m_wS) vkDestroySampler(m_dev, m_wS, nullptr);
    if (m_wV) vkDestroyImageView(m_dev, m_wV, nullptr); if (m_wImg) vkDestroyImage(m_dev, m_wImg, nullptr); if (m_wMem) vkFreeMemory(m_dev, m_wMem, nullptr);
    if (m_flatNormView) vkDestroyImageView(m_dev, m_flatNormView, nullptr); if (m_flatNormImg) vkDestroyImage(m_dev, m_flatNormImg, nullptr); if (m_flatNormMem) vkFreeMemory(m_dev, m_flatNormMem, nullptr);
    if (m_blackView) vkDestroyImageView(m_dev, m_blackView, nullptr); if (m_blackImg) vkDestroyImage(m_dev, m_blackImg, nullptr); if (m_blackMem) vkFreeMemory(m_dev, m_blackMem, nullptr);

    for (int i = 0; i < FRAMES; i++) {
        vkDestroyBuffer(m_dev, m_geomUb[i], nullptr); vkFreeMemory(m_dev, m_geomUbm[i], nullptr);
        vkDestroyBuffer(m_dev, m_globalUb[i], nullptr); vkFreeMemory(m_dev, m_globalUbm[i], nullptr);
    }
    vkDestroySampler(m_dev, m_shadowSampler, nullptr);
    vkDestroyDescriptorPool(m_dev, m_dp, nullptr);
    vkDestroyDescriptorSetLayout(m_dev, m_geomDsl, nullptr);
    vkDestroyDescriptorSetLayout(m_dev, m_lightDsl, nullptr);
    vkDestroyPipeline(m_dev, m_geomPipe, nullptr); vkDestroyPipelineLayout(m_dev, m_geomPl, nullptr);
    vkDestroyPipeline(m_dev, m_lightPipe, nullptr); vkDestroyPipelineLayout(m_dev, m_lightPl, nullptr);
    vkDestroyPipeline(m_dev, m_shadowPipe, nullptr); vkDestroyPipelineLayout(m_dev, m_shadowPl, nullptr);
    vkDestroyRenderPass(m_dev, m_mainRp, nullptr); vkDestroyRenderPass(m_dev, m_shadowRp, nullptr);
    vkDestroyCommandPool(m_dev, m_cp, nullptr);
    for (int i = 0; i < FRAMES; i++) {
        vkDestroySemaphore(m_dev, m_imgReady[i], nullptr); vkDestroySemaphore(m_dev, m_renDone[i], nullptr); vkDestroyFence(m_dev, m_fence[i], nullptr);
    }
    vkDestroyDevice(m_dev, nullptr);
    vkDestroySurfaceKHR(m_inst, m_surf, nullptr);
    vkDestroyInstance(m_inst, nullptr);
}

void Renderer::initInstance() {
    VkApplicationInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO; ai.apiVersion = VK_API_VERSION_1_2;
    uint32_t n = 0; const char** exts = glfwGetRequiredInstanceExtensions(&n);
    std::vector<const char*> extensions(exts, exts + n);
    VkInstanceCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO; ci.pApplicationInfo = &ai;
    ci.enabledExtensionCount = extensions.size(); ci.ppEnabledExtensionNames = extensions.data();
    if (vkCreateInstance(&ci, nullptr, &m_inst) != VK_SUCCESS) throw std::runtime_error("");
}
void Renderer::initSurface() { if (glfwCreateWindowSurface(m_inst, m_win.handle(), nullptr, &m_surf) != VK_SUCCESS) throw std::runtime_error(""); }
void Renderer::initDevice() {
    uint32_t n = 0; vkEnumeratePhysicalDevices(m_inst, &n, nullptr);
    std::vector<VkPhysicalDevice> devs(n); vkEnumeratePhysicalDevices(m_inst, &n, devs.data());
    for (auto d : devs) if (deviceOk(d)) { m_gpu = d; break; }
    auto qfi = findQFI(m_gpu); m_gfxFam = qfi.gfx.value(); m_presFam = qfi.present.value();
    float prio = 1.f; std::vector<VkDeviceQueueCreateInfo> qcis;
    VkDeviceQueueCreateInfo q1{}; q1.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; q1.queueFamilyIndex = m_gfxFam; q1.queueCount = 1; q1.pQueuePriorities = &prio; qcis.push_back(q1);
    if (m_gfxFam != m_presFam) {
        VkDeviceQueueCreateInfo q2{}; q2.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO; q2.queueFamilyIndex = m_presFam; q2.queueCount = 1; q2.pQueuePriorities = &prio; qcis.push_back(q2);
    }
    VkPhysicalDeviceFeatures feat{};
    feat.samplerAnisotropy = VK_TRUE;
    feat.tessellationShader = VK_TRUE;

    VkDeviceCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    ci.queueCreateInfoCount = qcis.size(); ci.pQueueCreateInfos = qcis.data();
    ci.enabledExtensionCount = kDevExts.size(); ci.ppEnabledExtensionNames = kDevExts.data();
    ci.pEnabledFeatures = &feat;
    if (vkCreateDevice(m_gpu, &ci, nullptr, &m_dev) != VK_SUCCESS) throw std::runtime_error("");
    vkGetDeviceQueue(m_dev, m_gfxFam, 0, &m_gfxQ); vkGetDeviceQueue(m_dev, m_presFam, 0, &m_presQ);
}
void Renderer::initSwapchain() {
    auto sc = querySC(m_gpu); auto fmt = pickFmt(sc.fmts); auto mode = pickMode(sc.modes); auto ext = pickExtent(sc.caps);
    uint32_t count = sc.caps.minImageCount + 1; if (sc.caps.maxImageCount > 0) count = std::min(count, sc.caps.maxImageCount);
    VkSwapchainCreateInfoKHR ci{}; ci.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR; ci.surface = m_surf; ci.minImageCount = count; ci.imageFormat = fmt.format; ci.imageColorSpace = fmt.colorSpace; ci.imageExtent = ext; ci.imageArrayLayers = 1; ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    uint32_t fams[] = {m_gfxFam, m_presFam};
    if (m_gfxFam != m_presFam) { ci.imageSharingMode = VK_SHARING_MODE_CONCURRENT; ci.queueFamilyIndexCount = 2; ci.pQueueFamilyIndices = fams; } else { ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; }
    ci.preTransform = sc.caps.currentTransform; ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; ci.presentMode = mode; ci.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(m_dev, &ci, nullptr, &m_sc) != VK_SUCCESS) throw std::runtime_error("");
    vkGetSwapchainImagesKHR(m_dev, m_sc, &count, nullptr); m_scImgs.resize(count); vkGetSwapchainImagesKHR(m_dev, m_sc, &count, m_scImgs.data()); m_scFmt = fmt.format; m_scExt = ext;
}
void Renderer::initImageViews() { m_scViews.resize(m_scImgs.size()); for (size_t i = 0; i < m_scImgs.size(); i++) m_scViews[i] = mkView(m_scImgs[i], m_scFmt, VK_IMAGE_ASPECT_COLOR_BIT); }
void Renderer::initShadowMap() {
    for (int i = 0; i < FRAMES; i++) {
        mkImg(2048, 2048, VK_FORMAT_D32_SFLOAT, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_shadowImg[i], m_shadowMem[i]);
        m_shadowView[i] = mkView(m_shadowImg[i], VK_FORMAT_D32_SFLOAT, VK_IMAGE_ASPECT_DEPTH_BIT);
    }
    VkSamplerCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; si.magFilter = VK_FILTER_LINEAR; si.minFilter = VK_FILTER_LINEAR; si.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE; si.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    vkCreateSampler(m_dev, &si, nullptr, &m_shadowSampler);
}
void Renderer::initGBuffers() {
    auto mkGBuf = [&](VkFormat fmt, VkImage& img, VkDeviceMemory& mem, VkImageView& view) {
        mkImg(m_scExt.width, m_scExt.height, fmt, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
        view = mkView(img, fmt, VK_IMAGE_ASPECT_COLOR_BIT);
    };
    mkGBuf(VK_FORMAT_R16G16B16A16_SFLOAT, m_gPosImg, m_gPosMem, m_gPosView);
    mkGBuf(VK_FORMAT_R16G16B16A16_SFLOAT, m_gNormImg, m_gNormMem, m_gNormView);
    mkGBuf(VK_FORMAT_R8G8B8A8_UNORM, m_gAlbedoImg, m_gAlbedoMem, m_gAlbedoView);
    VkFormat dFmt = depthFmt(); mkImg(m_scExt.width, m_scExt.height, dFmt, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, m_di, m_dm);
    m_dv = mkView(m_di, dFmt, VK_IMAGE_ASPECT_DEPTH_BIT);
}
void Renderer::initRenderPasses() {
    VkAttachmentDescription sd{}; sd.format = VK_FORMAT_D32_SFLOAT; sd.samples = VK_SAMPLE_COUNT_1_BIT; sd.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; sd.storeOp = VK_ATTACHMENT_STORE_OP_STORE; sd.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; sd.finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkAttachmentReference sRef{0, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    VkSubpassDescription sSub{}; sSub.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; sSub.pDepthStencilAttachment = &sRef;
    VkSubpassDependency sDep{}; sDep.srcSubpass = 0; sDep.dstSubpass = VK_SUBPASS_EXTERNAL; sDep.srcStageMask = VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT; sDep.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; sDep.srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT; sDep.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    VkRenderPassCreateInfo sci{}; sci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO; sci.attachmentCount = 1; sci.pAttachments = &sd; sci.subpassCount = 1; sci.pSubpasses = &sSub; sci.dependencyCount = 1; sci.pDependencies = &sDep;
    if (vkCreateRenderPass(m_dev, &sci, nullptr, &m_shadowRp) != VK_SUCCESS) throw std::runtime_error("");

    std::array<VkAttachmentDescription, 5> atts{};
    atts[0].format = m_scFmt; atts[0].samples = VK_SAMPLE_COUNT_1_BIT; atts[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; atts[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE; atts[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; atts[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    atts[1].format = depthFmt(); atts[1].samples = VK_SAMPLE_COUNT_1_BIT; atts[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; atts[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; atts[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; atts[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    for (int i = 2; i <= 4; i++) { atts[i].samples = VK_SAMPLE_COUNT_1_BIT; atts[i].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; atts[i].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; atts[i].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; atts[i].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL; }
    atts[2].format = VK_FORMAT_R16G16B16A16_SFLOAT; atts[3].format = VK_FORMAT_R16G16B16A16_SFLOAT; atts[4].format = VK_FORMAT_R8G8B8A8_UNORM;
    std::array<VkAttachmentReference, 3> gColorRefs = {{{2, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, {3, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}, {4, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL}}};
    VkAttachmentReference gDepthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};
    std::array<VkAttachmentReference, 3> lInputRefs = {{{2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, {3, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}, {4, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL}}};
    VkAttachmentReference lColorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    std::array<VkSubpassDescription, 2> subpasses{};
    subpasses[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; subpasses[0].colorAttachmentCount = gColorRefs.size(); subpasses[0].pColorAttachments = gColorRefs.data(); subpasses[0].pDepthStencilAttachment = &gDepthRef;
    subpasses[1].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS; subpasses[1].inputAttachmentCount = lInputRefs.size(); subpasses[1].pInputAttachments = lInputRefs.data(); subpasses[1].colorAttachmentCount = 1; subpasses[1].pColorAttachments = &lColorRef;
    std::array<VkSubpassDependency, 3> deps{};
    deps[0].srcSubpass = VK_SUBPASS_EXTERNAL; deps[0].dstSubpass = 0; deps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT; deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    deps[1].srcSubpass = 0; deps[1].dstSubpass = 1; deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; deps[1].dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT; deps[1].dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    deps[2].srcSubpass = 1; deps[2].dstSubpass = VK_SUBPASS_EXTERNAL; deps[2].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT; deps[2].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT; deps[2].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo mci{}; mci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO; mci.attachmentCount = atts.size(); mci.pAttachments = atts.data(); mci.subpassCount = subpasses.size(); mci.pSubpasses = subpasses.data(); mci.dependencyCount = deps.size(); mci.pDependencies = deps.data();
    if (vkCreateRenderPass(m_dev, &mci, nullptr, &m_mainRp) != VK_SUCCESS) throw std::runtime_error("");
}
void Renderer::initFramebuffers() {
    for (int i = 0; i < FRAMES; i++) {
        VkFramebufferCreateInfo sfci{}; sfci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; sfci.renderPass = m_shadowRp; sfci.attachmentCount = 1; sfci.pAttachments = &m_shadowView[i]; sfci.width = 2048; sfci.height = 2048; sfci.layers = 1; vkCreateFramebuffer(m_dev, &sfci, nullptr, &m_shadowFb[i]);
    }
    m_fbs.resize(m_scViews.size());
    for (size_t i = 0; i < m_scViews.size(); i++) {
        std::array<VkImageView, 5> atts = {m_scViews[i], m_dv, m_gPosView, m_gNormView, m_gAlbedoView};
        VkFramebufferCreateInfo fi{}; fi.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO; fi.renderPass = m_mainRp; fi.attachmentCount = atts.size(); fi.pAttachments = atts.data(); fi.width = m_scExt.width; fi.height = m_scExt.height; fi.layers = 1; vkCreateFramebuffer(m_dev, &fi, nullptr, &m_fbs[i]);
    }
}
void Renderer::initCmdPool() { VkCommandPoolCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO; ci.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT; ci.queueFamilyIndex = m_gfxFam; vkCreateCommandPool(m_dev, &ci, nullptr, &m_cp); }

void Renderer::initDescLayouts() {
    std::array<VkDescriptorSetLayoutBinding, 4> gB{};
    gB[0].binding = 0; gB[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; gB[0].descriptorCount = 1; gB[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    gB[1].binding = 1; gB[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; gB[1].descriptorCount = 1; gB[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    gB[2].binding = 2; gB[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; gB[2].descriptorCount = 1; gB[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    gB[3].binding = 3; gB[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; gB[3].descriptorCount = 1; gB[3].stageFlags = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
    VkDescriptorSetLayoutCreateInfo gCi{}; gCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; gCi.bindingCount = gB.size(); gCi.pBindings = gB.data();
    vkCreateDescriptorSetLayout(m_dev, &gCi, nullptr, &m_geomDsl);

    std::array<VkDescriptorSetLayoutBinding, 5> lB{};
    lB[0].binding = 0; lB[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; lB[0].descriptorCount = 1; lB[0].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lB[1].binding = 1; lB[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; lB[1].descriptorCount = 1; lB[1].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lB[2].binding = 2; lB[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; lB[2].descriptorCount = 1; lB[2].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lB[3].binding = 3; lB[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; lB[3].descriptorCount = 1; lB[3].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    lB[4].binding = 4; lB[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; lB[4].descriptorCount = 1; lB[4].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo lCi{}; lCi.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO; lCi.bindingCount = lB.size(); lCi.pBindings = lB.data();
    vkCreateDescriptorSetLayout(m_dev, &lCi, nullptr, &m_lightDsl);
}

void Renderer::initPipelines() {
    auto vG = makeShader(readFile("shaders/geom.vert.spv"));
    auto tC = makeShader(readFile("shaders/geom.tesc.spv"));
    auto tE = makeShader(readFile("shaders/geom.tese.spv"));
    auto fG = makeShader(readFile("shaders/geom.frag.spv"));
    auto vL = makeShader(readFile("shaders/light.vert.spv"));
    auto fL = makeShader(readFile("shaders/light.frag.spv"));
    auto vS = makeShader(readFile("shaders/shadow.spv"));

    VkPushConstantRange gPr{}; gPr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_FRAGMENT_BIT; gPr.size = sizeof(GeomPush);
    VkPipelineLayoutCreateInfo gPli{}; gPli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; gPli.setLayoutCount = 1; gPli.pSetLayouts = &m_geomDsl; gPli.pushConstantRangeCount = 1; gPli.pPushConstantRanges = &gPr;
    vkCreatePipelineLayout(m_dev, &gPli, nullptr, &m_geomPl);

    VkPushConstantRange sPr{}; sPr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; sPr.size = sizeof(ShadowPush);
    VkPipelineLayoutCreateInfo sPli{}; sPli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; sPli.pushConstantRangeCount = 1; sPli.pPushConstantRanges = &sPr;
    vkCreatePipelineLayout(m_dev, &sPli, nullptr, &m_shadowPl);

    VkPipelineLayoutCreateInfo lPli{}; lPli.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; lPli.setLayoutCount = 1; lPli.pSetLayouts = &m_lightDsl;
    vkCreatePipelineLayout(m_dev, &lPli, nullptr, &m_lightPl);

    auto bd = Vertex::binding(); auto ad = Vertex::attrs();
    VkPipelineVertexInputStateCreateInfo vi{}; vi.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO; vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &bd; vi.vertexAttributeDescriptionCount = ad.size(); vi.pVertexAttributeDescriptions = ad.data();

    VkPipelineInputAssemblyStateCreateInfo ia{}; ia.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    ia.topology = VK_PRIMITIVE_TOPOLOGY_PATCH_LIST;

    VkPipelineTessellationStateCreateInfo ts{}; ts.sType = VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO;
    ts.patchControlPoints = 3;

    VkPipelineViewportStateCreateInfo vp{}; vp.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO; vp.viewportCount = 1; vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs{}; rs.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO; rs.polygonMode = VK_POLYGON_MODE_FILL; rs.cullMode = VK_CULL_MODE_NONE; rs.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; rs.lineWidth = 1.f;
    VkPipelineMultisampleStateCreateInfo ms{}; ms.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO; ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineDepthStencilStateCreateInfo ds{}; ds.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO; ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE; ds.depthCompareOp = VK_COMPARE_OP_LESS;

    std::array<VkPipelineColorBlendAttachmentState, 3> cbG{}; for (int i = 0; i < 3; i++) cbG[i].colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cbGS{}; cbGS.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; cbGS.attachmentCount = cbG.size(); cbGS.pAttachments = cbG.data();

    std::vector<VkDynamicState> dynStates = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dyn{}; dyn.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO; dyn.dynamicStateCount = dynStates.size(); dyn.pDynamicStates = dynStates.data();

    VkPipelineShaderStageCreateInfo ssG[4]{};
    ssG[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ssG[0].pName = "main"; ssG[0].stage = VK_SHADER_STAGE_VERTEX_BIT; ssG[0].module = vG;
    ssG[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ssG[1].pName = "main"; ssG[1].stage = VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT; ssG[1].module = tC;
    ssG[2].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ssG[2].pName = "main"; ssG[2].stage = VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT; ssG[2].module = tE;
    ssG[3].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ssG[3].pName = "main"; ssG[3].stage = VK_SHADER_STAGE_FRAGMENT_BIT; ssG[3].module = fG;

    VkGraphicsPipelineCreateInfo gCi{}; gCi.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    gCi.stageCount = 4; gCi.pStages = ssG;
    gCi.pVertexInputState = &vi; gCi.pInputAssemblyState = &ia; gCi.pTessellationState = &ts; gCi.pViewportState = &vp;
    gCi.pRasterizationState = &rs; gCi.pMultisampleState = &ms; gCi.pDepthStencilState = &ds; gCi.pColorBlendState = &cbGS; gCi.pDynamicState = &dyn;
    gCi.layout = m_geomPl; gCi.renderPass = m_mainRp; gCi.subpass = 0;
    vkCreateGraphicsPipelines(m_dev, VK_NULL_HANDLE, 1, &gCi, nullptr, &m_geomPipe);

    VkPipelineShaderStageCreateInfo ssL[2]{};
    ssL[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ssL[0].pName = "main"; ssL[0].stage = VK_SHADER_STAGE_VERTEX_BIT; ssL[0].module = vL;
    ssL[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ssL[1].pName = "main"; ssL[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; ssL[1].module = fL;
    VkPipelineVertexInputStateCreateInfo viE{}; viE.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo iaTri{}; iaTri.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO; iaTri.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    ds.depthTestEnable = VK_FALSE; ds.depthWriteEnable = VK_FALSE;
    VkPipelineColorBlendAttachmentState cbL{}; cbL.colorWriteMask = 0xF;
    VkPipelineColorBlendStateCreateInfo cbLS{}; cbLS.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO; cbLS.attachmentCount = 1; cbLS.pAttachments = &cbL;

    VkGraphicsPipelineCreateInfo lCi = gCi; lCi.stageCount = 2; lCi.pStages = ssL; lCi.pTessellationState = nullptr;
    lCi.pVertexInputState = &viE; lCi.pInputAssemblyState = &iaTri; lCi.pDepthStencilState = &ds; lCi.pColorBlendState = &cbLS; lCi.layout = m_lightPl; lCi.subpass = 1;
    vkCreateGraphicsPipelines(m_dev, VK_NULL_HANDLE, 1, &lCi, nullptr, &m_lightPipe);

    VkPipelineShaderStageCreateInfo ssS[1]{};
    ssS[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO; ssS[0].pName = "main"; ssS[0].stage = VK_SHADER_STAGE_VERTEX_BIT; ssS[0].module = vS;
    ds.depthTestEnable = VK_TRUE; ds.depthWriteEnable = VK_TRUE; rs.depthBiasEnable = VK_TRUE;
    dynStates.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS); dyn.dynamicStateCount = dynStates.size(); dyn.pDynamicStates = dynStates.data();

    VkGraphicsPipelineCreateInfo sCi = gCi; sCi.stageCount = 1; sCi.pStages = ssS; sCi.pTessellationState = nullptr;
    sCi.pInputAssemblyState = &iaTri; sCi.pDepthStencilState = &ds; sCi.pColorBlendState = nullptr; sCi.layout = m_shadowPl; sCi.renderPass = m_shadowRp; sCi.subpass = 0;
    vkCreateGraphicsPipelines(m_dev, VK_NULL_HANDLE, 1, &sCi, nullptr, &m_shadowPipe);

    vkDestroyShaderModule(m_dev, vG, nullptr); vkDestroyShaderModule(m_dev, tC, nullptr); vkDestroyShaderModule(m_dev, tE, nullptr); vkDestroyShaderModule(m_dev, fG, nullptr);
    vkDestroyShaderModule(m_dev, vL, nullptr); vkDestroyShaderModule(m_dev, fL, nullptr); vkDestroyShaderModule(m_dev, vS, nullptr);
}

void Renderer::createDummyTexture(uint8_t r, uint8_t g, uint8_t b, uint8_t a, VkFormat format, VkImage& img, VkDeviceMemory& mem, VkImageView& view) {
    uint8_t px[4] = {r, g, b, a};
    VkBuffer sb; VkDeviceMemory sm;
    mkBuf(4, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sb, sm);
    void* p; vkMapMemory(m_dev, sm, 0, 4, 0, &p); memcpy(p, px, 4); vkUnmapMemory(m_dev, sm);
    mkImg(1, 1, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, img, mem);
    transitionImg(img, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    cpBufToImg(sb, img, 1, 1);
    transitionImg(img, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkDestroyBuffer(m_dev, sb, nullptr); vkFreeMemory(m_dev, sm, nullptr);
    view = mkView(img, format, VK_IMAGE_ASPECT_COLOR_BIT);
}

void Renderer::initDummyTextures() {
    VkSamplerCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO; si.magFilter = VK_FILTER_LINEAR; si.minFilter = VK_FILTER_LINEAR; si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    vkCreateSampler(m_dev, &si, nullptr, &m_wS);

    createDummyTexture(255, 255, 255, 255, VK_FORMAT_R8G8B8A8_SRGB, m_wImg, m_wMem, m_wV);
    createDummyTexture(128, 128, 255, 255, VK_FORMAT_R8G8B8A8_UNORM, m_flatNormImg, m_flatNormMem, m_flatNormView);
    createDummyTexture(0, 0, 0, 255, VK_FORMAT_R8G8B8A8_UNORM, m_blackImg, m_blackMem, m_blackView);
}

void Renderer::initUBOs() {
    for (int i = 0; i < FRAMES; i++) {
        mkBuf(sizeof(GeomUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_geomUb[i], m_geomUbm[i]);
        vkMapMemory(m_dev, m_geomUbm[i], 0, sizeof(GeomUBO), 0, &m_geomUbp[i]);
        mkBuf(sizeof(GlobalUBO), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, m_globalUb[i], m_globalUbm[i]);
        vkMapMemory(m_dev, m_globalUbm[i], 0, sizeof(GlobalUBO), 0, &m_globalUbp[i]);
    }
}

void Renderer::initDescPool() {
    std::array<VkDescriptorPoolSize, 3> sz = {{
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, (MAX_MESHES + 1) * FRAMES},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, (3 * MAX_MESHES + 1) * FRAMES},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 3 * FRAMES}
    }};
    VkDescriptorPoolCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    ci.poolSizeCount = sz.size(); ci.pPoolSizes = sz.data(); ci.maxSets = (MAX_MESHES + 1) * FRAMES;
    vkCreateDescriptorPool(m_dev, &ci, nullptr, &m_dp);
}

void Renderer::allocLightDescSets() {
    std::vector<VkDescriptorSetLayout> layouts(FRAMES, m_lightDsl); VkDescriptorSetAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_dp; ai.descriptorSetCount = FRAMES; ai.pSetLayouts = layouts.data(); vkAllocateDescriptorSets(m_dev, &ai, m_lightDs.data());
}

void Renderer::updateLightDescSets() {
    for (int i = 0; i < FRAMES; i++) {
        VkDescriptorBufferInfo bI{m_globalUb[i], 0, sizeof(GlobalUBO)};
        VkDescriptorImageInfo  iP{VK_NULL_HANDLE, m_gPosView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo  iN{VK_NULL_HANDLE, m_gNormView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo  iA{VK_NULL_HANDLE, m_gAlbedoView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo  iS{m_shadowSampler, m_shadowView[i], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        std::array<VkWriteDescriptorSet, 5> w{};
        w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[0].dstSet = m_lightDs[i]; w[0].dstBinding = 0; w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[0].descriptorCount = 1; w[0].pBufferInfo = &bI;
        w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[1].dstSet = m_lightDs[i]; w[1].dstBinding = 1; w[1].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; w[1].descriptorCount = 1; w[1].pImageInfo = &iP;
        w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[2].dstSet = m_lightDs[i]; w[2].dstBinding = 2; w[2].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; w[2].descriptorCount = 1; w[2].pImageInfo = &iN;
        w[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[3].dstSet = m_lightDs[i]; w[3].dstBinding = 3; w[3].descriptorType = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT; w[3].descriptorCount = 1; w[3].pImageInfo = &iA;
        w[4].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[4].dstSet = m_lightDs[i]; w[4].dstBinding = 4; w[4].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[4].descriptorCount = 1; w[4].pImageInfo = &iS;
        vkUpdateDescriptorSets(m_dev, w.size(), w.data(), 0, nullptr);
    }
}

void Renderer::initCmdBuffers() {
    m_cmds.resize(FRAMES); VkCommandBufferAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    ai.commandPool = m_cp; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = FRAMES; vkAllocateCommandBuffers(m_dev, &ai, m_cmds.data());
}

void Renderer::initSync() {
    VkSemaphoreCreateInfo si{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO}; VkFenceCreateInfo fi{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO}; fi.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    for (int i = 0; i < FRAMES; i++) { vkCreateSemaphore(m_dev, &si, nullptr, &m_imgReady[i]); vkCreateSemaphore(m_dev, &si, nullptr, &m_renDone[i]); vkCreateFence(m_dev, &fi, nullptr, &m_fence[i]); }
}

void Renderer::allocMeshDescSets(GpuMesh& gm) {
    std::vector<VkDescriptorSetLayout> layouts(FRAMES, m_geomDsl); VkDescriptorSetAllocateInfo ai{}; ai.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    ai.descriptorPool = m_dp; ai.descriptorSetCount = FRAMES; ai.pSetLayouts = layouts.data();
    if (vkAllocateDescriptorSets(m_dev, &ai, gm.ds.data()) != VK_SUCCESS) {
        throw std::runtime_error("Failed to allocate descriptor sets! Increase MAX_MESHES.");
    }
}

void Renderer::writeMeshDescSets(GpuMesh& gm) {
    VkImageView iv0 = gm.hasTex[0] ? gm.texView[0] : m_wV;           VkSampler is0 = gm.hasTex[0] ? gm.texSampler[0] : m_wS;
    VkImageView iv1 = gm.hasTex[1] ? gm.texView[1] : m_flatNormView; VkSampler is1 = gm.hasTex[1] ? gm.texSampler[1] : m_wS;
    VkImageView iv2 = gm.hasTex[2] ? gm.texView[2] : m_blackView;    VkSampler is2 = gm.hasTex[2] ? gm.texSampler[2] : m_wS;

    for (int i = 0; i < FRAMES; i++) {
        VkDescriptorBufferInfo bI{m_geomUb[i], 0, sizeof(GeomUBO)};
        VkDescriptorImageInfo iI0{is0, iv0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo iI1{is1, iv1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        VkDescriptorImageInfo iI2{is2, iv2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        std::array<VkWriteDescriptorSet, 4> w{};
        w[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[0].dstSet = gm.ds[i]; w[0].dstBinding = 0; w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[0].descriptorCount = 1; w[0].pBufferInfo = &bI;
        w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[1].dstSet = gm.ds[i]; w[1].dstBinding = 1; w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[1].descriptorCount = 1; w[1].pImageInfo = &iI0;
        w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[2].dstSet = gm.ds[i]; w[2].dstBinding = 2; w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[2].descriptorCount = 1; w[2].pImageInfo = &iI1;
        w[3].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET; w[3].dstSet = gm.ds[i]; w[3].dstBinding = 3; w[3].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[3].descriptorCount = 1; w[3].pImageInfo = &iI2;
        vkUpdateDescriptorSets(m_dev, w.size(), w.data(), 0, nullptr);
    }
}

Renderer::GpuTex Renderer::loadTexture(const std::string& path, VkFormat format) {
    std::string cacheKey = path + "|" + std::to_string((int)format);
    if (m_texCache.count(cacheKey)) return m_texCache[cacheKey];

    int w, h, ch; stbi_uc* px = stbi_load(path.c_str(), &w, &h, &ch, STBI_rgb_alpha);
    if (!px) {
        std::cerr << "\n[ERROR] Failed to load texture: " << path << "\n"
                  << "        Reason: " << stbi_failure_reason() << "\n"
                  << "        Note: stb_image does NOT support .dds or .tif files.\n";
        return {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
    }

    GpuTex tex;
    VkDeviceSize sz = w * h * 4; VkBuffer sb; VkDeviceMemory sm;
    mkBuf(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sb, sm);

    void* p; vkMapMemory(m_dev, sm, 0, sz, 0, &p); memcpy(p, px, sz); vkUnmapMemory(m_dev, sm); stbi_image_free(px);

    mkImg(w, h, format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT|VK_IMAGE_USAGE_SAMPLED_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, tex.img, tex.mem);
    transitionImg(tex.img, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    cpBufToImg(sb, tex.img, w, h);
    transitionImg(tex.img, format, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    vkDestroyBuffer(m_dev, sb, nullptr); vkFreeMemory(m_dev, sm, nullptr);

    // ОТКЛЮЧАЕМ АНИЗОТРОПИЮ НА ВРЕМЯ (Из-за неё некоторые драйверы выдают черные текстуры)
    VkSamplerCreateInfo si{}; si.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    si.magFilter = VK_FILTER_LINEAR; si.minFilter = VK_FILTER_LINEAR;
    si.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT; si.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    si.anisotropyEnable = VK_FALSE; // <--- БЫЛО TRUE
    si.maxAnisotropy = 1.0f;        // <--- БЫЛО 16.f
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

    tex.view = mkView(tex.img, format, VK_IMAGE_ASPECT_COLOR_BIT);
    vkCreateSampler(m_dev, &si, nullptr, &tex.sampler);

    m_texCache[cacheKey] = tex;
    return tex;
}

void Renderer::uploadMesh(const MeshData& mesh) {
    GpuMesh gm; gm.count = mesh.indices.size(); gm.mat = mesh.material;
    auto upload = [&](const void* data, VkDeviceSize sz, VkBufferUsageFlags usage, VkBuffer& buf, VkDeviceMemory& mem) {
        VkBuffer sb; VkDeviceMemory sm; mkBuf(sz, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, sb, sm);
        void* p; vkMapMemory(m_dev, sm, 0, sz, 0, &p); memcpy(p, data, sz); vkUnmapMemory(m_dev, sm);
        mkBuf(sz, VK_BUFFER_USAGE_TRANSFER_DST_BIT|usage, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, buf, mem);
        cpBuf(sb, buf, sz); vkDestroyBuffer(m_dev, sb, nullptr); vkFreeMemory(m_dev, sm, nullptr);
    };
    upload(mesh.vertices.data(), sizeof(Vertex)*mesh.vertices.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, gm.vb, gm.vm);
    upload(mesh.indices.data(), sizeof(uint32_t)*mesh.indices.size(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT, gm.ib, gm.im);

    if (!mesh.diffuseTex.empty()) { auto tex = loadTexture(mesh.diffuseTex, VK_FORMAT_R8G8B8A8_SRGB);  if(tex.view) { gm.texView[0] = tex.view; gm.texSampler[0] = tex.sampler; gm.hasTex[0] = true; } }
    if (!mesh.normalTex.empty())  { auto tex = loadTexture(mesh.normalTex,  VK_FORMAT_R8G8B8A8_UNORM); if(tex.view) { gm.texView[1] = tex.view; gm.texSampler[1] = tex.sampler; gm.hasTex[1] = true; } }
    if (!mesh.dispTex.empty())    { auto tex = loadTexture(mesh.dispTex,    VK_FORMAT_R8G8B8A8_UNORM); if(tex.view) { gm.texView[2] = tex.view; gm.texSampler[2] = tex.sampler; gm.hasTex[2] = true; } }

    allocMeshDescSets(gm); writeMeshDescSets(gm);
    m_meshes.push_back(std::move(gm));
}

void Renderer::clearMeshes() {
    vkDeviceWaitIdle(m_dev);
    for (auto& gm : m_meshes) {
        vkDestroyBuffer(m_dev, gm.vb, nullptr); vkFreeMemory(m_dev, gm.vm, nullptr);
        vkDestroyBuffer(m_dev, gm.ib, nullptr); vkFreeMemory(m_dev, gm.im, nullptr);
    }
    m_meshes.clear();

    for (auto& [path, tex] : m_texCache) {
        vkDestroySampler(m_dev, tex.sampler, nullptr);
        vkDestroyImageView(m_dev, tex.view, nullptr);
        vkDestroyImage(m_dev, tex.img, nullptr);
        vkFreeMemory(m_dev, tex.mem, nullptr);
    }
    m_texCache.clear();
}

void Renderer::addLight(const Light& light) { if (m_lights.size() < 100) m_lights.push_back(light); }
void Renderer::clearLights() { m_lights.clear(); }
void Renderer::queueDraw(int meshIdx, const glm::mat4& model) { m_draws.push_back({meshIdx, model}); }
void Renderer::clearDraws() { m_draws.clear(); }

void Renderer::renderFrame(glm::vec3 camPos, glm::mat4 view, glm::mat4 proj) {
    vkWaitForFences(m_dev, 1, &m_fence[m_frame], VK_TRUE, UINT64_MAX);
    VkResult r = vkAcquireNextImageKHR(m_dev, m_sc, UINT64_MAX, m_imgReady[m_frame], VK_NULL_HANDLE, &m_imgIdx);
    if (r == VK_ERROR_OUT_OF_DATE_KHR) { recreateSwapchain(); return; }
    vkResetFences(m_dev, 1, &m_fence[m_frame]);

    GeomUBO gUbo{view, proj, glm::vec4(camPos, 1.0f)};
    memcpy(m_geomUbp[m_frame], &gUbo, sizeof(GeomUBO));

    GlobalUBO lUbo{};
    lUbo.view = view; lUbo.proj = proj; lUbo.viewPos = glm::vec4(camPos, 1.0f);
    lUbo.info.x = m_lights.size();
    for (size_t i = 0; i < m_lights.size(); i++) lUbo.lights[i] = m_lights[i];
    memcpy(m_globalUbp[m_frame], &lUbo, sizeof(GlobalUBO));

    auto cmd = m_cmds[m_frame]; vkResetCommandBuffer(cmd, 0);
    VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; vkBeginCommandBuffer(cmd, &bi);

    std::array<VkClearValue, 1> sClr{}; sClr[0].depthStencil = {1.f, 0};
    VkRenderPassBeginInfo sRpi{}; sRpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO; sRpi.renderPass = m_shadowRp;
    sRpi.framebuffer = m_shadowFb[m_frame]; sRpi.renderArea.extent = {2048, 2048};
    sRpi.clearValueCount = sClr.size(); sRpi.pClearValues = sClr.data();
    vkCmdBeginRenderPass(cmd, &sRpi, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_shadowPipe);
    vkCmdSetDepthBias(cmd, 1.25f, 0.0f, 1.75f);

    for (size_t i = 0; i < m_lights.size(); i++) {
        int quad = (int)m_lights[i].info.w; if (quad < 0) continue;
        float qx = (quad % 2) * 1024.f; float qy = (quad / 2) * 1024.f;
        VkViewport vp{qx, qy, 1024.f, 1024.f, 0.f, 1.f}; vkCmdSetViewport(cmd, 0, 1, &vp);
        VkRect2D sc{{(int32_t)qx, (int32_t)qy}, {1024, 1024}}; vkCmdSetScissor(cmd, 0, 1, &sc);

        for (auto& d : m_draws) {
            auto& gm = m_meshes[d.meshIdx];
            ShadowPush sp{m_lights[i].lightSpaceMatrix * d.model};
            vkCmdPushConstants(cmd, m_shadowPl, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(ShadowPush), &sp);
            VkDeviceSize off = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &gm.vb, &off); vkCmdBindIndexBuffer(cmd, gm.ib, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, gm.count, 1, 0, 0, 0);
        }
    }
    vkCmdEndRenderPass(cmd);

    std::array<VkClearValue, 5> mClr{};
    mClr[0].color = {{0.f, 0.f, 0.f, 1.f}}; mClr[1].depthStencil = {1.f, 0}; mClr[2].color = {{0.f, 0.f, 0.f, 0.f}}; mClr[3].color = {{0.f, 0.f, 0.f, 0.f}}; mClr[4].color = {{0.f, 0.f, 0.f, 0.f}};
    VkRenderPassBeginInfo mRpi{}; mRpi.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO; mRpi.renderPass = m_mainRp;
    mRpi.framebuffer = m_fbs[m_imgIdx]; mRpi.renderArea.extent = m_scExt; mRpi.clearValueCount = mClr.size(); mRpi.pClearValues = mClr.data();
    vkCmdBeginRenderPass(cmd, &mRpi, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geomPipe);
    VkViewport mvp{0, 0, (float)m_scExt.width, (float)m_scExt.height, 0, 1}; vkCmdSetViewport(cmd, 0, 1, &mvp);
    VkRect2D msc{{0,0}, m_scExt}; vkCmdSetScissor(cmd, 0, 1, &msc);

    for (auto& d : m_draws) {
        auto& gm = m_meshes[d.meshIdx];
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_geomPl, 0, 1, &gm.ds[m_frame], 0, nullptr);

        // ОБНОВЛЕНО: Передаем 1.0f если есть DispMap, иначе 0.0f
        GeomPush gp{d.model, gm.mat.diffuse, gm.mat.specular, gm.mat.ambient, gm.mat.shininess, gm.hasTex[2] ? 1.0f : 0.0f, {0,0}};

        // ОБНОВЛЕНО: Добавлен флаг тесселятора в маску
        vkCmdPushConstants(cmd, m_geomPl, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(GeomPush), &gp);

        VkDeviceSize off = 0; vkCmdBindVertexBuffers(cmd, 0, 1, &gm.vb, &off); vkCmdBindIndexBuffer(cmd, gm.ib, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, gm.count, 1, 0, 0, 0);
    }

    vkCmdNextSubpass(cmd, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightPipe);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, m_lightPl, 0, 1, &m_lightDs[m_frame], 0, nullptr);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    vkCmdEndRenderPass(cmd);

    vkEndCommandBuffer(cmd);
    VkPipelineStageFlags wait = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo si{}; si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO; si.waitSemaphoreCount = 1; si.pWaitSemaphores = &m_imgReady[m_frame];
    si.pWaitDstStageMask = &wait; si.commandBufferCount = 1; si.pCommandBuffers = &cmd; si.signalSemaphoreCount = 1; si.pSignalSemaphores = &m_renDone[m_frame];
    vkQueueSubmit(m_gfxQ, 1, &si, m_fence[m_frame]);
    VkPresentInfoKHR pi{}; pi.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR; pi.waitSemaphoreCount = 1; pi.pWaitSemaphores = &m_renDone[m_frame];
    pi.swapchainCount = 1; pi.pSwapchains = &m_sc; pi.pImageIndices = &m_imgIdx;
    r = vkQueuePresentKHR(m_presQ, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR || m_win.resized()) { m_win.clearResize(); recreateSwapchain(); }
    m_frame = (m_frame + 1) % FRAMES;
}

void Renderer::destroySwapchain() {
    for (int i = 0; i < FRAMES; i++) { vkDestroyFramebuffer(m_dev, m_shadowFb[i], nullptr); vkDestroyImageView(m_dev, m_shadowView[i], nullptr); vkDestroyImage(m_dev, m_shadowImg[i], nullptr); vkFreeMemory(m_dev, m_shadowMem[i], nullptr); }
    vkDestroyImageView(m_dev, m_gPosView, nullptr); vkDestroyImage(m_dev, m_gPosImg, nullptr); vkFreeMemory(m_dev, m_gPosMem, nullptr);
    vkDestroyImageView(m_dev, m_gNormView, nullptr); vkDestroyImage(m_dev, m_gNormImg, nullptr); vkFreeMemory(m_dev, m_gNormMem, nullptr);
    vkDestroyImageView(m_dev, m_gAlbedoView, nullptr); vkDestroyImage(m_dev, m_gAlbedoImg, nullptr); vkFreeMemory(m_dev, m_gAlbedoMem, nullptr);
    vkDestroyImageView(m_dev, m_dv, nullptr); vkDestroyImage(m_dev, m_di, nullptr); vkFreeMemory(m_dev, m_dm, nullptr);
    for (auto fb : m_fbs) vkDestroyFramebuffer(m_dev, fb, nullptr); for (auto iv : m_scViews) vkDestroyImageView(m_dev, iv, nullptr); vkDestroySwapchainKHR(m_dev, m_sc, nullptr);
    m_fbs.clear(); m_scViews.clear(); m_scImgs.clear();
}

void Renderer::recreateSwapchain() {
    int w = 0, h = 0; while (!w || !h) { glfwGetFramebufferSize(m_win.handle(), &w, &h); glfwWaitEvents(); }
    vkDeviceWaitIdle(m_dev); destroySwapchain(); initSwapchain(); initImageViews(); initShadowMap(); initGBuffers(); initFramebuffers(); updateLightDescSets();
}

Renderer::QFI Renderer::findQFI(VkPhysicalDevice dev) {
    QFI qi; uint32_t n; vkGetPhysicalDeviceQueueFamilyProperties(dev, &n, nullptr);
    std::vector<VkQueueFamilyProperties> fams(n); vkGetPhysicalDeviceQueueFamilyProperties(dev, &n, fams.data());
    for (uint32_t i = 0; i < n; i++) {
        if (fams[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) qi.gfx = i; VkBool32 ps = false; vkGetPhysicalDeviceSurfaceSupportKHR(dev, i, m_surf, &ps);
        if (ps) qi.present = i; if (qi.ok()) break;
    } return qi;
}

Renderer::SCSupport Renderer::querySC(VkPhysicalDevice dev) {
    SCSupport s; vkGetPhysicalDeviceSurfaceCapabilitiesKHR(dev, m_surf, &s.caps); uint32_t n; vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surf, &n, nullptr);
    s.fmts.resize(n); vkGetPhysicalDeviceSurfaceFormatsKHR(dev, m_surf, &n, s.fmts.data()); vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surf, &n, nullptr);
    s.modes.resize(n); vkGetPhysicalDeviceSurfacePresentModesKHR(dev, m_surf, &n, s.modes.data()); return s;
}

bool Renderer::deviceOk(VkPhysicalDevice dev) {
    if (!findQFI(dev).ok()) return false;
    uint32_t n; vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, nullptr); std::vector<VkExtensionProperties> exts(n); vkEnumerateDeviceExtensionProperties(dev, nullptr, &n, exts.data());
    for (auto& req : kDevExts) { bool found = false; for (auto& e : exts) if (!strcmp(e.extensionName, req)) { found = true; break; } if (!found) return false; }
    auto sc = querySC(dev); if (sc.fmts.empty() || sc.modes.empty()) return false;
    VkPhysicalDeviceFeatures f; vkGetPhysicalDeviceFeatures(dev, &f);
    return f.samplerAnisotropy && f.tessellationShader;
}

VkSurfaceFormatKHR Renderer::pickFmt(const std::vector<VkSurfaceFormatKHR>& fmts) { for (auto& f : fmts) if (f.format == VK_FORMAT_B8G8R8A8_SRGB && f.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) return f; return fmts[0]; }
VkPresentModeKHR Renderer::pickMode(const std::vector<VkPresentModeKHR>& modes) { for (auto m : modes) if (m == VK_PRESENT_MODE_MAILBOX_KHR) return m; return VK_PRESENT_MODE_FIFO_KHR; }
VkExtent2D Renderer::pickExtent(const VkSurfaceCapabilitiesKHR& caps) {
    if (caps.currentExtent.width != UINT32_MAX) return caps.currentExtent; int w, h; glfwGetFramebufferSize(m_win.handle(), &w, &h);
    return { std::clamp((uint32_t)w, caps.minImageExtent.width, caps.maxImageExtent.width), std::clamp((uint32_t)h, caps.minImageExtent.height, caps.maxImageExtent.height) };
}
VkFormat Renderer::depthFmt() { return findFmt({VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT}, VK_IMAGE_TILING_OPTIMAL, VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT); }
VkFormat Renderer::findFmt(const std::vector<VkFormat>& cands, VkImageTiling tiling, VkFormatFeatureFlags feat) {
    for (auto f : cands) { VkFormatProperties p; vkGetPhysicalDeviceFormatProperties(m_gpu, f, &p); if (tiling == VK_IMAGE_TILING_OPTIMAL && (p.optimalTilingFeatures & feat) == feat) return f; }
    throw std::runtime_error("");
}
uint32_t Renderer::memType(uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mp; vkGetPhysicalDeviceMemoryProperties(m_gpu, &mp);
    for (uint32_t i = 0; i < mp.memoryTypeCount; i++) if ((filter & (1<<i)) && (mp.memoryTypes[i].propertyFlags & props) == props) return i; throw std::runtime_error("");
}
VkShaderModule Renderer::makeShader(const std::vector<char>& code) { VkShaderModuleCreateInfo ci{}; ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO; ci.codeSize = code.size(); ci.pCode = reinterpret_cast<const uint32_t*>(code.data()); VkShaderModule m; if (vkCreateShaderModule(m_dev, &ci, nullptr, &m) != VK_SUCCESS) throw std::runtime_error(""); return m; }
std::vector<char> Renderer::readFile(const std::string& path) { std::ifstream f(path, std::ios::ate|std::ios::binary); if (!f) throw std::runtime_error(""); size_t sz = f.tellg(); std::vector<char> buf(sz); f.seekg(0); f.read(buf.data(), sz); return buf; }
void Renderer::mkBuf(VkDeviceSize sz, VkBufferUsageFlags usage, VkMemoryPropertyFlags props, VkBuffer& buf, VkDeviceMemory& mem) { VkBufferCreateInfo ci{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO}; ci.size = sz; ci.usage = usage; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE; vkCreateBuffer(m_dev, &ci, nullptr, &buf); VkMemoryRequirements req; vkGetBufferMemoryRequirements(m_dev, buf, &req); VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize = req.size; ai.memoryTypeIndex = memType(req.memoryTypeBits, props); vkAllocateMemory(m_dev, &ai, nullptr, &mem); vkBindBufferMemory(m_dev, buf, mem, 0); }
void Renderer::cpBuf(VkBuffer src, VkBuffer dst, VkDeviceSize sz) { auto cmd = beginOnce(); VkBufferCopy c{0, 0, sz}; vkCmdCopyBuffer(cmd, src, dst, 1, &c); endOnce(cmd); }
void Renderer::mkImg(uint32_t w, uint32_t h, VkFormat fmt, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags props, VkImage& img, VkDeviceMemory& mem) { VkImageCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO}; ci.imageType = VK_IMAGE_TYPE_2D; ci.format = fmt; ci.extent = {w,h,1}; ci.mipLevels = 1; ci.arrayLayers = 1; ci.samples = VK_SAMPLE_COUNT_1_BIT; ci.tiling = tiling; ci.usage = usage; ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE; vkCreateImage(m_dev, &ci, nullptr, &img); VkMemoryRequirements req; vkGetImageMemoryRequirements(m_dev, img, &req); VkMemoryAllocateInfo ai{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO}; ai.allocationSize = req.size; ai.memoryTypeIndex = memType(req.memoryTypeBits, props); vkAllocateMemory(m_dev, &ai, nullptr, &mem); vkBindImageMemory(m_dev, img, mem, 0); }
VkImageView Renderer::mkView(VkImage img, VkFormat fmt, VkImageAspectFlags aspect) { VkImageViewCreateInfo ci{VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO}; ci.image = img; ci.viewType = VK_IMAGE_VIEW_TYPE_2D; ci.format = fmt; ci.subresourceRange = {aspect, 0, 1, 0, 1}; VkImageView v; vkCreateImageView(m_dev, &ci, nullptr, &v); return v; }

void Renderer::transitionImg(VkImage img, VkFormat, VkImageLayout from, VkImageLayout to) {
    auto cmd = beginOnce();
    VkImageMemoryBarrier b{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    b.oldLayout = from; b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img; b.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    VkPipelineStageFlags src, dst;
    if (from == VK_IMAGE_LAYOUT_UNDEFINED) {
        b.srcAccessMask = 0; b.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        src = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; dst = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else {
        b.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT; b.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        src = VK_PIPELINE_STAGE_TRANSFER_BIT;
        dst = VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT; // ИСПРАВЛЕНИЕ: Барьер для всех стадий (включая Tessellation Shader)
    }
    vkCmdPipelineBarrier(cmd, src, dst, 0, 0, nullptr, 0, nullptr, 1, &b);
    endOnce(cmd);
}

void Renderer::cpBufToImg(VkBuffer buf, VkImage img, uint32_t w, uint32_t h) { auto cmd = beginOnce(); VkBufferImageCopy r{}; r.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1}; r.imageExtent = {w, h, 1}; vkCmdCopyBufferToImage(cmd, buf, img, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &r); endOnce(cmd); }
VkCommandBuffer Renderer::beginOnce() { VkCommandBufferAllocateInfo ai{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO}; ai.commandPool = m_cp; ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY; ai.commandBufferCount = 1; VkCommandBuffer cmd; vkAllocateCommandBuffers(m_dev, &ai, &cmd); VkCommandBufferBeginInfo bi{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO}; bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; vkBeginCommandBuffer(cmd, &bi); return cmd; }
void Renderer::endOnce(VkCommandBuffer cmd) { vkEndCommandBuffer(cmd); VkSubmitInfo si{VK_STRUCTURE_TYPE_SUBMIT_INFO}; si.commandBufferCount = 1; si.pCommandBuffers = &cmd; vkQueueSubmit(m_gfxQ, 1, &si, VK_NULL_HANDLE); vkQueueWaitIdle(m_gfxQ); vkFreeCommandBuffers(m_dev, m_cp, 1, &cmd); }
