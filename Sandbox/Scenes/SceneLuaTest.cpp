#include "SceneLuaTest.h"

using namespace jm;
using namespace maths;

SceneLuaTest::SceneLuaTest(const std::string& SceneName)
	: Scene(SceneName)
{
}

SceneLuaTest::~SceneLuaTest()
{
}

void SceneLuaTest::OnInit()
{
	Scene::OnInit();

	SceneBinding* scene = new SceneBinding(this);
	LuaScripting::GetGlobal()->RegisterObject(SceneBinding::className,"currentScene", scene);


    String root = ROOT_DIR;

	LuaScripting::GetGlobal()->RunFile(root + "/Sandbox/Test.lua");
}

void SceneLuaTest::OnUpdate(TimeStep* timeStep)
{
	LuaScripting::GetGlobal()->SetDeltaTime(static_cast<double>(timeStep->GetSeconds()));
	LuaScripting::GetGlobal()->FixedUpdate();
	Scene::OnUpdate(timeStep);
}

void SceneLuaTest::Render2D()
{
	using namespace jm::internal;

	RenderString("FPS : " + StringFormat::ToString(Engine::Instance()->GetFPS()), Vector2(0.9f, 0.9f), 0.6f, Vector4(1.0f, 1.0f, 1.0f, 1.0f));
}

void SceneLuaTest::OnCleanupScene()
{
	if (m_CurrentScene)
	{
		if (m_pCamera)
		{
			delete m_pCamera;
			m_pCamera = nullptr;
		}
	}

	Scene::OnCleanupScene();
}

void SceneLuaTest::OnIMGUI()
{
	ImGui::Begin(m_SceneName.c_str());
 	if(ImGui::Button("<- Back"))
	{
		Application::Instance()->GetSceneManager()->JumpToScene("SceneSelect");
		ImGui::End();
		return;
	}

    ImGui::End();
}