#pragma once
#include "VK.h"
#include "Graphics/API/Pipeline.h"
#include "VKDescriptorSet.h"

namespace lumos
{
	class Shader;

	namespace graphics
	{
		class VKCommandBuffer;

		class VKPipeline : public Pipeline
		{
		public:
			VKPipeline();
			VKPipeline(const PipelineInfo& pipelineCI);
			~VKPipeline();

			bool Init(const PipelineInfo& pipelineCI);

			void Unload() const;
			void SetActive(graphics::CommandBuffer* cmdBuffer) override;

			vk::DescriptorSetLayout* GetDescriptorLayout(int id) { return &m_DescriptorLayouts[id]; };
			
			vk::DescriptorPool  GetDescriptorPool() const { return m_DescriptorPool; };
			vk::PipelineLayout  GetPipelineLayout() const { return m_PipelineLayout; };
			std::string 		GetPipelineName() 	const { return m_PipelineName; };
			vk::Pipeline 		GetPipeline() 		const { return m_Pipeline; }

			vk::DescriptorSet CreateDescriptorSet();

		private:
			vk::VertexInputBindingDescription 	m_VertexBindingDescription;
			vk::PipelineLayout 					m_PipelineLayout;
			vk::DescriptorPool 					m_DescriptorPool;
			vk::Pipeline 							m_Pipeline;

			std::vector<vk::DescriptorSetLayout> 	m_DescriptorLayouts;

			std::string 						m_PipelineName;
		};
	}
}


