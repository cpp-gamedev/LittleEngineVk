#include <glm/gtx/quaternion.hpp>
#include "core/io.hpp"
#include "core/jobs.hpp"
#include "core/log.hpp"
#include "core/maths.hpp"
#include "core/map_store.hpp"
#include "core/transform.hpp"
#include "core/services.hpp"
#include "engine/levk.hpp"
#include "engine/window/window.hpp"
#include "vuk/info.hpp"
#include "vuk/context.hpp"
#include "vuk/utils.hpp"
#include "vuk/shader.hpp"
#include "vuk/draw/vertex.hpp"
#include "window/window_impl.hpp"

namespace le
{
namespace
{
std::unique_ptr<IOReader> g_uReader;
}

s32 engine::run(s32 argc, char** argv)
{
	Services services;
	try
	{
		services.add<os::Service, os::Args>({argc, argv});
		services.add<log::Service, std::string_view>("debug.log");
		services.add<jobs::Service, u8>(4);
		services.add<Window::Service>();

		NativeWindow dummyWindow({});
		vuk::InitData initData;
#if 1 || defined(LEVK_DEBUG) // Always enabled, for time being
		initData.options.flags.set(vuk::InitData::Flag::eValidation);
#endif
		initData.config.instanceExtensions = WindowImpl::vulkanInstanceExtensions();
		initData.config.createTempSurface = [&](vk::Instance instance) { return WindowImpl::createSurface(instance, dummyWindow); };

		services.add<vuk::Service>(std::move(initData));
	}
	catch (std::exception const& e)
	{
		LOG_E("Failed to initialise services: {}", e.what());
		return 1;
	}

	auto const exeDirectory = os::dirPath(os::Dir::eExecutable);
	auto [result, dataPath] = FileReader::findUpwards(exeDirectory, {"data"});
	LOGIF_I(true, "Executable located at: {}", exeDirectory.generic_string());
	if (result == IOReader::Code::eSuccess)
	{
		LOG_D("Found data at: {}", dataPath.generic_string());
	}
	else
	{
		LOG_E("Could not locate data!");
		return 1;
	}
	g_uReader = std::make_unique<FileReader>(dataPath);
	for (s32 i = 0; i < 10; ++i)
	{
		jobs::enqueue([i]() {
			std::this_thread::sleep_for(stdch::milliseconds(maths::randomRange(10, 1000)));
			log::fmtLog(log::Level::eDebug, "Job #{}", __FILE__, __LINE__, i);
		});
	}
	try
	{
		vuk::Shader::Data tutorialData;
		std::array shaderIDs = {stdfs::path("shaders/tutorial.vert.spv"), stdfs::path("shaders/tutorial.frag.spv")};
		ASSERT(g_uReader->checkPresences(ArrayView<stdfs::path const>(shaderIDs)), "Shaders missing!");
		tutorialData.pReader = g_uReader.get();
		tutorialData.codeIDMap[vuk::Shader::Type::eVertex] = shaderIDs.at(0);
		tutorialData.codeIDMap[vuk::Shader::Type::eFragment] = shaderIDs.at(1);
		vuk::Shader tutorialShader(std::move(tutorialData));

		vuk::Vertex const triangle0Verts[] = {
			vuk::Vertex{{0.0f, -0.5f}, {1.0f, 0.0f, 0.0f}},
			vuk::Vertex{{0.5f, 0.5f}, {0.0f, 1.0f, 0.0f}},
			vuk::Vertex{{-0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
		};

		vuk::Vertex const quad0Verts[] = {{{-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f}},
										  {{0.5f, -0.5f}, {0.0f, 1.0f, 0.0f}},
										  {{0.5f, 0.5f}, {0.0f, 0.0f, 1.0f}},
										  {{-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f}}};
		u32 const quad0Indices[] = {0, 1, 2, 2, 3, 0};

		auto createStagingBuffer = [](vk::DeviceSize size) -> vuk::VkResource<vk::Buffer> {
			vuk::BufferData data;
			data.size = size;
			data.properties = vk::MemoryPropertyFlagBits::eHostVisible | vk::MemoryPropertyFlagBits::eHostCoherent;
			data.usage = vk::BufferUsageFlagBits::eTransferSrc;
			return vuk::createBuffer(data);
		};
		auto createDeviceBuffer = [](vk::DeviceSize size, vk::BufferUsageFlags flags) -> vuk::VkResource<vk::Buffer> {
			vuk::BufferData data;
			data.size = size;
			data.properties = vk::MemoryPropertyFlagBits::eDeviceLocal;
			data.usage = flags | vk::BufferUsageFlagBits::eTransferDst;
			return vuk::createBuffer(data);
		};
		auto copyBuffer = [](vk::Buffer src, vk::Buffer dst, vk::DeviceSize size, vk::Queue queue,
							 vk::CommandPool pool) -> vuk::TransferOp {
			vuk::TransferOp ret{queue, pool, {}, {}};
			vuk::copyBuffer(src, dst, size, &ret);
			return ret;
		};

		vk::CommandPoolCreateInfo commandPoolCreateInfo;
		auto const& qfi = vuk::g_info.queueFamilyIndices;
		commandPoolCreateInfo.queueFamilyIndex = qfi.transfer;
		auto transferPool = vuk::g_info.device.createCommandPool(commandPoolCreateInfo);

		auto q = vuk::g_info.queues.transfer;
		vuk::VkResource<vk::Buffer> triangle0VB, quad0VBIB;
		{
			auto d = vuk::g_info.device;
			std::vector<vuk::TransferOp> ops;
			auto const t0vbSize = sizeof(triangle0Verts);
			auto const q0vbSize = sizeof(quad0Verts);
			auto const q0ibSize = sizeof(quad0Indices);
			auto const q0vbibSize = q0vbSize + q0ibSize;
			auto tri0stage = createStagingBuffer(t0vbSize);
			auto quad0stage = createStagingBuffer(q0vbibSize);
			triangle0VB = createDeviceBuffer(t0vbSize, vk::BufferUsageFlagBits::eVertexBuffer);
			quad0VBIB = createDeviceBuffer(q0vbibSize, vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eIndexBuffer);
			auto pMem = d.mapMemory(tri0stage.memory, 0, t0vbSize);
			std::memcpy(pMem, triangle0Verts, t0vbSize);
			d.unmapMemory(tri0stage.memory);
			pMem = vuk::g_info.device.mapMemory(quad0stage.memory, 0, q0vbSize);
			std::memcpy(pMem, quad0Verts, q0vbSize);
			vuk::g_info.device.unmapMemory(quad0stage.memory);
			pMem = vuk::g_info.device.mapMemory(quad0stage.memory, q0vbSize, q0ibSize);
			std::memcpy(pMem, quad0Indices, q0ibSize);
			vuk::g_info.device.unmapMemory(quad0stage.memory);
			ops.push_back(copyBuffer(tri0stage.resource, triangle0VB.resource, t0vbSize, q, transferPool));
			ops.push_back(copyBuffer(quad0stage.resource, quad0VBIB.resource, q0vbibSize, q, transferPool));
			std::vector<vk::Fence> fences;
			std::for_each(ops.begin(), ops.end(), [&fences](auto const& op) { fences.push_back(op.transferred); });
			vuk::g_info.waitAll(fences);
			for (auto& op : ops)
			{
				vuk::vkDestroy(op.transferred);
				vuk::g_info.device.freeCommandBuffers(op.pool, op.commandBuffer);
			}
			vuk::vkDestroy(tri0stage, quad0stage);
		}

		Window w0, w1;
		Window::Data data0;
		data0.config.size = {1280, 720};
		data0.config.title = "LittleEngineVk Demo";
		auto data1 = data0;
		data1.config.title += " 2";
		data1.config.centreOffset = {100, 100};
		// data1.options.colourSpace = ColourSpace::eRGBLinear;
		bool bRecreate0 = false, bRecreate1 = false;
		bool bClose0 = false, bClose1 = false;
		auto registerInput = [](Window& self, Window& other, bool& bRecreate, bool& bClose, std::shared_ptr<int>& token) {
			token = self.registerInput([&](Key key, Action action, Mods mods) {
				if (self.isOpen() && key == Key::eW && action == Action::eRelease && mods & Mods::eCONTROL)
				{
					bClose = true;
				}
				if (!other.isOpen() && (key == Key::eT || key == Key::eN) && action == Action::eRelease && mods & Mods::eCONTROL)
				{
					bRecreate = true;
				}
			});
		};
		std::shared_ptr<s32> token0, token1;
		registerInput(w0, w1, bRecreate1, bClose0, token0);
		registerInput(w1, w0, bRecreate0, bClose1, token1);
		auto createRenderer = [&tutorialShader](vk::Pipeline* pPipeline, vk::PipelineLayout* pLayout, vuk::Context** ppContext,
												WindowID id) {
			vuk::vkDestroy(*pPipeline, *pLayout);
			*ppContext = WindowImpl::context(id);
			if (!*ppContext)
			{
				return;
			}
			vk::PipelineLayoutCreateInfo layoutCreateInfo;
			vk::DescriptorSetLayout setLayout = vuk::g_info.matricesLayout;
			layoutCreateInfo.setLayoutCount = 1;
			layoutCreateInfo.pSetLayouts = &setLayout;
			*pLayout = vuk::g_info.device.createPipelineLayout(layoutCreateInfo);
			vuk::PipelineData pipelineData;
			pipelineData.pShader = &tutorialShader;
			pipelineData.renderPass = (*ppContext)->m_renderPass;
			*pPipeline = vuk::createPipeline(*pLayout, pipelineData);
			return;
		};

		if (w0.create(data0) && w1.create(data1))
		{
			vuk::Context* pContext0 = nullptr;
			vuk::Context* pContext1 = nullptr;
			vk::PipelineLayout layout0, layout1;
			vk::Pipeline pipeline0;
			vk::Pipeline pipeline1;

			createRenderer(&pipeline0, &layout0, &pContext0, w0.id());
			createRenderer(&pipeline1, &layout1, &pContext1, w1.id());

			vuk::MatricesUBO mats0;
			vuk::MatricesUBO mats1;
			Transform transform0;

			Time t = Time::elapsed();
			while (w0.isOpen() || w1.isOpen())
			{
				[[maybe_unused]] Time dt = Time::elapsed() - t;
				t = Time::elapsed();

				{
					// Update matrices
					transform0.setOrientation(
						glm::rotate(transform0.orientation(), glm::radians(dt.to_s() * 10), glm::vec3(0.0f, 1.0f, 0.0f)));
					mats0.mat_m = mats1.mat_m = transform0.model();
					mats0.mat_v = mats1.mat_v =
						glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
					if (w0.isOpen())
					{
						auto const size = w0.framebufferSize();
						auto proj = glm::perspective(glm::radians(45.0f), (f32)size.x / size.y, 0.1f, 10.0f);
						proj[1][1] *= -1;
						mats0.mat_vp = proj * mats0.mat_v;
					}
					if (w1.isOpen())
					{
						auto const size = w1.framebufferSize();
						auto proj = glm::perspective(glm::radians(45.0f), (f32)size.x / size.y, 0.1f, 10.0f);
						proj[1][1] *= -1;
						mats1.mat_vp = proj * mats1.mat_v;
					}
				}

				std::this_thread::sleep_for(stdch::milliseconds(10));
				if (w0.isClosing())
				{
					w0.destroy();
				}
				if (w1.isClosing())
				{
					w1.destroy();
				}
				if (bRecreate0)
				{
					bRecreate0 = false;
					w0.create(data0);
					createRenderer(&pipeline0, &layout0, &pContext0, w0.id());
				}
				if (bRecreate1)
				{
					bRecreate1 = false;
					w1.create(data1);
					createRenderer(&pipeline1, &layout1, &pContext1, w1.id());
				}
				if (bClose0)
				{
					bClose0 = false;
					w0.close();
				}
				if (bClose1)
				{
					bClose1 = false;
					w1.close();
				}
				Window::pollEvents();
				// Render
				try
				{
					auto drawFrame = [](vuk::MatricesUBO* pMats, vuk::Context* pContext, vk::Pipeline pipeline, vk::PipelineLayout layout,
										vk::Buffer vertexBuffer, u32 vertCount, u32 indexCount) -> bool {
						vuk::BeginPass pass;
						pass.ubos.mats = *pMats;
						pass.pipelineLayout = layout;
						return pContext->renderFrame(
							[&](vuk::Context::FrameDriver driver) {
								vk::Viewport viewport = pContext->transformViewport();
								vk::Rect2D scissor = pContext->transformScissor();
								driver.commandBuffer.setViewport(0, viewport);
								driver.commandBuffer.setScissor(0, scissor);
								driver.commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
								vk::DeviceSize offsets[] = {0};
								driver.commandBuffer.bindVertexBuffers(0, 1, &vertexBuffer, offsets);
								if (indexCount > 0)
								{
									vk::DeviceSize offset = vertCount * sizeof(vuk::Vertex);
									driver.commandBuffer.bindIndexBuffer(vertexBuffer, offset, vk::IndexType::eUint32);
									driver.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, driver.matrices,
																			{});
									driver.commandBuffer.drawIndexed(indexCount, 1, 0, 0, 0);
								}
								else
								{
									driver.commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, driver.matrices,
																			{});
									driver.commandBuffer.draw(vertCount, 1, 0, 0);
								}
							},
							pass);
					};

					if (w0.isOpen())
					{
						auto const size = w0.framebufferSize();
						auto proj = glm::perspective(glm::radians(45.0f), (f32)size.x / size.y, 0.1f, 10.0f);
						proj[1][1] *= -1;
						mats0.mat_vp = proj * mats0.mat_v;
						drawFrame(&mats0, pContext0, pipeline0, layout0, quad0VBIB.resource, (u32)arraySize(quad0Verts),
								  (u32)arraySize(quad0Indices));
					}
					if (w1.isOpen())
					{
						// drawFrame(pContext1, pipeline1, triangle0VB.resource, (u32)arraySize(triangle0Verts), 0);
						drawFrame(&mats1, pContext1, pipeline1, layout1, quad0VBIB.resource, (u32)arraySize(quad0Verts),
								  (u32)arraySize(quad0Indices));
					}
				}
				catch (std::exception const& e)
				{
					LOG_E("EXCEPTION!\n\t{}", e.what());
				}
			}
			vuk::g_info.device.waitIdle();
			vuk::vkDestroy(pipeline0, pipeline1, layout0, layout1);
			vuk::vkDestroy(transferPool);
			vuk::g_info.device.destroyBuffer(triangle0VB.resource);
			vuk::g_info.device.destroyBuffer(quad0VBIB.resource);
			vuk::g_info.device.freeMemory(triangle0VB.memory);
			vuk::g_info.device.freeMemory(quad0VBIB.memory);
		}
	}
	catch (std::exception const& e)
	{
		LOG_E("Exception!\n\t{}", e.what());
	}
	std::this_thread::sleep_for(stdch::milliseconds(maths::randomRange(10, 1000)));
	return 0;
}
} // namespace le
