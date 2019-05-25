#include "LM.h"
#include "DeferredRenderer.h"
#include "Graphics/API/Shader.h"
#include "Graphics/RenderList.h"
#include "Graphics/API/Framebuffer.h"
#include "Graphics/Light.h"
#include "Graphics/API/Textures/Texture2D.h"
#include "Graphics/API/Textures/TextureDepth.h"
#include "Graphics/API/Textures/TextureCube.h"
#include "Graphics/ModelLoader/ModelLoader.h"
#include "Graphics/Mesh.h"
#include "Graphics/MeshFactory.h"
#include "Graphics/Material.h"
#include "Graphics/LightSetUp.h"
#include "Graphics/API/Renderer.h"
#include "Graphics/API/CommandBuffer.h"
#include "Graphics/API/Swapchain.h"
#include "Graphics/API/RenderPass.h"
#include "Graphics/API/Pipeline.h"
#include "Graphics/GBuffer.h"
#include "App/Scene.h"
#include "Entity/Entity.h"
#include "SkyboxRenderer.h"
#include "Maths/Maths.h"
#include "ShadowRenderer.h"
#include "App/Application.h"
#include "Graphics/RenderManager.h"
#include "Graphics/Camera/Camera.h"
#include "Graphics/Light.h"

#define MAX_LIGHTS 32
#define MAX_SHADOWMAPS 16

namespace lumos
{
	namespace graphics
	{
		enum VSSystemUniformIndices : int32
		{
			VSSystemUniformIndex_ProjectionMatrix = 0,
			VSSystemUniformIndex_ViewMatrix = 1,
			VSSystemUniformIndex_ModelMatrix = 2,
			VSSystemUniformIndex_TextureMatrix = 3,
			VSSystemUniformIndex_Size
		};

		enum PSSystemUniformIndices : int32
		{
			PSSystemUniformIndex_Lights = 0,
			PSSystemUniformIndex_Size
		};

		struct UniformBufferLight
		{
			graphics::Light light[MAX_LIGHTS];
			lumos::maths::Vector4 cameraPosition;
			lumos::maths::Matrix4 viewMatrix;
			lumos::maths::Matrix4 uShadowTransform[MAX_SHADOWMAPS];
			lumos::maths::Vector4 uSplitDepth[MAX_SHADOWMAPS];
			float lightCount;
			float shadowCount;
			float mode;
			float p1;
		};

		struct UniformBufferObject
		{
			lumos::maths::Matrix4 projView;
		};

		DeferredRenderer::DeferredRenderer(uint width, uint height)
		{
			DeferredRenderer::SetScreenBufferSize(width, height);
			DeferredRenderer::Init();
		}

		DeferredRenderer::~DeferredRenderer()
		{
			delete m_DeferredShader;
			delete m_OffScreenShader;
			delete m_FBO;
			delete m_DefaultTexture;
			delete m_UniformBuffer;

			AlignedFree(uboDataDynamic.model);

			delete m_ModelUniformBuffer;
			delete m_LightUniformBuffer;
			delete m_DeferredRenderpass;
			delete m_DeferredPipeline;
			delete m_OffScreenRenderpass;
			delete m_OffScreenPipeline;
			delete m_ScreenQuad;
			delete m_DefaultDescriptorSet;
			delete m_DeferredDescriptorSet;
			delete m_DefaultMaterialDataUniformBuffer;
			delete m_LightSetup;

			delete[] m_VSSystemUniformBuffer;
			delete[] m_PSSystemUniformBuffer;
			for (auto& commandBuffer : m_CommandBuffers)
			{
				delete commandBuffer;
			}

			for (auto fbo : m_Framebuffers)
				delete fbo;

			m_CommandBuffers.clear();

			delete m_DeferredCommandBuffers;
		}

		void DeferredRenderer::Init()
		{
			m_OffScreenShader = Shader::CreateFromFile("DeferredColour", "/CoreShaders/");
			m_DeferredShader = Shader::CreateFromFile("DeferredLight", "/CoreShaders/");

			m_DefaultTexture = Texture2D::CreateFromFile("Test", "/CoreTextures/checkerboard.tga");

			m_DeferredDescriptorSet = nullptr;
			m_DefaultDescriptorSet = nullptr;

			TextureParameters param;
			param.filter = TextureFilter::LINEAR;
			param.format = TextureFormat::RGBA;
			param.wrap = TextureWrap::CLAMP_TO_EDGE;
			m_PreintegratedFG = std::unique_ptr<Texture2D>(Texture2D::CreateFromFile("PreintegratedFG", "/CoreTextures/PreintegratedFG.png", param));

			m_LightUniformBuffer = nullptr;
			m_UniformBuffer = nullptr;
			m_ModelUniformBuffer = nullptr;

			m_DefaultMaterialDataUniformBuffer = graphics::UniformBuffer::Create();

			MaterialProperties properties;
			properties.glossColour = 1.0f;
			properties.specularColour = maths::Vector4(0.0f, 1.0f, 0.0f, 1.0f);
			properties.usingAlbedoMap = 1.0f;
			properties.usingGlossMap = 0.0f;
			properties.usingNormalMap = 0.0f;
			properties.usingSpecularMap = 0.0f;

			uint32_t bufferSize = static_cast<uint32_t>(sizeof(MaterialProperties));
			m_DefaultMaterialDataUniformBuffer->Init(bufferSize, nullptr);
			m_DefaultMaterialDataUniformBuffer->SetData(bufferSize, &properties);

			m_ScreenQuad = graphics::CreateQuad();

			m_CommandQueue.reserve(1000);
			m_LightSetup = new LightSetup();

			//
			// Vertex shader system uniforms
			//
			m_VSSystemUniformBufferSize = sizeof(maths::Matrix4);
			m_VSSystemUniformBuffer = new byte[m_VSSystemUniformBufferSize];
			memset(m_VSSystemUniformBuffer, 0, m_VSSystemUniformBufferSize);
			m_VSSystemUniformBufferOffsets.resize(VSSystemUniformIndex_Size);

			// Per Scene System Uniforms
			m_VSSystemUniformBufferOffsets[VSSystemUniformIndex_ProjectionMatrix] = 0;

			// Pixel/fragment shader system uniforms
			m_PSSystemUniformBufferSize = sizeof(UniformBufferLight);
			m_PSSystemUniformBuffer = new byte[m_PSSystemUniformBufferSize];
			memset(m_PSSystemUniformBuffer, 0, m_PSSystemUniformBufferSize);
			m_PSSystemUniformBufferOffsets.resize(PSSystemUniformIndex_Size);

			// Per Scene System Uniforms
			m_PSSystemUniformBufferOffsets[PSSystemUniformIndex_Lights] = 0;

			m_OffScreenRenderpass = graphics::RenderPass::Create();
			m_DeferredRenderpass = graphics::RenderPass::Create();

			TextureType textureTypes[2] = { TextureType::COLOUR, TextureType::DEPTH };
			graphics::RenderpassInfo renderpassCI{};
			renderpassCI.attachmentCount = 2;
			renderpassCI.textureType = textureTypes;

			m_DeferredRenderpass->Init(renderpassCI);

			TextureType textureTypesOffScreen[5] = { TextureType::COLOUR , TextureType::COLOUR ,TextureType::COLOUR ,TextureType::COLOUR ,TextureType::DEPTH };
			graphics::RenderpassInfo renderpassCIOffScreen{};
			renderpassCIOffScreen.attachmentCount = 5;
			renderpassCIOffScreen.textureType = textureTypesOffScreen;

			m_OffScreenRenderpass->Init(renderpassCIOffScreen);

			m_CommandBuffers.resize(Renderer::GetSwapchain()->GetSwapchainBufferCount());

			for (auto& commandBuffer : m_CommandBuffers)
			{
				commandBuffer = graphics::CommandBuffer::Create();
				commandBuffer->Init(true);
			}

			m_DeferredCommandBuffers = graphics::CommandBuffer::Create();
			m_DeferredCommandBuffers->Init(true);

			CreateOffScreenPipeline();
			CreateDeferredPipeline();
			CreateOffScreenBuffer();
			CreateOffScreenFBO();
			CreateFramebuffers();
			CreateLightBuffer();
			CreateDefaultDescriptorSet();

			graphics::DescriptorInfo info{};
			info.pipeline = m_DeferredPipeline;
			info.layoutIndex = 1; //?
			info.shader = m_DeferredShader;
			m_DeferredDescriptorSet = graphics::DescriptorSet::Create(info);

			m_ClearColour = maths::Vector4(0.1f, 0.1f, 0.1f, 1.0f);

			CreateScreenDescriptorSet();
		}

		void DeferredRenderer::RenderScene(RenderList* renderList, Scene* scene)
		{
			SubmitLightSetup(scene);

			BeginOffscreen();

			renderList->RenderOpaqueObjects([&](Entity* obj)
			{
				if (obj != nullptr)
				{
					auto* model = obj->GetComponent<MeshComponent>();
					if (model && model->m_Model)
					{
						auto mesh = model->m_Model;
						if (mesh->GetMaterial())
						{
							if (mesh->GetMaterial()->GetDescriptorSet() == nullptr || mesh->GetMaterial()->GetPipeline() != m_OffScreenPipeline)
								mesh->GetMaterial()->CreateDescriptorSet(m_OffScreenPipeline, 1);
						}

						TextureMatrixComponent* textureMatrixTransform = obj->GetComponent<TextureMatrixComponent>();
						maths::Matrix4 textureMatrix;
						if (textureMatrixTransform)
							textureMatrix = textureMatrixTransform->m_TextureMatrix;
						else
							textureMatrix = maths::Matrix4();

						auto transform = obj->GetComponent<TransformComponent>()->m_Transform.GetWorldMatrix();

#if 0
						bool inside = true;

						float maxScaling = 0.0f;
						maxScaling = maths::Max(transform.GetScaling().GetX(), maxScaling);
						maxScaling = maths::Max(transform.GetScaling().GetY(), maxScaling);
						maxScaling = maths::Max(transform.GetScaling().GetZ(), maxScaling);

						inside = GraphicsPipeline::Instance()->GetFrustum().InsideFrustum(transform * mesh->GetBoundingSphere()->Centre(), maxScaling * mesh->GetBoundingSphere()->SphereRadius());

						if (inside)
#endif
							SubmitMesh(mesh.get(), transform, textureMatrix);
					}
				}
			});

			SetSystemUniforms(m_OffScreenShader);

			PresentOffScreen();

			EndOffScreen();

			LightPass();
		}

		void DeferredRenderer::LightPass()
		{
			int commandBufferIndex = 0;
			if (!m_RenderTexture)
				commandBufferIndex = Renderer::GetSwapchain()->GetCurrentBufferId();

			Begin(commandBufferIndex);
			Present();
			End();

			if (!m_RenderTexture)
				PresentToScreen();
		}

		void DeferredRenderer::PresentToScreen()
		{
			Renderer::Present((m_CommandBuffers[Renderer::GetSwapchain()->GetCurrentBufferId()]));
		}

		void DeferredRenderer::Begin(int commandBufferID)
		{
			m_CommandQueue.clear();
			m_SystemUniforms.clear();

			m_CommandBufferIndex = commandBufferID;

			m_CommandBuffers[m_CommandBufferIndex]->BeginRecording();

			m_DeferredRenderpass->BeginRenderpass(m_CommandBuffers[m_CommandBufferIndex], m_ClearColour, m_Framebuffers[m_CommandBufferIndex], graphics::SECONDARY, m_ScreenBufferWidth, m_ScreenBufferHeight);
		}

		void DeferredRenderer::BeginOffscreen()
		{
			m_CommandQueue.clear();
			m_SystemUniforms.clear();

			m_DeferredCommandBuffers->BeginRecording();

			m_OffScreenRenderpass->BeginRenderpass(m_DeferredCommandBuffers, maths::Vector4(0.0f), m_FBO, graphics::SECONDARY, m_ScreenBufferWidth, m_ScreenBufferHeight);
		}

		void DeferredRenderer::BeginScene(Scene* scene)
		{
			auto camera = scene->GetCamera();
			auto proj = camera->GetProjectionMatrix();

			auto projView = proj * camera->GetViewMatrix();
			memcpy(m_VSSystemUniformBuffer + m_VSSystemUniformBufferOffsets[VSSystemUniformIndex_ProjectionMatrix], &projView, sizeof(maths::Matrix4));


			if (Application::Instance()->GetRenderManager()->GetSkyBox())
			{
				if (Application::Instance()->GetRenderManager()->GetSkyBox() != m_CubeMap)
				{
					SetCubeMap(Application::Instance()->GetRenderManager()->GetSkyBox());
					CreateScreenDescriptorSet();
				}
			}
			else
			{
				if (m_CubeMap)
				{
					m_CubeMap = nullptr;
					CreateScreenDescriptorSet();
				}
			}
		}

		void DeferredRenderer::Submit(const RenderCommand& command)
		{
			m_CommandQueue.push_back(command);
		}

		void DeferredRenderer::SubmitMesh(Mesh* mesh, const maths::Matrix4& transform, const maths::Matrix4& textureMatrix)
		{
			RenderCommand command;
			command.mesh = mesh;
			command.transform = transform;
			command.textureMatrix = textureMatrix;
			Submit(command);
		}

		void DeferredRenderer::SubmitLightSetup(Scene* scene)
		{
			auto lightList = scene->GetLightList();

			if (lightList.empty())
				return;
			uint32_t currentOffset = 0;

			for (int i = 0; i < lightList.size(); i++)
			{
				lightList[i]->m_Direction.Normalise();
				memcpy(m_PSSystemUniformBuffer + currentOffset + sizeof(graphics::Light) * i, &*lightList[i], sizeof(graphics::Light));
			}

			currentOffset += sizeof(graphics::Light) * MAX_LIGHTS;

			maths::Vector4 cameraPos = maths::Vector4(scene->GetCamera()->GetPosition());
			memcpy(m_PSSystemUniformBuffer + currentOffset, &cameraPos, sizeof(maths::Vector4));
			currentOffset += sizeof(maths::Vector4);

			auto shadowRenderer = Application::Instance()->GetRenderManager()->GetShadowRenderer();
			if (shadowRenderer)
			{
				maths::Matrix4* shadowTransforms = shadowRenderer->GetShadowProjView();
				auto viewMat = scene->GetCamera()->GetViewMatrix();
				lumos::maths::Vector4* uSplitDepth = shadowRenderer->GetSplitDepths();

				memcpy(m_PSSystemUniformBuffer + currentOffset, &viewMat, sizeof(maths::Matrix4));
				currentOffset += sizeof(maths::Matrix4);
				memcpy(m_PSSystemUniformBuffer + currentOffset, shadowTransforms, sizeof(maths::Matrix4) * MAX_SHADOWMAPS);
				currentOffset += sizeof(maths::Matrix4) * MAX_SHADOWMAPS;
				memcpy(m_PSSystemUniformBuffer + currentOffset, uSplitDepth, sizeof(lumos::maths::Vector4) * MAX_SHADOWMAPS);
				currentOffset += sizeof(maths::Vector4) * MAX_SHADOWMAPS;
			}
			else
			{
				currentOffset += sizeof(maths::Matrix4);
				currentOffset += sizeof(maths::Matrix4) * MAX_SHADOWMAPS;
				currentOffset += sizeof(maths::Vector4) * MAX_SHADOWMAPS;
			}

			float numLights = float(lightList.size());
			memcpy(m_PSSystemUniformBuffer + currentOffset, &numLights, sizeof(float));
			currentOffset += sizeof(float);

			float numShadows = shadowRenderer ? shadowRenderer->GetShadowMapNum() : 0.0f;
			memcpy(m_PSSystemUniformBuffer + currentOffset, &numShadows, sizeof(float));
			currentOffset += sizeof(float);

			float renderMode = 0.0f;
			memcpy(m_PSSystemUniformBuffer + currentOffset, &renderMode, sizeof(float));
			currentOffset += sizeof(float);
		}

		void DeferredRenderer::EndScene()
		{
		}

		void DeferredRenderer::End()
		{
			m_DeferredRenderpass->EndRenderpass(m_CommandBuffers[m_CommandBufferIndex]);
			m_CommandBuffers[m_CommandBufferIndex]->EndRecording();

			if (m_RenderTexture)
				m_CommandBuffers[0]->Execute(true);
		}

		void DeferredRenderer::EndOffScreen()
		{
			m_OffScreenRenderpass->EndRenderpass(m_DeferredCommandBuffers);
			m_DeferredCommandBuffers->EndRecording();
			m_DeferredCommandBuffers->Execute(true);
		}

		void DeferredRenderer::SetSystemUniforms(Shader* shader) const
		{
			m_UniformBuffer->SetData(sizeof(UniformBufferObject), *&m_VSSystemUniformBuffer);

			int index = 0;

			for (auto& command : m_CommandQueue)
			{
				maths::Matrix4* modelMat = reinterpret_cast<maths::Matrix4*>((reinterpret_cast<uint64_t>(uboDataDynamic.model) + (index * dynamicAlignment)));
				*modelMat = command.transform;
				index++;
			}
			m_ModelUniformBuffer->SetDynamicData(static_cast<uint32_t>(MAX_OBJECTS * dynamicAlignment), sizeof(maths::Matrix4), &*uboDataDynamic.model);

			m_LightUniformBuffer->SetData(sizeof(UniformBufferLight), *&m_PSSystemUniformBuffer);
		}

		void DeferredRenderer::Present()
		{
			graphics::CommandBuffer* currentCMDBuffer = (m_ScreenQuad->GetCommandBuffer(static_cast<int>(m_CommandBufferIndex)));

			currentCMDBuffer->BeginRecordingSecondary(m_DeferredRenderpass, m_Framebuffers[m_CommandBufferIndex]);
			currentCMDBuffer->UpdateViewport(m_ScreenBufferWidth, m_ScreenBufferHeight);

			m_DeferredPipeline->SetActive(currentCMDBuffer);

			Renderer::RenderMesh(m_ScreenQuad, m_DeferredPipeline, currentCMDBuffer, 0, m_DeferredDescriptorSet);

			currentCMDBuffer->EndRecording();
			currentCMDBuffer->ExecuteSecondary(m_CommandBuffers[m_CommandBufferIndex]);
		}

		void DeferredRenderer::PresentOffScreen()
		{
			int index = 0;

			for (auto& command : m_CommandQueue)
			{
				Mesh* mesh = command.mesh;

				graphics::CommandBuffer* currentCMDBuffer = mesh->GetCommandBuffer(0);

				currentCMDBuffer->BeginRecordingSecondary(m_OffScreenRenderpass, m_FBO);
				currentCMDBuffer->UpdateViewport(m_ScreenBufferWidth, m_ScreenBufferHeight);

				m_OffScreenPipeline->SetActive(currentCMDBuffer);

				uint32_t dynamicOffset = index * static_cast<uint32_t>(dynamicAlignment);

				Renderer::RenderMesh(mesh, m_OffScreenPipeline, currentCMDBuffer, dynamicOffset, m_DefaultDescriptorSet);

				currentCMDBuffer->EndRecording();
				currentCMDBuffer->ExecuteSecondary(m_DeferredCommandBuffers);

				index++;
			}
		}

		void DeferredRenderer::CreateOffScreenPipeline()
		{
			std::vector<graphics::DescriptorPoolInfo> poolInfo =
			{
				{ graphics::DescriptorType::UNIFORM_BUFFER, MAX_OBJECTS },
				{ graphics::DescriptorType::UNIFORM_BUFFER, MAX_OBJECTS },
				{ graphics::DescriptorType::UNIFORM_BUFFER_DYNAMIC, MAX_OBJECTS },
				{ graphics::DescriptorType::IMAGE_SAMPLER, MAX_OBJECTS },
				{ graphics::DescriptorType::IMAGE_SAMPLER, MAX_OBJECTS },
				{ graphics::DescriptorType::IMAGE_SAMPLER, MAX_OBJECTS },
				{ graphics::DescriptorType::IMAGE_SAMPLER, MAX_OBJECTS },
				{ graphics::DescriptorType::IMAGE_SAMPLER, MAX_OBJECTS }
			};

			std::vector<graphics::DescriptorLayoutInfo> layoutInfo =
			{
				{ graphics::DescriptorType::UNIFORM_BUFFER, graphics::ShaderStage::VERTEX, 0 },
				{ graphics::DescriptorType::UNIFORM_BUFFER_DYNAMIC,graphics::ShaderStage::VERTEX , 1 },
			};

			std::vector<graphics::DescriptorLayoutInfo> layoutInfoMesh =
			{
				{ graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 0 },
				{ graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 1 },
				{ graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 2 },
				{ graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 3 },
				{ graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 4 },
				{ graphics::DescriptorType::UNIFORM_BUFFER, graphics::ShaderStage::FRAGMENT, 5 },
			};

			auto attributeDescriptions = Vertex::getAttributeDescriptions();

			std::vector<graphics::DescriptorLayout> descriptorLayouts;

			graphics::DescriptorLayout sceneDescriptorLayout{};
			sceneDescriptorLayout.count = static_cast<uint>(layoutInfo.size());
			sceneDescriptorLayout.layoutInfo = layoutInfo.data();

			descriptorLayouts.push_back(sceneDescriptorLayout);

			graphics::DescriptorLayout meshDescriptorLayout{};
			meshDescriptorLayout.count = static_cast<uint>(layoutInfoMesh.size());
			meshDescriptorLayout.layoutInfo = layoutInfoMesh.data();

			descriptorLayouts.push_back(meshDescriptorLayout);

			graphics::PipelineInfo pipelineCI{};
			pipelineCI.pipelineName = "OffScreenRenderer";
			pipelineCI.shader = m_OffScreenShader;
			pipelineCI.vulkanRenderpass = m_OffScreenRenderpass;
			pipelineCI.numVertexLayout = static_cast<uint>(attributeDescriptions.size());
			pipelineCI.descriptorLayouts = descriptorLayouts;
			pipelineCI.vertexLayout = attributeDescriptions.data();
			pipelineCI.numLayoutBindings = static_cast<uint>(poolInfo.size());
			pipelineCI.typeCounts = poolInfo.data();
			pipelineCI.strideSize = sizeof(Vertex);
			pipelineCI.numColorAttachments = 5;
			pipelineCI.wireframeEnabled = false;
			pipelineCI.cullMode = graphics::CullMode::BACK;
			pipelineCI.transparencyEnabled = false;
			pipelineCI.depthBiasEnabled = false;
			pipelineCI.width = m_ScreenBufferWidth;
			pipelineCI.height = m_ScreenBufferHeight;
			pipelineCI.maxObjects = MAX_OBJECTS;

			m_OffScreenPipeline = graphics::Pipeline::Create(pipelineCI);
		}

		void DeferredRenderer::CreateDeferredPipeline()
		{
			std::vector<graphics::DescriptorPoolInfo> poolInfo =
			{
				{ graphics::DescriptorType::IMAGE_SAMPLER , 1 },
				{ graphics::DescriptorType::IMAGE_SAMPLER , 1 },
				{ graphics::DescriptorType::IMAGE_SAMPLER , 1 },
				{ graphics::DescriptorType::IMAGE_SAMPLER , 1 },
				{ graphics::DescriptorType::IMAGE_SAMPLER , 1 },
				{ graphics::DescriptorType::IMAGE_SAMPLER , 1 },
				{ graphics::DescriptorType::IMAGE_SAMPLER , 1 },
				{ graphics::DescriptorType::IMAGE_SAMPLER , 1 },
				{ graphics::DescriptorType::UNIFORM_BUFFER, 1 }
			};

			std::vector<graphics::DescriptorLayoutInfo> layoutInfo =
			{
				{ graphics::DescriptorType::UNIFORM_BUFFER, graphics::ShaderStage::FRAGMENT, 0 },

			};

			std::vector<graphics::DescriptorLayoutInfo> layoutInfoMesh =
			{
				 { graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 0 },
				 { graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 1 },
				 { graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 2 },
				 { graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 3 },
				 { graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 4 },
				 { graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 5 },
				 { graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 6 },
				 { graphics::DescriptorType::IMAGE_SAMPLER,graphics::ShaderStage::FRAGMENT , 7 }
			};

			auto attributeDescriptions = Vertex::getAttributeDescriptions();

			std::vector<graphics::DescriptorLayout> descriptorLayouts;

			graphics::DescriptorLayout sceneDescriptorLayout{};
			sceneDescriptorLayout.count = static_cast<uint>(layoutInfo.size());
			sceneDescriptorLayout.layoutInfo = layoutInfo.data();

			descriptorLayouts.push_back(sceneDescriptorLayout);

			graphics::DescriptorLayout meshDescriptorLayout{};
			meshDescriptorLayout.count = static_cast<uint>(layoutInfoMesh.size());
			meshDescriptorLayout.layoutInfo = layoutInfoMesh.data();

			descriptorLayouts.push_back(meshDescriptorLayout);

			graphics::PipelineInfo pipelineCI{};
			pipelineCI.pipelineName = "Deferred";
			pipelineCI.shader = m_DeferredShader;
			pipelineCI.vulkanRenderpass = m_DeferredRenderpass;
			pipelineCI.numVertexLayout = static_cast<uint>(attributeDescriptions.size());
			pipelineCI.descriptorLayouts = descriptorLayouts;
			pipelineCI.vertexLayout = attributeDescriptions.data();
			pipelineCI.numLayoutBindings = static_cast<uint>(poolInfo.size());
			pipelineCI.typeCounts = poolInfo.data();
			pipelineCI.strideSize = sizeof(Vertex);
			pipelineCI.numColorAttachments = 1;
			pipelineCI.wireframeEnabled = false;
			pipelineCI.cullMode = graphics::CullMode::NONE;
			pipelineCI.transparencyEnabled = false;
			pipelineCI.depthBiasEnabled = false;
			pipelineCI.width = m_ScreenBufferWidth;
			pipelineCI.height = m_ScreenBufferHeight;
			pipelineCI.maxObjects = 10;

			m_DeferredPipeline = graphics::Pipeline::Create(pipelineCI);
		}

		void DeferredRenderer::CreateOffScreenBuffer()
		{
			if (m_UniformBuffer == nullptr)
			{
				m_UniformBuffer = graphics::UniformBuffer::Create();

				uint32_t bufferSize = static_cast<uint32_t>(sizeof(UniformBufferObject));
				m_UniformBuffer->Init(bufferSize, nullptr);
			}

			if (m_ModelUniformBuffer == nullptr)
			{
				m_ModelUniformBuffer = graphics::UniformBuffer::Create();
				const size_t minUboAlignment = graphics::GraphicsContext::GetContext()->GetMinUniformBufferOffsetAlignment();

				dynamicAlignment = sizeof(lumos::maths::Matrix4);
				if (minUboAlignment > 0)
				{
					dynamicAlignment = (dynamicAlignment + minUboAlignment - 1) & ~(minUboAlignment - 1);
				}

				uint32_t bufferSize2 = static_cast<uint32_t>(MAX_OBJECTS * dynamicAlignment);

				uboDataDynamic.model = static_cast<maths::Matrix4*>(AlignedAlloc(bufferSize2, dynamicAlignment));

				m_ModelUniformBuffer->Init(bufferSize2, nullptr);
			}

			std::vector<graphics::BufferInfo> bufferInfos;

			graphics::BufferInfo bufferInfo = {};
			bufferInfo.buffer = m_UniformBuffer;
			bufferInfo.offset = 0;
			bufferInfo.size = sizeof(UniformBufferObject);
			bufferInfo.type = graphics::DescriptorType::UNIFORM_BUFFER;
			bufferInfo.binding = 0;
			bufferInfo.shaderType = ShaderType::VERTEX;
			bufferInfo.systemUniforms = true;

			graphics::BufferInfo bufferInfo2 = {};
			bufferInfo2.buffer = m_ModelUniformBuffer;
			bufferInfo2.offset = 0;
			bufferInfo2.size = sizeof(UniformBufferModel);
			bufferInfo2.type = graphics::DescriptorType::UNIFORM_BUFFER_DYNAMIC;
			bufferInfo2.binding = 1;
			bufferInfo2.shaderType = ShaderType::VERTEX;
			bufferInfo2.systemUniforms = false;

			bufferInfos.push_back(bufferInfo);
			bufferInfos.push_back(bufferInfo2);

			m_OffScreenPipeline->GetDescriptorSet()->Update(bufferInfos);
		}

		void DeferredRenderer::CreateOffScreenFBO()
		{
			const uint attachmentCount = 5;
			TextureType attachmentTypes[attachmentCount];
			attachmentTypes[0] = TextureType::COLOUR;
			attachmentTypes[1] = TextureType::COLOUR;
			attachmentTypes[2] = TextureType::COLOUR;
			attachmentTypes[3] = TextureType::COLOUR;
			attachmentTypes[4] = TextureType::DEPTH;

			FramebufferInfo bufferInfo{};
			bufferInfo.width = m_ScreenBufferWidth;
			bufferInfo.height = m_ScreenBufferHeight;
			bufferInfo.attachmentCount = attachmentCount;
			bufferInfo.renderPass = m_OffScreenRenderpass;
			bufferInfo.attachmentTypes = attachmentTypes;

			Texture* attachments[attachmentCount];
			attachments[0] = Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_COLOUR];
			attachments[1] = Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_POSITION];
			attachments[2] = Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_NORMALS];
			attachments[3] = Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_PBR];
			attachments[4] = Application::Instance()->GetRenderManager()->GetGBuffer()->m_DepthTexture;
			bufferInfo.attachments = attachments;

			m_FBO = Framebuffer::Create(bufferInfo);
		}

		void DeferredRenderer::SetRenderTarget(Texture* texture)
		{
			m_RenderTexture = texture;

			for (auto fbo : m_Framebuffers)
				delete fbo;
			m_Framebuffers.clear();

			CreateFramebuffers();
		}

		void DeferredRenderer::SetRenderToGBufferTexture(bool set)
		{
			m_RenderToGBufferTexture = true;
			m_RenderTexture = Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_OFFSCREEN0];

			for (auto fbo : m_Framebuffers)
				delete fbo;
			m_Framebuffers.clear();

			CreateFramebuffers();
		}

		void DeferredRenderer::CreateFramebuffers()
		{
			TextureType attachmentTypes[2];
			attachmentTypes[0] = TextureType::COLOUR;
			attachmentTypes[1] = TextureType::DEPTH;

			Texture* attachments[2];
			attachments[1] = reinterpret_cast<Texture*>(Application::Instance()->GetRenderManager()->GetGBuffer()->m_DepthTexture);
			FramebufferInfo bufferInfo{};
			bufferInfo.width = m_ScreenBufferWidth;
			bufferInfo.height = m_ScreenBufferHeight;
			bufferInfo.attachmentCount = 2;
			bufferInfo.renderPass = m_DeferredRenderpass;
			bufferInfo.attachmentTypes = attachmentTypes;

			if (m_RenderTexture)
			{
				attachments[0] = m_RenderTexture;
				bufferInfo.attachments = attachments;
				bufferInfo.screenFBO = false;
				m_Framebuffers.emplace_back(Framebuffer::Create(bufferInfo));
			}
			else
			{
				for (uint32_t i = 0; i < Renderer::GetSwapchain()->GetSwapchainBufferCount(); i++)
				{
					bufferInfo.screenFBO = true;
					attachments[0] = Renderer::GetSwapchain()->GetImage(i);
					bufferInfo.attachments = attachments;

					m_Framebuffers.emplace_back(Framebuffer::Create(bufferInfo));
				}
			}
		}

		void DeferredRenderer::CreateLightBuffer()
		{
			if (m_LightUniformBuffer == nullptr)
			{
				m_LightUniformBuffer = graphics::UniformBuffer::Create();

				uint32_t bufferSize = static_cast<uint32_t>(sizeof(UniformBufferLight));
				m_LightUniformBuffer->Init(bufferSize, nullptr);
			}

			std::vector<graphics::BufferInfo> bufferInfos;

			graphics::BufferInfo bufferInfo = {};
			bufferInfo.name = "LightData";
			bufferInfo.buffer = m_LightUniformBuffer;
			bufferInfo.offset = 0;
			bufferInfo.size = sizeof(UniformBufferLight);
			bufferInfo.type = graphics::DescriptorType::UNIFORM_BUFFER;
			bufferInfo.binding = 0;
			bufferInfo.shaderType = ShaderType::FRAGMENT;
			bufferInfo.systemUniforms = false;

			bufferInfos.push_back(bufferInfo);

			m_DeferredPipeline->GetDescriptorSet()->Update(bufferInfos);
		}

		void DeferredRenderer::OnResize(uint width, uint height)
		{
			delete m_DeferredPipeline;
			delete m_OffScreenPipeline;
			delete m_FBO;

			for (auto fbo : m_Framebuffers)
				delete fbo;
			m_Framebuffers.clear();

			if (m_RenderToGBufferTexture)
				m_RenderTexture = Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_OFFSCREEN0];

			DeferredRenderer::SetScreenBufferSize(width, height);

			CreateOffScreenPipeline();
			CreateDeferredPipeline();
			CreateOffScreenBuffer();
			CreateOffScreenFBO();
			CreateFramebuffers();
			CreateLightBuffer();
			CreateDefaultDescriptorSet();

			graphics::DescriptorInfo info{};
			info.pipeline = m_DeferredPipeline;
			info.layoutIndex = 1; //?
			info.shader = m_DeferredShader;
			if (m_DeferredDescriptorSet)
				delete m_DeferredDescriptorSet;
			m_DeferredDescriptorSet = graphics::DescriptorSet::Create(info);

			m_CubeMap = nullptr;

			m_ClearColour = maths::Vector4(0.8f, 0.8f, 0.8f, 1.0f);
		}

		void DeferredRenderer::SetCubeMap(Texture* cubeMap)
		{
			m_CubeMap = cubeMap;
		}

		void DeferredRenderer::CreateDefaultDescriptorSet()
		{
			if (m_DefaultDescriptorSet)
				delete m_DefaultDescriptorSet;

			graphics::DescriptorInfo info{};
			info.pipeline = m_OffScreenPipeline;
			info.layoutIndex = 1;
			info.shader = m_OffScreenShader;
			m_DefaultDescriptorSet = graphics::DescriptorSet::Create(info);

			std::vector<graphics::ImageInfo> imageInfos;

			graphics::ImageInfo imageInfo = {};
			imageInfo.texture = { m_DefaultTexture };
			imageInfo.binding = 0;
			imageInfo.name = "u_AlbedoMap";

			imageInfos.push_back(imageInfo);

			std::vector<graphics::BufferInfo> bufferInfos;

			graphics::BufferInfo bufferInfo = {};
			bufferInfo.buffer = m_DefaultMaterialDataUniformBuffer;
			bufferInfo.offset = 0;
			bufferInfo.size = sizeof(MaterialProperties);
			bufferInfo.type = graphics::DescriptorType::UNIFORM_BUFFER;
			bufferInfo.binding = 5;
			bufferInfo.shaderType = ShaderType::FRAGMENT;
			bufferInfo.name = "UniformMaterialData";
			bufferInfo.systemUniforms = false;

			bufferInfos.push_back(bufferInfo);

			m_DefaultDescriptorSet->Update(imageInfos, bufferInfos);
		}

		void DeferredRenderer::CreateScreenDescriptorSet()
		{
			std::vector<graphics::ImageInfo> bufferInfos;

			graphics::ImageInfo imageInfo = {};
			imageInfo.texture = { Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_COLOUR] };
			imageInfo.binding = 0;
			imageInfo.name = "uColourSampler";

			graphics::ImageInfo imageInfo2 = {};
			imageInfo2.texture = { Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_POSITION] };
			imageInfo2.binding = 1;
			imageInfo2.name = "uPositionSampler";

			graphics::ImageInfo imageInfo3 = {};
			imageInfo3.texture = { Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_NORMALS] };
			imageInfo3.binding = 2;
			imageInfo3.name = "uNormalSampler";

			graphics::ImageInfo imageInfo4 = {};
			imageInfo4.texture = { Application::Instance()->GetRenderManager()->GetGBuffer()->m_ScreenTex[SCREENTEX_PBR] };
			imageInfo4.binding = 3;
			imageInfo4.name = "uPBRSampler";

			graphics::ImageInfo imageInfo5 = {};
			imageInfo5.texture = { m_PreintegratedFG.get() };
			imageInfo5.binding = 4;
			imageInfo5.name = "uPreintegratedFG";

			graphics::ImageInfo imageInfo6 = {};
			imageInfo6.texture = { m_CubeMap };
			imageInfo6.binding = 5;
			imageInfo6.type = TextureType::CUBE;
			imageInfo6.name = "uEnvironmentMap";

			graphics::ImageInfo imageInfo7 = {};
			auto shadowRenderer = Application::Instance()->GetRenderManager()->GetShadowRenderer();
			if (shadowRenderer)
			{
				imageInfo7.texture = { reinterpret_cast<Texture*>(shadowRenderer->GetTexture()) };
				imageInfo7.binding = 6;
				imageInfo7.type = TextureType::DEPTHARRAY;
				imageInfo7.name = "uShadowMap";
			}

			graphics::ImageInfo imageInfo8 = {};
			imageInfo8.texture = { Application::Instance()->GetRenderManager()->GetGBuffer()->m_DepthTexture };
			imageInfo8.binding = 7;
			imageInfo8.type = TextureType::DEPTH;
			imageInfo8.name = "uDepthSampler";

			bufferInfos.push_back(imageInfo);
			bufferInfos.push_back(imageInfo2);
			bufferInfos.push_back(imageInfo3);
			bufferInfos.push_back(imageInfo4);
			bufferInfos.push_back(imageInfo5);
			if (m_CubeMap)
				bufferInfos.push_back(imageInfo6);
			if (shadowRenderer)
				bufferInfos.push_back(imageInfo7);

			m_DeferredDescriptorSet->Update(bufferInfos);
		}
	}
}
