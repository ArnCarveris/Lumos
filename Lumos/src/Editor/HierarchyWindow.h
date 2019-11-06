#pragma once
#include "lmpch.h"
#include "EditorWindow.h"

#include <entt/entt.hpp>
#include <imgui/imgui.h>

namespace Lumos
{
	class Entity;

	class HierarchyWindow : public EditorWindow
	{
	public:
		HierarchyWindow();
		~HierarchyWindow() = default;

        void DrawNode(entt::entity node, entt::registry& registry);
		void DrawNode(Entity* node, bool defaultOpen = false);
		void OnImGui() override;
		
	private:
		ImGuiTextFilter m_HierarchyFilter;
		entt::entity m_DoubleClicked;
		entt::entity m_HadRecentDroppedEntity;
		entt::entity m_CopiedEntity;
	};
}
