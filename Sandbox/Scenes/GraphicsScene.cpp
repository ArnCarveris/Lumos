#include "GraphicsScene.h"

using namespace Lumos;
using namespace Maths;

GraphicsScene::GraphicsScene(const std::string& SceneName) : Scene(SceneName), m_Terrain(nullptr) {}

GraphicsScene::~GraphicsScene() = default;

void GraphicsScene::OnInit()
{
	Scene::OnInit();
	Application::Instance()->GetSystem<LumosPhysicsEngine>()->SetDampingFactor(0.998f);
	Application::Instance()->GetSystem<LumosPhysicsEngine>()->SetIntegrationType(IntegrationType::RUNGE_KUTTA_4);
	Application::Instance()->GetSystem<LumosPhysicsEngine>()->SetBroadphase(Lumos::CreateRef<Octree>(5, 3, Lumos::CreateRef<SortAndSweepBroadphase>()));

	LoadModels();

	m_SceneBoundingRadius = 200.0f;

	m_pCamera = new ThirdPersonCamera(45.0f, 0.1f, 1000.0f, (float) m_ScreenWidth / (float) m_ScreenHeight);
	m_pCamera->SetPosition(Maths::Vector3(120.0f, 70.0f, 260.0f));

	String environmentFiles[11] =
	{
		"/Textures/cubemap/CubeMap0.tga",
		"/Textures/cubemap/CubeMap1.tga",
		"/Textures/cubemap/CubeMap2.tga",
		"/Textures/cubemap/CubeMap3.tga",
		"/Textures/cubemap/CubeMap4.tga",
		"/Textures/cubemap/CubeMap5.tga",
		"/Textures/cubemap/CubeMap6.tga",
		"/Textures/cubemap/CubeMap7.tga",
		"/Textures/cubemap/CubeMap8.tga",
		"/Textures/cubemap/CubeMap9.tga",
		"/Textures/cubemap/CubeMap10.tga"
	};

	m_EnvironmentMap = Graphics::TextureCube::CreateFromVCross(environmentFiles, 11);

	auto lightEntity = EntityManager::Instance()->CreateEntity("Directional Light");
	lightEntity->AddComponent<Graphics::Light>(Maths::Vector3(26.0f, 22.0f, 48.5f), Maths::Vector4(1.0f), 2.0f);
	lightEntity->AddComponent<Maths::Transform>(Matrix4::Translation(Maths::Vector3(26.0f, 22.0f, 48.5f)));
	//AddEntity(lightEntity);

	Application::Instance()->GetSystem<AudioManager>()->SetListener(m_pCamera);

	auto shadowRenderer = new Graphics::ShadowRenderer();
	auto deferredRenderer = new Graphics::DeferredRenderer(m_ScreenWidth, m_ScreenHeight);
	auto skyboxRenderer = new Graphics::SkyboxRenderer(m_ScreenWidth, m_ScreenHeight, m_EnvironmentMap);
	//shadowRenderer->SetLightEntity(lightEntity);

	deferredRenderer->SetRenderToGBufferTexture(true);
	skyboxRenderer->SetRenderToGBufferTexture(true);

	auto shadowLayer = new Layer3D(shadowRenderer);
	auto deferredLayer = new Layer3D(deferredRenderer);
	auto skyBoxLayer = new Layer3D(skyboxRenderer);
	Application::Instance()->PushLayer(shadowLayer);
	Application::Instance()->PushLayer(deferredLayer);
	Application::Instance()->PushLayer(skyBoxLayer);

	Application::Instance()->GetRenderManager()->SetShadowRenderer(shadowRenderer);
	Application::Instance()->GetRenderManager()->SetSkyBoxTexture(m_EnvironmentMap);
}

void GraphicsScene::OnUpdate(TimeStep* timeStep)
{
	Scene::OnUpdate(timeStep);
}

void GraphicsScene::OnCleanupScene()
{
	if (m_CurrentScene)
	{
		SAFE_DELETE(m_pCamera)
        SAFE_DELETE(m_EnvironmentMap);
	}

	Scene::OnCleanupScene();
}

void GraphicsScene::LoadModels()
{
	//HeightMap
	m_Terrain = EntityManager::Instance()->CreateEntity("heightmap");
	m_Terrain->AddComponent<Maths::Transform>(Matrix4::Scale(Maths::Vector3(1.0f)));
	m_Terrain->AddComponent<TextureMatrixComponent>(Matrix4::Scale(Maths::Vector3(1.0f, 1.0f, 1.0f)));
    Lumos::Ref<Graphics::Mesh> terrain = Lumos::Ref<Graphics::Mesh>(new Terrain());
	auto material = Lumos::CreateRef<Material>();

	material->LoadMaterial("checkerboard", "/CoreTextures/checkerboard.tga");

	m_Terrain->AddComponent<MaterialComponent>(material);
    
	m_Terrain->AddComponent<MeshComponent>(terrain);

	//AddEntity(m_Terrain);
}

int width = 500;
int height = 500;
int lowside = 50;
int lowscale = 10;
float xRand = 1.0f;
float yRand = 150.0f;
float zRand = 1.0f;
float texRandX = 1.0f / 16.0f;
float texRandZ = 1.0f / 16.0f;

void GraphicsScene::OnImGui()
{
    /*ImGui::Begin("Terrain");

	ImGui::SliderInt("Width", &width, 1, 5000);
	ImGui::SliderInt("Height", &height, 1, 5000);
	ImGui::SliderInt("lowside", &lowside, 1, 300);
	ImGui::SliderInt("lowscale", &lowscale, 1, 300);

	ImGui::SliderFloat("xRand", &xRand, 0.0f, 300.0f);
	ImGui::SliderFloat("yRand", &yRand, 0.0f, 300.0f);
	ImGui::SliderFloat("zRand", &zRand, 0.0f, 300.0f);

	ImGui::InputFloat("texRandX", &texRandX);
	ImGui::InputFloat("texRandZ", &texRandZ);
    
    if(ImGui::Button("Rebuild Terrain"))
    {
        EntityManager::Instance()->DeleteEntity(m_Terrain);
        
        m_Terrain = EntityManager::Instance()->CreateEntity("heightmap");
        m_Terrain->AddComponent<TransformComponent>(Matrix4::Scale(Maths::Vector3(1.0f)));
        m_Terrain->AddComponent<TextureMatrixComponent>(Matrix4::Scale(Maths::Vector3(1.0f, 1.0f, 1.0f)));
        Lumos::Ref<Graphics::Mesh> terrain = Lumos::Ref<Graphics::Mesh>(new Terrain(width,height,lowside,lowscale,xRand,yRand,zRand,texRandX,texRandZ));
        auto material = Lumos::CreateRef<Material>();
        
        material->LoadMaterial("checkerboard", "/CoreTextures/checkerboard.tga");
        
        m_Terrain->AddComponent<MaterialComponent>(material);
        m_Terrain->SetBoundingRadius(800.0f);
        
        m_Terrain->AddComponent<MeshComponent>(terrain);
        
        AddEntity(m_Terrain);
    }
    
    ImGui::End();*/
}
