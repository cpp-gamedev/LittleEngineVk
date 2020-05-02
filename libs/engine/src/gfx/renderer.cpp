#include <unordered_set>
#include <glm/gtc/matrix_transform.hpp>
#include "core/assert.hpp"
#include "core/log.hpp"
#include "core/transform.hpp"
#include "core/utils.hpp"
#include "engine/assets/resources.hpp"
#include "engine/gfx/mesh.hpp"
#include "info.hpp"
#include "utils.hpp"
#include "pipeline_impl.hpp"
#include "renderer_impl.hpp"
#include "resource_descriptors.hpp"

namespace le::gfx
{
ScreenRect::ScreenRect(glm::vec4 const& ltrb) noexcept : left(ltrb.x), top(ltrb.y), right(ltrb.z), bottom(ltrb.w) {}

ScreenRect::ScreenRect(glm::vec2 const& size, glm::vec2 const& leftTop) noexcept
	: left(leftTop.x), top(leftTop.y), right(leftTop.x + size.x), bottom(leftTop.y + size.y)
{
}

f32 ScreenRect::aspect() const
{
	glm::vec2 const size = {right - left, bottom - top};
	return size.x / size.y;
}

Renderer::Renderer() = default;
Renderer::Renderer(Renderer&&) = default;
Renderer& Renderer::operator=(Renderer&&) = default;
Renderer::~Renderer() = default;

std::string const Renderer::s_tName = utils::tName<Renderer>();

Pipeline* Renderer::createPipeline(Pipeline::Info info)
{
	if (m_uImpl)
	{
		return m_uImpl->createPipeline(std::move(info));
	}
	return nullptr;
}

void Renderer::update()
{
	if (m_uImpl)
	{
		m_uImpl->update();
	}
}

void Renderer::render(Scene const& scene)
{
	if (m_uImpl)
	{
		m_uImpl->render(scene);
	}
}

RendererImpl::RendererImpl(Info const& info) : m_presenter(info.presenterInfo), m_window(info.windowID)
{
	m_name = fmt::format("{}:{}", Renderer::s_tName, m_window);
	create(info.frameCount);
}

RendererImpl::~RendererImpl()
{
	m_pipelines.clear();
	destroy();
}

void RendererImpl::create(u8 frameCount)
{
	if (m_frames.empty() && frameCount > 0)
	{
		m_frameCount = frameCount;
		// Descriptors
		auto descriptorSetup = rd::allocateSets(frameCount);
		ASSERT(descriptorSetup.set.size() == (size_t)frameCount, "Invalid setup!");
		m_descriptorPool = descriptorSetup.descriptorPool;
		for (u8 idx = 0; idx < frameCount; ++idx)
		{
			RendererImpl::FrameSync frame;
			frame.set = descriptorSetup.set.at((size_t)idx);
			frame.renderReady = g_info.device.createSemaphore({});
			frame.presentReady = g_info.device.createSemaphore({});
			frame.drawing = g_info.device.createFence({});
			// Commands
			vk::CommandPoolCreateInfo commandPoolCreateInfo;
			commandPoolCreateInfo.queueFamilyIndex = g_info.queueFamilyIndices.graphics;
			commandPoolCreateInfo.flags = vk::CommandPoolCreateFlagBits::eResetCommandBuffer | vk::CommandPoolCreateFlagBits::eTransient;
			frame.commandPool = g_info.device.createCommandPool(commandPoolCreateInfo);
			vk::CommandBufferAllocateInfo allocInfo;
			allocInfo.commandPool = frame.commandPool;
			allocInfo.level = vk::CommandBufferLevel::ePrimary;
			allocInfo.commandBufferCount = 1;
			frame.commandBuffer = g_info.device.allocateCommandBuffers(allocInfo).front();
			m_frames.push_back(std::move(frame));
		}
		LOG_D("[{}] created", m_name);
	}
	return;
}

void RendererImpl::destroy()
{
	for (auto& pipeline : m_pipelines)
	{
		pipeline.m_uImpl->m_activeFences.clear();
	}
	if (!m_frames.empty())
	{
		for (auto& frame : m_frames)
		{
			frame.set.destroy();
			vkDestroy(frame.commandPool, frame.framebuffer, frame.drawing, frame.renderReady, frame.presentReady);
		}
		vkDestroy(m_descriptorPool);
		m_descriptorPool = vk::DescriptorPool();
		m_frames.clear();
		m_index = 0;
		LOG_D("[{}] destroyed", m_name);
	}
	return;
}

void RendererImpl::reset()
{
	if (m_frameCount > 0)
	{
		create(m_frameCount);
		g_info.device.waitIdle();
		for (auto& frame : m_frames)
		{
			frame.bNascent = true;
			g_info.device.resetFences(frame.drawing);
			g_info.device.resetCommandPool(frame.commandPool, vk::CommandPoolResetFlagBits::eReleaseResources);
		}
		LOG_D("[{}] reset", m_name);
	}
	return;
}

Pipeline* RendererImpl::createPipeline(Pipeline::Info info)
{
	PipelineImpl::Info implInfo;
	implInfo.renderPass = m_presenter.m_renderPass;
	implInfo.pShader = info.pShader;
	implInfo.setLayouts = {rd::g_setLayout};
	implInfo.name = info.name;
	implInfo.polygonMode = g_polygonModeMap.at((size_t)info.polygonMode);
	implInfo.cullMode = g_cullModeMap.at((size_t)info.cullMode);
	implInfo.staticLineWidth = info.lineWidth;
	implInfo.bBlend = info.bBlend;
	implInfo.window = m_window;
	vk::PushConstantRange pcRange;
	pcRange.size = sizeof(rd::PushConstants);
	pcRange.stageFlags = vkFlags::vertFragShader;
	implInfo.pushConstantRanges = {pcRange};
	m_pipelines.push_back({});
	auto& pipeline = m_pipelines.back();
	pipeline.m_uImpl = std::make_unique<PipelineImpl>(&pipeline);
	if (!pipeline.m_uImpl->create(std::move(implInfo)))
	{
		m_pipelines.pop_back();
		return nullptr;
	}
	return &pipeline;
}

void RendererImpl::update()
{
	switch (m_presenter.m_state)
	{
	case Presenter::State::eDestroyed:
	{
		destroy();
		return;
	}
	case Presenter::State::eSwapchainDestroyed:
	{
		destroy();
		return;
	}
	case Presenter::State::eSwapchainRecreated:
	{
		destroy();
		create(m_frameCount);
		break;
	}
	default:
	{
		break;
	}
	}
	for (auto& pipeline : m_pipelines)
	{
		pipeline.m_uImpl->update();
	}
	return;
}

bool RendererImpl::render(Renderer::Scene const& scene)
{
	auto const mg = colours::Magenta;
	auto& frame = frameSync();
	if (!frame.bNascent)
	{
		waitFor(frame.drawing);
	}
	// Write sets
	u32 objectID = 0;
	u32 diffuseID = 0;
	u32 specularID = 0;
	rd::SSBOs ssbos;
	std::vector<std::vector<rd::PushConstants>> pushConstants;
	pushConstants.reserve(scene.batches.size());
	frame.set.update();
	frame.set.writeDiffuse(*g_pResources->get<Texture>("textures/white"), diffuseID++);
	frame.set.writeSpecular(*g_pResources->get<Texture>("textures/black"), specularID++);
	for (auto& batch : scene.batches)
	{
		pushConstants.push_back({});
		pushConstants.back().reserve(batch.drawables.size());
		for (auto [pMesh, pTransform, _] : batch.drawables)
		{
			rd::PushConstants pc;
			pc.objectID = objectID;
			ssbos.models.ssbo.push_back(pTransform->model());
			ssbos.normals.ssbo.push_back(pTransform->normalModel());
			ssbos.materials.ssbo.push_back(*pMesh->m_material.pMaterial);
			ssbos.tints.ssbo.push_back(pMesh->m_material.tint.toVec4());
			ssbos.flags.ssbo.push_back(0);
			if (pMesh->m_material.flags.isSet(Material::Flag::eLit))
			{
				ssbos.flags.ssbo.at(objectID) |= rd::SSBOFlags::eLIT;
			}
			if (pMesh->m_material.flags.isSet(Material::Flag::eOpaque))
			{
				ssbos.flags.ssbo.at(objectID) |= rd::SSBOFlags::eOPAQUE;
			}
			if (pMesh->m_material.flags.isSet(Material::Flag::eTextured))
			{
				ssbos.flags.ssbo.at(objectID) |= rd::SSBOFlags::eTEXTURED;
				if (!pMesh->m_material.pDiffuse)
				{
					ssbos.tints.ssbo.at(objectID) = mg.toVec4();
					pc.diffuseID = 0;
				}
				else
				{
					frame.set.writeDiffuse(*pMesh->m_material.pDiffuse, diffuseID);
					pc.diffuseID = diffuseID++;
				}
				if (pMesh->m_material.pSpecular)
				{
					frame.set.writeSpecular(*pMesh->m_material.pSpecular, specularID);
					pc.specularID = specularID++;
				}
			}
			pushConstants.back().push_back(pc);
			++objectID;
			ASSERT(objectID == (u32)ssbos.models.ssbo.size(), "Index mismatch! Expect UB in shaders");
		}
	}
	rd::UBOView view(scene.view, (u32)scene.dirLights.size());
	std::copy(scene.dirLights.begin(), scene.dirLights.end(), std::back_inserter(ssbos.dirLights.ssbo));
	frame.set.writeSSBOs(ssbos);
	frame.set.writeView(view);
	// Acquire
	auto [acquire, bResult] = m_presenter.acquireNextImage(frame.renderReady, frame.drawing);
	if (!bResult)
	{
		return false;
	}
	// Framebuffer
	vkDestroy(frame.framebuffer);
	vk::FramebufferCreateInfo createInfo;
	createInfo.attachmentCount = (u32)acquire.attachments.size();
	createInfo.pAttachments = acquire.attachments.data();
	createInfo.renderPass = acquire.renderPass;
	createInfo.width = acquire.swapchainExtent.width;
	createInfo.height = acquire.swapchainExtent.height;
	createInfo.layers = 1;
	frame.framebuffer = g_info.device.createFramebuffer(createInfo);
	// RenderPass
	auto const c = scene.clear.colour;
	std::array const clearColour = {c.r.toF32(), c.g.toF32(), c.b.toF32(), c.a.toF32()};
	vk::ClearColorValue const colour = clearColour;
	vk::ClearDepthStencilValue const depth = {scene.clear.depthStencil.x, (u32)scene.clear.depthStencil.y};
	std::array<vk::ClearValue, 2> const clearValues = {colour, depth};
	vk::RenderPassBeginInfo renderPassInfo;
	renderPassInfo.renderPass = acquire.renderPass;
	renderPassInfo.framebuffer = frame.framebuffer;
	renderPassInfo.renderArea.extent = acquire.swapchainExtent;
	renderPassInfo.clearValueCount = (u32)clearValues.size();
	renderPassInfo.pClearValues = clearValues.data();
	// Begin
	auto const& commandBuffer = frame.commandBuffer;
	vk::CommandBufferBeginInfo beginInfo;
	commandBuffer.begin(beginInfo);
	commandBuffer.beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
	// Draw
	std::unordered_set<PipelineImpl*> pipelines;
	size_t batchIdx = 0;
	size_t drawableIdx = 0;
	for (auto& batch : scene.batches)
	{
		commandBuffer.setViewport(0, transformViewport(batch.viewport));
		commandBuffer.setScissor(0, transformScissor(batch.scissor));
		vk::Pipeline pipeline;
		for (auto [pMesh, pTransform, pPipeline] : batch.drawables)
		{
			if (pipeline != pPipeline->m_uImpl->m_pipeline)
			{
				pipeline = pPipeline->m_uImpl->m_pipeline;
				commandBuffer.bindPipeline(vk::PipelineBindPoint::eGraphics, pipeline);
			}
			auto layout = pPipeline->m_uImpl->m_layout;
			vk::DeviceSize offsets[] = {0};
			commandBuffer.bindDescriptorSets(vk::PipelineBindPoint::eGraphics, layout, 0, frame.set.m_descriptorSet, {});
			commandBuffer.pushConstants<rd::PushConstants>(layout, vkFlags::vertFragShader, 0, pushConstants.at(batchIdx).at(drawableIdx));
			commandBuffer.bindVertexBuffers(0, 1, &pMesh->m_uImpl->vbo.buffer, offsets);
			if (pMesh->m_uImpl->indexCount > 0)
			{
				commandBuffer.bindIndexBuffer(pMesh->m_uImpl->ibo.buffer, 0, vk::IndexType::eUint32);
				commandBuffer.drawIndexed(pMesh->m_uImpl->indexCount, 1, 0, 0, 0);
			}
			else
			{
				commandBuffer.draw(pMesh->m_uImpl->vertexCount, 1, 0, 0);
			}
			pipelines.insert(pPipeline->m_uImpl.get());
			++drawableIdx;
		}
		drawableIdx = 0;
		++batchIdx;
	}
	commandBuffer.endRenderPass();
	commandBuffer.end();
	// Submit
	vk::SubmitInfo submitInfo;
	vk::PipelineStageFlags const waitStages[] = {vk::PipelineStageFlagBits::eColorAttachmentOutput};
	submitInfo.waitSemaphoreCount = 1;
	submitInfo.pWaitSemaphores = &frame.renderReady;
	submitInfo.pWaitDstStageMask = waitStages;
	submitInfo.commandBufferCount = 1;
	submitInfo.pCommandBuffers = &commandBuffer;
	submitInfo.signalSemaphoreCount = 1;
	submitInfo.pSignalSemaphores = &frame.presentReady;
	g_info.device.resetFences(frame.drawing);
	for (auto pPipeline : pipelines)
	{
		pPipeline->attach(frame.drawing);
	}
	frame.set.attach(frame.drawing);
	g_info.queues.graphics.submit(submitInfo, frame.drawing);
	if (m_presenter.present(frame.presentReady))
	{
		next();
		return true;
	}
	return false;
}

vk::Viewport RendererImpl::transformViewport(ScreenRect const& nRect, glm::vec2 const& depth) const
{
	vk::Viewport viewport;
	auto const& extent = m_presenter.m_swapchain.extent;
	glm::vec2 const size = {nRect.right - nRect.left, nRect.bottom - nRect.top};
	viewport.minDepth = depth.x;
	viewport.maxDepth = depth.y;
	viewport.width = size.x * (f32)extent.width;
	viewport.height = -(size.y * (f32)extent.height);
	viewport.x = nRect.left * (f32)extent.width;
	viewport.y = nRect.top * (f32)extent.height - (f32)viewport.height;
	return viewport;
}

vk::Rect2D RendererImpl::transformScissor(ScreenRect const& nRect) const
{
	vk::Rect2D scissor;
	glm::vec2 const size = {nRect.right - nRect.left, nRect.bottom - nRect.top};
	scissor.offset.x = (s32)(nRect.left * (f32)m_presenter.m_swapchain.extent.width);
	scissor.offset.y = (s32)(nRect.top * (f32)m_presenter.m_swapchain.extent.height);
	scissor.extent.width = (u32)(size.x * (f32)m_presenter.m_swapchain.extent.width);
	scissor.extent.height = (u32)(size.y * (f32)m_presenter.m_swapchain.extent.height);
	return scissor;
}

void RendererImpl::onFramebufferResize()
{
	m_presenter.onFramebufferResize();
	return;
}

RendererImpl::FrameSync& RendererImpl::frameSync()
{
	ASSERT(m_index < m_frames.size(), "Invalid index!");
	return m_frames.at(m_index);
}

void RendererImpl::next()
{
	m_frames.at(m_index).bNascent = false;
	m_index = (m_index + 1) % m_frames.size();
	return;
}
} // namespace le::gfx