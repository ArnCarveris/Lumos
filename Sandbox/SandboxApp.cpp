#include <LumosEngine.h>
#include <App/EntryPoint.h>
#include "Scenes/Scene3D.h"
#include "Scenes/GraphicsScene.h"
#include "Scenes/SceneModelViewer.h"
#include "Scenes/Scene2D.h"
#include "Scenes/MaterialTest.h"

using namespace Lumos;

class Game : public Application
{
public:
	explicit Game(const WindowProperties& windowProperties) : Application(windowProperties)
	{
	}

	~Game()
	{
	}

	void Init() override
	{
		Application::Init();

		const String root = ROOT_DIR;
		VFS::Get()->Mount("Meshes", root + "/Sandbox/res/meshes");
		VFS::Get()->Mount("Textures", root + "/Sandbox/res/textures");
		VFS::Get()->Mount("Sounds", root + "/Sandbox/res/sounds");

		GetSceneManager()->EnqueueScene<SceneModelViewer>("SceneModelViewer");
		GetSceneManager()->EnqueueScene<Scene2D>("2D Test");
		GetSceneManager()->EnqueueScene<Scene3D>("Physics Scene");
		GetSceneManager()->EnqueueScene<GraphicsScene>("Terrain Test");
		GetSceneManager()->EnqueueScene<MaterialTest>("Material Test");
		GetSceneManager()->SwitchScene(2);
        GetSceneManager()->ApplySceneSwitch();
	}
};

Lumos::Application* Lumos::CreateApplication()
{
#ifdef LUMOS_PLATFORM_IOS
    WindowProperties windowProperties;
#else
    WindowProperties windowProperties = LuaManager::Instance()->LoadConfigFile(ROOT_DIR"/Sandbox/Settings.lua");
	windowProperties.ShowConsole = true;
#endif

    return new Game(windowProperties);
}
