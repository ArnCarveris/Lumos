#pragma once
#include "LM.h"
#include "LumosComponent.h"
#include "Audio/SoundNode.h"

namespace Lumos
{
	class LUMOS_EXPORT SoundComponent : public LumosComponent
	{
	public:
        SoundComponent();
		explicit SoundComponent(Ref<SoundNode>& sound);

		void Init();

		void OnIMGUI() override;
        
        SoundNode* GetSoundNode() const { return m_SoundNode.get(); }

		nlohmann::json Serialise() override { return nullptr; };
		void Deserialise(nlohmann::json& data) override {};
        
    private:
        Ref<SoundNode> m_SoundNode;
	};
}
