#include "LM.h"
#include "Layer2D.h"
#include "Graphics/Renderers/Renderer2D.h"
#include "App/Scene.h"

namespace lumos
{
	Layer2D::Layer2D(graphics::Renderer2D* renderer, const std::string& debugName)
		: Layer(debugName), m_Scene(nullptr), m_Renderer(renderer)
	{
	}

	Layer2D::~Layer2D()
	{
		delete m_Renderer;
	}

	void Layer2D::OnAttach()
	{
	}

	void Layer2D::OnDetach()
	{
	}

	void Layer2D::OnUpdate(TimeStep* dt, Scene* scene)
	{
		m_Renderer->BeginScene(scene);
	}

	void Layer2D::OnEvent(Event& event)
	{
        EventDispatcher dispatcher(event);
        dispatcher.Dispatch<WindowResizeEvent>(BIND_EVENT_FN(Layer2D::OnwindowResizeEvent));
	}

	void Layer2D::OnRender(Scene* scene)
	{
		m_Renderer->Render(scene);
	}

    bool Layer2D::OnwindowResizeEvent(WindowResizeEvent & e)
    {
		m_Renderer->OnResize(e.GetWidth(), e.GetHeight());
        return false;
    }
}
