#include "pch.h"
#include "ImGuiLayer.h"

#include "Platfrom/OpenGL/ImGuiOpenGLRenderer.h"
#include "PlagC/Application.h"
#include "PlagC/Core.h"

#include <GLFW/glfw3.h>

PlagC::ImGuiLayer::ImGuiLayer() : Layer("ImGui Layer")
{
	PC_CORE_TRACE("ImGUI CREATED");
}


PlagC::ImGuiLayer::~ImGuiLayer()
{

}

void PlagC::ImGuiLayer::OnAttach()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();

	//-----------------------------------------------------
	// Config
	//-----------------------------------------------------

	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	// Later
	// io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

	//-----------------------------------------------------
	// Backend Flags
	//-----------------------------------------------------

	io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
	io.BackendFlags |= ImGuiBackendFlags_HasSetMousePos;

	//-----------------------------------------------------
	// Keyboard Mapping
	//-----------------------------------------------------

	io.KeyMap[ImGuiKey_Tab] = GLFW_KEY_TAB;
	io.KeyMap[ImGuiKey_LeftArrow] = GLFW_KEY_LEFT;
	io.KeyMap[ImGuiKey_RightArrow] = GLFW_KEY_RIGHT;
	io.KeyMap[ImGuiKey_UpArrow] = GLFW_KEY_UP;
	io.KeyMap[ImGuiKey_DownArrow] = GLFW_KEY_DOWN;
	io.KeyMap[ImGuiKey_PageUp] = GLFW_KEY_PAGE_UP;
	io.KeyMap[ImGuiKey_PageDown] = GLFW_KEY_PAGE_DOWN;
	io.KeyMap[ImGuiKey_Home] = GLFW_KEY_HOME;
	io.KeyMap[ImGuiKey_End] = GLFW_KEY_END;
	io.KeyMap[ImGuiKey_Insert] = GLFW_KEY_INSERT;
	io.KeyMap[ImGuiKey_Delete] = GLFW_KEY_DELETE;
	io.KeyMap[ImGuiKey_Backspace] = GLFW_KEY_BACKSPACE;
	io.KeyMap[ImGuiKey_Space] = GLFW_KEY_SPACE;
	io.KeyMap[ImGuiKey_Enter] = GLFW_KEY_ENTER;
	io.KeyMap[ImGuiKey_Escape] = GLFW_KEY_ESCAPE;

	io.KeyMap[ImGuiKey_A] = GLFW_KEY_A;
	io.KeyMap[ImGuiKey_C] = GLFW_KEY_C;
	io.KeyMap[ImGuiKey_V] = GLFW_KEY_V;
	io.KeyMap[ImGuiKey_X] = GLFW_KEY_X;
	io.KeyMap[ImGuiKey_Y] = GLFW_KEY_Y;
	io.KeyMap[ImGuiKey_Z] = GLFW_KEY_Z;

	//-----------------------------------------------------
	// Style
	//-----------------------------------------------------

	ImGui::StyleColorsDark();

	ImGuiStyle& style = ImGui::GetStyle();

	style.WindowPadding = ImVec2(10, 10);
	style.FramePadding = ImVec2(8, 5);
	//style.CellPadding = ImVec2(6, 6);
	style.ItemSpacing = ImVec2(8, 6);
	style.ItemInnerSpacing = ImVec2(6, 4);

	style.IndentSpacing = 20.0f;

	style.ScrollbarSize = 14.0f;
	style.GrabMinSize = 10.0f;

	style.WindowBorderSize = 1.0f;
	style.ChildBorderSize = 1.0f;
	style.PopupBorderSize = 1.0f;
	style.FrameBorderSize = 0.0f;
	style.TabBorderSize = 0.0f;

	style.WindowRounding = 6.0f;
	style.ChildRounding = 5.0f;
	style.FrameRounding = 5.0f;
	style.PopupRounding = 5.0f;
	style.ScrollbarRounding = 8.0f;
	style.GrabRounding = 5.0f;
	style.TabRounding = 5.0f;

	SetDarkThemeColors();

	//-----------------------------------------------------
	// OpenGL Backend
	//-----------------------------------------------------

	ImGui_ImplOpenGL3_Init("#version 410");
}

void PlagC::ImGuiLayer::OnDetach()
{

}

void PlagC::ImGuiLayer::OnUpdate()
{
	Begin();

	DrawDockspace();

	DrawSceneHierarchy();
	DrawViewport();
	DrawInspector();
	DrawConsole();
	DrawStatistics();

	End();
}

void PlagC::ImGuiLayer::Begin()
{
	ImGuiIO& io = ImGui::GetIO();

	Application& app = Application::Get();

	io.DisplaySize = ImVec2(
		(float)app.GetWindow().GetWidth(),
		(float)app.GetWindow().GetHeight());

	float time = (float)glfwGetTime();

	io.DeltaTime =
		m_Time > 0.0f ?
		(time - m_Time) :
		(1.0f / 60.0f);

	m_Time = time;

	ImGui_ImplOpenGL3_NewFrame();
	ImGui::NewFrame();
}

void PlagC::ImGuiLayer::End()
{
	ImGui::Render();

	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

	ImGuiIO& io = ImGui::GetIO();

	if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
	{
		GLFWwindow* backupCurrentContext = glfwGetCurrentContext();

		ImGui::UpdatePlatformWindows();
		ImGui::RenderPlatformWindowsDefault();

		glfwMakeContextCurrent(backupCurrentContext);
	}
}

void PlagC::ImGuiLayer::SetDarkThemeColors()
{
	ImVec4* colors = ImGui::GetStyle().Colors;

	colors[ImGuiCol_Text] = ImVec4(0.95f, 0.96f, 0.98f, 1.00f);
	colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

	colors[ImGuiCol_WindowBg] = ImVec4(0.10f, 0.105f, 0.11f, 1.00f);
	colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.105f, 0.11f, 1.00f);
	colors[ImGuiCol_PopupBg] = ImVec4(0.08f, 0.08f, 0.08f, 1.00f);

	colors[ImGuiCol_Border] = ImVec4(0.20f, 0.205f, 0.21f, 1.00f);

	colors[ImGuiCol_FrameBg] = ImVec4(0.20f, 0.205f, 0.21f, 1.00f);
	colors[ImGuiCol_FrameBgHovered] = ImVec4(0.28f, 0.285f, 0.29f, 1.00f);
	colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.305f, 0.31f, 1.00f);

	colors[ImGuiCol_TitleBg] = ImVec4(0.15f, 0.150f, 0.15f, 1.00f);
	colors[ImGuiCol_TitleBgActive] = ImVec4(0.15f, 0.150f, 0.15f, 1.00f);

	colors[ImGuiCol_MenuBarBg] = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);

	colors[ImGuiCol_Header] = ImVec4(0.20f, 0.35f, 0.60f, 0.70f);
	colors[ImGuiCol_HeaderHovered] = ImVec4(0.26f, 0.45f, 0.75f, 1.00f);
	colors[ImGuiCol_HeaderActive] = ImVec4(0.20f, 0.35f, 0.60f, 1.00f);

	colors[ImGuiCol_Button] = ImVec4(0.20f, 0.35f, 0.60f, 0.70f);
	colors[ImGuiCol_ButtonHovered] = ImVec4(0.26f, 0.45f, 0.75f, 1.00f);
	colors[ImGuiCol_ButtonActive] = ImVec4(0.18f, 0.30f, 0.55f, 1.00f);

	colors[ImGuiCol_Tab] = ImVec4(0.15f, 0.16f, 0.19f, 1.00f);
	colors[ImGuiCol_TabHovered] = ImVec4(0.26f, 0.45f, 0.75f, 1.00f);
	colors[ImGuiCol_TabActive] = ImVec4(0.20f, 0.35f, 0.60f, 1.00f);

	colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.50f, 1.00f, 1.00f);

	colors[ImGuiCol_ResizeGrip] = ImVec4(0.20f, 0.35f, 0.60f, 0.40f);
	colors[ImGuiCol_ResizeGripHovered] = ImVec4(0.26f, 0.45f, 0.75f, 1.00f);
	colors[ImGuiCol_ResizeGripActive] = ImVec4(0.26f, 0.45f, 0.75f, 1.00f);

	colors[ImGuiCol_Separator] = ImVec4(0.25f, 0.25f, 0.25f, 1.00f);
}

void PlagC::ImGuiLayer::DrawDockspace()
{
	static bool optFullscreen = true;
	static bool optPadding = false;

	ImGuiWindowFlags windowFlags =
		ImGuiWindowFlags_MenuBar |
		ImGuiWindowFlags_NoDocking;

	if (optFullscreen)
	{
		const ImGuiViewport* viewport = ImGui::GetMainViewport();

		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);

		ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);

		windowFlags |= ImGuiWindowFlags_NoTitleBar;
		windowFlags |= ImGuiWindowFlags_NoCollapse;
		windowFlags |= ImGuiWindowFlags_NoResize;
		windowFlags |= ImGuiWindowFlags_NoMove;
		windowFlags |= ImGuiWindowFlags_NoBringToFrontOnFocus;
		windowFlags |= ImGuiWindowFlags_NoNavFocus;
	}

	if (!optPadding)
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

	ImGui::Begin("PlagC Dockspace", nullptr, windowFlags);

	if (!optPadding)
		ImGui::PopStyleVar();

	if (optFullscreen)
		ImGui::PopStyleVar(2);

	if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_DockingEnable)
	{
		ImGuiID dockspaceID = ImGui::GetID("PlagCDockspace");
		ImGui::DockSpace(dockspaceID, ImVec2(0.0f, 0.0f));
	}

	DrawMenuBar();

	ImGui::End();
}

void PlagC::ImGuiLayer::DrawMenuBar()
{
	if (!ImGui::BeginMenuBar())
		return;

	//---------------------------------------------------
	// File
	//---------------------------------------------------

	if (ImGui::BeginMenu("File"))
	{
		ImGui::MenuItem("New Scene", "Ctrl+N");
		ImGui::MenuItem("Open Scene", "Ctrl+O");
		ImGui::MenuItem("Save Scene", "Ctrl+S");
		ImGui::MenuItem("Save Scene As");

		ImGui::Separator();

		ImGui::MenuItem("Exit");

		ImGui::EndMenu();
	}

	//---------------------------------------------------
	// Edit
	//---------------------------------------------------

	if (ImGui::BeginMenu("Edit"))
	{
		ImGui::MenuItem("Undo", "Ctrl+Z");
		ImGui::MenuItem("Redo", "Ctrl+Y");

		ImGui::Separator();

		ImGui::MenuItem("Cut", "Ctrl+X");
		ImGui::MenuItem("Copy", "Ctrl+C");
		ImGui::MenuItem("Paste", "Ctrl+V");

		ImGui::EndMenu();
	}

	//---------------------------------------------------
	// View
	//---------------------------------------------------

	if (ImGui::BeginMenu("View"))
	{
		ImGui::MenuItem("Scene Hierarchy");
		ImGui::MenuItem("Inspector");
		ImGui::MenuItem("Viewport");
		ImGui::MenuItem("Console");
		ImGui::MenuItem("Statistics");

		ImGui::EndMenu();
	}

	//---------------------------------------------------
	// Scene
	//---------------------------------------------------

	if (ImGui::BeginMenu("Scene"))
	{
		ImGui::MenuItem("Create Empty");
		ImGui::MenuItem("Camera");
		ImGui::MenuItem("Directional Light");
		ImGui::MenuItem("Point Light");
		ImGui::MenuItem("Cube");
		ImGui::MenuItem("Sphere");
		ImGui::MenuItem("Plane");

		ImGui::EndMenu();
	}

	//---------------------------------------------------
	// Renderer
	//---------------------------------------------------

	if (ImGui::BeginMenu("Renderer"))
	{
		static bool wireframe = false;
		static bool vsync = true;
		static bool bloom = false;
		static bool shadows = true;
		static bool hdr = true;

		ImGui::MenuItem("Wireframe", nullptr, &wireframe);
		ImGui::MenuItem("VSync", nullptr, &vsync);
		ImGui::MenuItem("Bloom", nullptr, &bloom);
		ImGui::MenuItem("Shadows", nullptr, &shadows);
		ImGui::MenuItem("HDR", nullptr, &hdr);

		ImGui::EndMenu();
	}

	//---------------------------------------------------
	// Window
	//---------------------------------------------------

	if (ImGui::BeginMenu("Window"))
	{
		ImGui::MenuItem("Content Browser");
		ImGui::MenuItem("Material Editor");
		ImGui::MenuItem("Animation");
		ImGui::MenuItem("Profiler");

		ImGui::EndMenu();
	}

	//---------------------------------------------------
	// Help
	//---------------------------------------------------

	if (ImGui::BeginMenu("Help"))
	{
		ImGui::MenuItem("Documentation");
		ImGui::MenuItem("About PlagC");

		ImGui::EndMenu();
	}

	//---------------------------------------------------
	// Right Side Statistics
	//---------------------------------------------------

	float fps = ImGui::GetIO().Framerate;

	std::string stats =
		"FPS : " + std::to_string((int)fps) +
		"    CPU : " +
		std::to_string(1000.0f / fps).substr(0, 4) +
		" ms";

	float width = ImGui::CalcTextSize(stats.c_str()).x;

	ImGui::SetCursorPosX(ImGui::GetWindowWidth() - width - 20.0f);

	ImGui::Text("%s", stats.c_str());

	ImGui::EndMenuBar();
}

void PlagC::ImGuiLayer::DrawSceneHierarchy()
{
	ImGui::Begin("Scene");

	if (ImGui::TreeNodeEx("Main Scene",
		ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Selectable("Camera");
		ImGui::Selectable("Directional Light");
		ImGui::Selectable("Cube");
		ImGui::Selectable("Player");

		ImGui::TreePop();
	}

	ImGui::Separator();

	if (ImGui::Button("+ Create"))
	{
	}

	ImGui::End();
}

void PlagC::ImGuiLayer::DrawViewport()
{
	ImGui::Begin("Viewport");

	ImVec2 viewportSize = ImGui::GetContentRegionAvail();

	ImDrawList* drawList = ImGui::GetWindowDrawList();

	ImVec2 min = ImGui::GetCursorScreenPos();

	ImVec2 max =
	{
		min.x + viewportSize.x,
		min.y + viewportSize.y
	};

	//-----------------------------------------------------
	// Background
	//-----------------------------------------------------

	drawList->AddRectFilled(
		min,
		max,
		IM_COL32(30, 30, 30, 255));

	//-----------------------------------------------------
	// Border
	//-----------------------------------------------------

	drawList->AddRect(
		min,
		max,
		IM_COL32(70, 70, 70, 255));

	//-----------------------------------------------------
	// Crosshair
	//-----------------------------------------------------

	ImVec2 center =
	{
		(min.x + max.x) * 0.5f,
		(min.y + max.y) * 0.5f
	};

	drawList->AddLine(
		ImVec2(center.x - 20, center.y),
		ImVec2(center.x + 20, center.y),
		IM_COL32(120, 120, 120, 255),
		2.0f);

	drawList->AddLine(
		ImVec2(center.x, center.y - 20),
		ImVec2(center.x, center.y + 20),
		IM_COL32(120, 120, 120, 255),
		2.0f);

	//-----------------------------------------------------
	// Text
	//-----------------------------------------------------

	drawList->AddText(
		ImVec2(min.x + 15, min.y + 15),
		IM_COL32_WHITE,
		"Renderer Output");

	drawList->AddText(
		ImVec2(center.x - 55, center.y + 30),
		IM_COL32(150, 150, 150, 255),
		"Framebuffer");

	//-----------------------------------------------------
	// Reserve Space
	//-----------------------------------------------------

	ImGui::Dummy(viewportSize);

	ImGui::End();
}

void PlagC::ImGuiLayer::DrawInspector()
{
	ImGui::Begin("Inspector");

	static float position[3] = { 0.0f, 1.0f, 0.0f };
	static float rotation[3] = { 0.0f, 0.0f, 0.0f };
	static float scale[3] = { 1.0f, 1.0f, 1.0f };

	static bool visible = true;
	static bool castShadow = true;

	ImGui::Text("Cube");
	ImGui::Separator();

	if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::DragFloat3("Position", position, 0.1f);
		ImGui::DragFloat3("Rotation", rotation, 0.5f);
		ImGui::DragFloat3("Scale", scale, 0.1f);
	}

	if (ImGui::CollapsingHeader("Mesh", ImGuiTreeNodeFlags_DefaultOpen))
	{
		ImGui::Text("Mesh : Cube");
		ImGui::Text("Vertices : 24");
		ImGui::Text("Triangles : 12");
	}

	if (ImGui::CollapsingHeader("Material", ImGuiTreeNodeFlags_DefaultOpen))
	{
		static int shader = 0;

		const char* shaders[] =
		{
			"PBR",
			"Phong",
			"Unlit"
		};

		ImGui::Combo("Shader", &shader, shaders, IM_ARRAYSIZE(shaders));

		ImGui::Checkbox("Visible", &visible);
		ImGui::Checkbox("Cast Shadow", &castShadow);

		static float color[3] = { 0.7f, 0.7f, 0.7f };
		ImGui::ColorEdit3("Albedo", color);
	}

	ImGui::End();
}

void PlagC::ImGuiLayer::DrawConsole()
{
	ImGui::Begin("Console");

	if (ImGui::Button("Clear"))
	{
		// Clear log later
	}

	ImGui::Separator();

	ImGui::BeginChild("Log");

	ImGui::TextColored(
		ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
		"[Info] Engine Started");

	ImGui::SameLine(350);
	ImGui::Text("00:00:01");

	ImGui::TextColored(
		ImVec4(0.2f, 1.0f, 0.2f, 1.0f),
		"[Info] Renderer Initialized");

	ImGui::SameLine(350);
	ImGui::Text("00:00:02");

	ImGui::TextColored(
		ImVec4(1.0f, 1.0f, 0.2f, 1.0f),
		"[Warn] Texture Missing");

	ImGui::SameLine(350);
	ImGui::Text("00:00:04");

	ImGui::TextColored(
		ImVec4(1.0f, 0.3f, 0.3f, 1.0f),
		"[Error] Shader Compilation Failed");

	ImGui::SameLine(350);
	ImGui::Text("00:00:05");

	ImGui::EndChild();

	ImGui::End();
}

void PlagC::ImGuiLayer::DrawStatistics()
{
	ImGui::Begin("Statistics");

	ImGuiIO& io = ImGui::GetIO();

	ImGui::Text("Performance");
	ImGui::Separator();

	ImGui::Text("FPS            %.0f", io.Framerate);
	ImGui::Text("Frame Time     %.3f ms", 1000.0f / io.Framerate);

	ImGui::Spacing();

	ImGui::Text("Renderer");
	ImGui::Separator();

	ImGui::Text("Draw Calls     18");
	ImGui::Text("Vertices       152000");
	ImGui::Text("Triangles      50000");

	ImGui::Spacing();

	ImGui::Text("Memory");
	ImGui::Separator();

	ImGui::Text("Textures       14");
	ImGui::Text("Shaders        8");
	ImGui::Text("Meshes         6");

	ImGui::Spacing();

	ImGui::Text("Camera");
	ImGui::Separator();

	ImGui::Text("Position");
	ImGui::BulletText("X : %.2f", 0.0f);
	ImGui::BulletText("Y : %.2f", 1.0f);
	ImGui::BulletText("Z : %.2f", 5.0f);

	ImGui::End();
}



void PlagC::ImGuiLayer::OnEvent(Event& e)
{
	EventDispatcher dispatcher(e);
	dispatcher.Dispatch<MouseButtonPressedEvent>(PC_BIND_EVENT_FN(ImGuiLayer::OnMouseButtonPressedEvent));
	dispatcher.Dispatch<MouseButtonReleasedEvent>(PC_BIND_EVENT_FN(ImGuiLayer::OnMouseButtonReleasedEvent));
	dispatcher.Dispatch<MouseMovedEvent>(PC_BIND_EVENT_FN(ImGuiLayer::OnMouseMovedEvent));
	dispatcher.Dispatch<MouseScrolledEvent>(PC_BIND_EVENT_FN(ImGuiLayer::OnMouseScrolledEvent));
	dispatcher.Dispatch<KeyPressedEvent>(PC_BIND_EVENT_FN(ImGuiLayer::OnKeyPressedEvent));
	dispatcher.Dispatch<KeyReleasedEvent>(PC_BIND_EVENT_FN(ImGuiLayer::OnKeyReleasedEvent));
	dispatcher.Dispatch<WindowResizeEvent>(PC_BIND_EVENT_FN(ImGuiLayer::OnWindowResizeEvent));
	dispatcher.Dispatch<KeyTypedEvent>(PC_BIND_EVENT_FN(ImGuiLayer::OnKeyTypedEvent));
}

bool PlagC::ImGuiLayer::OnMouseButtonPressedEvent(MouseButtonPressedEvent& e)
{
	PC_CORE_INFO("IMGUI MOUSE MOVED!");
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDown[(int)(e.GetMouseButton())] = true;
	return false;
}

bool PlagC::ImGuiLayer::OnMouseButtonReleasedEvent(MouseButtonReleasedEvent& e)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseDown[(int)(e.GetMouseButton())] = false;
	return false;
}

bool PlagC::ImGuiLayer::OnMouseMovedEvent(MouseMovedEvent& e)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MousePos = ImVec2(e.GetX(), e.GetY());
	return false;
}

bool PlagC::ImGuiLayer::OnMouseScrolledEvent(MouseScrolledEvent& e)
{
	ImGuiIO& io = ImGui::GetIO();
	io.MouseWheelH = e.GetXOffset();
	io.MouseWheel = e.GetYOffset();
	return false;
}

bool PlagC::ImGuiLayer::OnKeyPressedEvent(KeyPressedEvent& e)
{
	ImGuiIO& io = ImGui::GetIO();
	io.KeysDown[e.GetKeyCode()] = true;

	io.KeyCtrl = io.KeysDown[GLFW_KEY_LEFT_CONTROL] || io.KeysDown[GLFW_KEY_RIGHT_CONTROL];
	io.KeyShift = io.KeysDown[GLFW_KEY_LEFT_SHIFT] || io.KeysDown[GLFW_KEY_RIGHT_SHIFT];
	io.KeyAlt = io.KeysDown[GLFW_KEY_LEFT_ALT] || io.KeysDown[GLFW_KEY_RIGHT_ALT];
	io.KeySuper = io.KeysDown[GLFW_KEY_LEFT_SUPER] || io.KeysDown[GLFW_KEY_RIGHT_SUPER];
	return false;
}

bool PlagC::ImGuiLayer::OnKeyReleasedEvent(KeyReleasedEvent& e)
{
	ImGuiIO& io = ImGui::GetIO();
	io.KeysDown[e.GetKeyCode()] = false;

	return false;
}

bool PlagC::ImGuiLayer::OnKeyTypedEvent(KeyTypedEvent& e)
{
	ImGuiIO& io = ImGui::GetIO();

	unsigned int keyCode = e.GetKeyCode();

	if (keyCode > 0 && keyCode < 0x10000)
		io.AddInputCharacter((unsigned short)keyCode);
	return false;
}

bool PlagC::ImGuiLayer::OnWindowResizeEvent(WindowResizeEvent& e)
{
	ImGuiIO& io = ImGui::GetIO();
	io.DisplaySize = ImVec2(e.GetWidth(), e.GetHeight());
	io.DisplayFramebufferScale = ImVec2(1.0f, 1.0f);
	glViewport(0, 0, e.GetWidth(), e.GetHeight());
	return false;
}


