#include "LocusEditorLayer.h"

#include <imgui/imgui.h>
#include <imgui/imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <ImGuizmo.h>

#include "Widgets/Widgets.h"
#include "Command/Command.h"
#include "Command/CommandHistory.h"
#include "Command/EntityCommands.h"
#include "Command/ValueCommands.h"

namespace Locus
{
	extern Entity g_SelectedEntity = {};

	LocusEditorLayer::LocusEditorLayer()
		: Layer("LocusEditorLayer")
	{
		Application::Get().GetWindow().SetFullscreen();

		m_ProjectPath = Application::Get().GetProjectPath();
		m_ProjectName = Application::Get().GetProjectName();

		// Initialize panels
		m_SceneHierarchyPanel = CreateRef<SceneHierarchyPanel>();
		m_PropertiesPanel = CreateRef<PropertiesPanel>();
		m_ProjectBrowserPanel = CreateRef<ProjectBrowserPanel>();
		m_ConsolePanel = CreateRef<ConsolePanel>();
		m_ResourceInspectorPanel = CreateRef<ResourceInspectorPanel>(m_ProjectBrowserPanel);

		CommandHistory::Init(this);

		// Textures
		m_PlayIcon = Texture2D::Create("resources/icons/PlayIcon.png");
		m_StopIcon = Texture2D::Create("resources/icons/StopIcon.png");
		m_PauseIcon = Texture2D::Create("resources/icons/PauseIcon.png");
		m_PhysicsIcon = Texture2D::Create("resources/icons/SimulatePhysicsIcon.png");
		m_PointerIcon = Texture2D::Create("resources/icons/PointerIcon.png");
		m_TranslateIcon = Texture2D::Create("resources/icons/TranslateIcon.png");
		m_ScaleIcon = Texture2D::Create("resources/icons/ScaleIcon.png");
		m_RotateIcon = Texture2D::Create("resources/icons/RotateIcon.png");

		// Shaders
		m_MaskShader = Shader::Create("resources/shaders/MaskShader.glsl");
		OutlinePostProcessShader = Shader::Create("resources/shaders/OutlinePostProcessShader.glsl");
	}

	LocusEditorLayer::~LocusEditorLayer()
	{
		CommandHistory::Shutdown();
	}

	void LocusEditorLayer::OnAttach()
	{
		LOCUS_PROFILE_FUNCTION();

		// Framebuffer
		FramebufferSpecification framebufferSpecs;
		framebufferSpecs.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::RED_INT, FramebufferTextureFormat::Depth };
		framebufferSpecs.Width = 1920;
		framebufferSpecs.Height = 1080;
		m_Framebuffer = Framebuffer::Create(framebufferSpecs);

		// Camera preview
		FramebufferSpecification activeCameraFramebufferSpecs;
		activeCameraFramebufferSpecs.Attachments = { FramebufferTextureFormat::RGBA8, FramebufferTextureFormat::Depth };
		activeCameraFramebufferSpecs.Width = 640;
		activeCameraFramebufferSpecs.Height = 360;
		m_ActiveCameraFramebuffer = Framebuffer::Create(activeCameraFramebufferSpecs);

		// Mask framebuffer for outline post processing
		FramebufferSpecification maskFramebufferSpecs;
		maskFramebufferSpecs.Attachments = { FramebufferTextureFormat::RED_INT };
		maskFramebufferSpecs.Width = 1920;
		maskFramebufferSpecs.Height = 1080;
		m_MaskFramebuffer = Framebuffer::Create(maskFramebufferSpecs);
		m_MaskTexture = Texture2D::Create(m_MaskFramebuffer->GetSpecification().Width, m_MaskFramebuffer->GetSpecification().Height, m_MaskFramebuffer->GetColorAttachmentRendererID(0));

		// Scene
		m_EditorScene = CreateRef<Scene>();
		m_ActiveScene = m_EditorScene;

		// Panels
		m_SceneHierarchyPanel->SetScene(m_ActiveScene);
		m_PropertiesPanel->SetScene(m_ActiveScene);

		// Editor Camera
		m_EditorCamera = EditorCamera(30.0f, 1920.0f / 1080.0f, 0.1f, 10000.0f);

		m_WindowSize = { Application::Get().GetWindow().GetWidth(), Application::Get().GetWindow().GetHeight() };
	}

	void LocusEditorLayer::OnDetach()
	{
		LOCUS_PROFILE_FUNCTION();
	}

	void LocusEditorLayer::OnUpdate(Timestep deltaTime)
	{
		// Profiling
		LOCUS_PROFILE_FUNCTION();
		RendererStats::StatsStartFrame();

		// On viewport resize
		if (FramebufferSpecification spec = m_Framebuffer->GetSpecification();
			m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f &&
			(spec.Width != m_ViewportSize.x || spec.Height != m_ViewportSize.y))
		{
			m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_MaskFramebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_EditorCamera.SetViewportSize(m_ViewportSize.x, m_ViewportSize.y);
			m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
			m_ActiveCameraViewportSize = m_ViewportSize * 0.2f;
		}

		// On window resize
		if (m_WindowSize.x != Application::Get().GetWindow().GetWidth() || 
			m_WindowSize.y != Application::Get().GetWindow().GetHeight())
		{
			m_WindowSize = { Application::Get().GetWindow().GetWidth(), Application::Get().GetWindow().GetHeight() };
		}

		RendererStats::ResetStats();
		m_Framebuffer->Bind();
		m_Framebuffer->ClearAttachmentInt(1, -1);

		switch (m_SceneState)
		{
			case SceneState::Edit:
			{
				m_ActiveScene->OnEditorUpdate(deltaTime, m_EditorCamera);
				m_EditorCamera.OnUpdate(deltaTime);
				break;
			}
			case SceneState::Play:
			{
				m_ActiveScene->OnRuntimeUpdate(deltaTime);
				break;
			}
			case SceneState::Physics:
			{
				m_ActiveScene->OnPhysicsUpdate(deltaTime, m_EditorCamera);
				m_EditorCamera.OnUpdate(deltaTime);
				break;
			}
			case SceneState::Pause:
			{
				m_ActiveScene->OnRuntimePause(deltaTime);
				break;
			}
			case SceneState::PhysicsPause:
			{
				m_ActiveScene->OnPhysicsPause(deltaTime, m_EditorCamera);
				m_EditorCamera.OnUpdate(deltaTime);
				break;
			}
		}

		// Gizmo visibility
		if (g_SelectedEntity.IsValid())
			m_GizmoVisible = true;
		else
			m_GizmoVisible = false;
		if (m_GizmoType == -1)
			m_GizmoVisible = false;

		// Disable keyboard capture when running scene
		if (m_SceneState == SceneState::Play && m_ViewportFocused)
			m_BlockEditorKeyInput = true;
		else
			m_BlockEditorKeyInput = false;

		OnRenderOverlay();

		m_Framebuffer->Unbind();

		// Make sure this is after unbinding main framebuffer as it uses a separate framebuffer.
		DrawActiveCameraView();

		// Render to the mask frame buffer for post processing effects like outlines.
		DrawToMaskFramebuffer();

		Input::ProcessKeys();

		RendererStats::StatsEndFrame();
	}

	void LocusEditorLayer::OnEvent(Event& e)
	{
		LOCUS_PROFILE_FUNCTION();

		if (m_ViewportHovered)
			m_EditorCamera.OnEvent(e);

		EventDispatcher dispatcher(e);
		dispatcher.Dispatch<WindowCloseEvent>(LOCUS_BIND_EVENT_FN(LocusEditorLayer::OnWindowClose));
		dispatcher.Dispatch<KeyPressedEvent>(LOCUS_BIND_EVENT_FN(LocusEditorLayer::OnKeyPressed));
		dispatcher.Dispatch<MouseButtonPressedEvent>(LOCUS_BIND_EVENT_FN(LocusEditorLayer::OnMouseButtonPressed));
	}

	void LocusEditorLayer::OnImGuiRender()
	{
		LOCUS_PROFILE_FUNCTION();

		if (m_LayoutStyle == LayoutStyle::Default)
			DrawDefaultLayout();
	}

	void LocusEditorLayer::OnRenderOverlay()
	{
		LOCUS_PROFILE_FUNCTION();

		if (g_SelectedEntity.IsValid())
			Renderer::DrawPostProcess(m_MaskTexture, OutlinePostProcessShader);
	}

	bool LocusEditorLayer::OnWindowClose(WindowCloseEvent& e)
	{
		LOCUS_PROFILE_FUNCTION();

		if (m_IsSaved)
			Application::Get().Close();
		else
			m_OpenSavePopup = true;
		e.m_Handled = true;

		return true;
	}

	bool LocusEditorLayer::OnKeyPressed(KeyPressedEvent& e)
	{
		LOCUS_PROFILE_FUNCTION();

		if (m_BlockEditorKeyInput)
			return false;
		bool control = Input::IsKeyHeld(Key::LeftControl) || Input::IsKeyHeld(Key::RightControl);
		bool shift = Input::IsKeyHeld(Key::LeftShift) || Input::IsKeyHeld(Key::RightShift);
		switch (e.GetKeyCode())
		{
			// Scene
			case Key::N:
			{
				if (control)
					NewScene();
				break;
			}
			case Key::O:
			{
				if (control)
					OpenScene();
				break;
			}
			case Key::S:
			{
				if (control && shift)
					SaveSceneAs();
				else if (control)
					SaveScene();
				break;
			}

			case Key::C:
			{
				if (control)
				{
					if (g_SelectedEntity)
						m_ClipboardEntity = g_SelectedEntity;
				}
				break;
			}

			case Key::V:
			{
				if (control)
				{
					if (g_SelectedEntity.IsValid())
						CommandHistory::AddCommand(new DuplicateEntityCommand(m_ActiveScene, g_SelectedEntity));
				}
				break;
			}

			case Key::D:
			{
				if (control)
				{
					if (g_SelectedEntity)
						CommandHistory::AddCommand(new DuplicateEntityCommand(m_ActiveScene, g_SelectedEntity));
				}
				break;
			}

			case Key::Delete:
			{
				if (g_SelectedEntity)
					CommandHistory::AddCommand(new DestroyEntityCommand(m_ActiveScene, g_SelectedEntity));
				break;
			}

			case Key::F:
			{
				if (g_SelectedEntity)
				{
					glm::mat4 transform = m_ActiveScene->GetWorldTransform(g_SelectedEntity);
					glm::vec3 position;
					Math::Decompose(transform, glm::vec3(), glm::quat(), position);
					m_EditorCamera.SetFocalPoint(position);
					m_EditorCamera.SetDistance(10.0f);
				}
				break;
			}
				
			// Command History
			case Key::Y:
			{
				if (control)
					CommandHistory::Redo();
				break;
			}
			case Key::Z:
			{
				if (control)
					CommandHistory::Undo();
				break;
			}

			// Gizmos
			case Key::Q:
				m_GizmoType = -1;
				break;
			case Key::W:
				m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;
				break;
			case Key::E:
				m_GizmoType = ImGuizmo::OPERATION::ROTATE;
				break;
			case Key::R:
			{
				if (control && shift)
				{
					ScriptEngine::ReloadScripts();
					m_PropertiesPanel->m_ScriptClasses = ScriptEngine::GetClassNames();
				}
				else
				{
					m_GizmoType = ImGuizmo::OPERATION::SCALE;
				}
				break;
			}
		}

		return true;
	}

	bool LocusEditorLayer::OnMouseButtonPressed(MouseButtonPressedEvent& e)
	{
		LOCUS_PROFILE_FUNCTION();

		auto [mx, my] = ImGui::GetMousePos();
		mx -= m_ViewportBounds[0].x;
		my -= m_ViewportBounds[0].y;
		my = m_ViewportSize.y - my;
		int mouseX = (int)mx;
		int mouseY = (int)my;

		if (e.GetMouseButton() == Mouse::ButtonLeft)
		{
			if (mouseX >= 0 && mouseY >= 0 && mouseX < (int)m_ViewportSize.x && mouseY < (int)m_ViewportSize.y 
				&& m_ViewportHovered && (!ImGuizmo::IsOver() || ImGuizmo::IsOver() && !m_GizmoVisible))
			{
				int pixelData = m_Framebuffer->ReadPixel(1, mouseX, mouseY); // TODO: This is really slow??
				g_SelectedEntity = pixelData == -1 ? Entity() : Entity((entt::entity)pixelData, m_ActiveScene.get());
				m_GizmoFirstClick = true;
			}
		}
		return false;
	}

	void LocusEditorLayer::NewScene()
	{
		LOCUS_PROFILE_FUNCTION();

		if (m_SceneState != SceneState::Edit)
			OnSceneStop();
		g_SelectedEntity = {};
		m_EditorScene = CreateRef<Scene>();
		m_EditorScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_ActiveScene = m_EditorScene;
		m_SceneHierarchyPanel->SetScene(m_ActiveScene);
		m_PropertiesPanel->SetScene(m_ActiveScene);
		m_SavePath = std::string();
		m_IsSaved = false;
		CommandHistory::Reset();
	}

	void LocusEditorLayer::OpenScene()
	{
		LOCUS_PROFILE_FUNCTION();

		std::string path = FileDialogs::OpenFile("Locus Scene (*.locus)\0*.locus\0");
		if (!path.empty())
			OpenScene(path);
	}

	void LocusEditorLayer::OpenScene(const std::filesystem::path& path)
	{
		LOCUS_PROFILE_FUNCTION();

		if (m_SceneState != SceneState::Edit)
			OnSceneStop();
		g_SelectedEntity = {};
		m_EditorScene = CreateRef<Scene>();
		m_EditorScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
		m_ActiveScene = m_EditorScene;
		m_SceneHierarchyPanel->SetScene(m_ActiveScene);
		m_PropertiesPanel->SetScene(m_ActiveScene);
		SceneSerializer serializer(m_EditorScene);
		serializer.Deserialize(path.string());
		m_SavePath = path.string();
		m_IsSaved = true;
		CommandHistory::Reset();
	}

	// Returns false if canceling file dialog.
	bool LocusEditorLayer::SaveSceneAs()
	{
		LOCUS_PROFILE_FUNCTION();

		std::string path = FileDialogs::SaveFile("Locus Scene (*.locus)\0*.locus\0");
		if (!path.empty())
		{
			SceneSerializer serializer(m_EditorScene);
			serializer.Serialize(path);
			m_SavePath = path;
			m_IsSaved = true;
			return true;
		}
		return false;
	}

	bool LocusEditorLayer::SaveScene()
	{
		LOCUS_PROFILE_FUNCTION();

		if (m_SavePath.empty())
		{
			return SaveSceneAs();
		}
		else
		{
			SceneSerializer serializer(m_EditorScene);
			serializer.Serialize(m_SavePath);
			m_IsSaved = true;
			return true;
		}
	}

	void LocusEditorLayer::DrawGizmo()
	{
		LOCUS_PROFILE_FUNCTION();

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y, m_ViewportSize.x, m_ViewportSize.y);
		ImGuizmo::AllowAxisFlip(true);

		// Camera
		const glm::mat4* proj = nullptr;
		const glm::mat4* view = nullptr;

		if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Physics || m_SceneState == SceneState::PhysicsPause)
		{
			proj = &m_EditorCamera.GetProjection();
			view = &m_EditorCamera.GetViewMatrix();
		}
		else 
		{
			Entity primaryCameraEntity = m_ActiveScene->GetPrimaryCameraEntity();
			if (primaryCameraEntity.IsValid())
			{
				auto& camera = primaryCameraEntity.GetComponent<CameraComponent>();
				proj = &camera.Camera.GetProjection();
				view = &glm::inverse(m_ActiveScene->GetWorldTransform(primaryCameraEntity));
			}
			else
			{
				return;
			}
		}

		// Entity transform
		auto& tc = g_SelectedEntity.GetComponent<TransformComponent>();
		glm::mat4& transform = m_ActiveScene->GetWorldTransform(g_SelectedEntity);

		// Snapping
		bool snap = Input::IsKeyHeld(Key::LeftControl);
		float snapValue = 0.5f; // Snap 0.5m for translation & scale
		if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
			snapValue = 45.0f; // Snap to 45 degrees for rotation
		float snapValues[3] = { snapValue, snapValue, snapValue };

		ImGuizmo::Manipulate(glm::value_ptr(*view), glm::value_ptr(*proj),
			(ImGuizmo::OPERATION)m_GizmoType, ImGuizmo::LOCAL, glm::value_ptr(transform),
			nullptr, snap ? snapValues : nullptr);

		// Convert back to local space
		if (tc.Parent)
			transform = glm::inverse(m_ActiveScene->GetWorldTransform(m_ActiveScene->GetEntityByUUID(tc.Parent))) * transform;

		glm::vec3 translation, scale;
		glm::quat rotation;
		Math::Decompose(transform, scale, rotation, translation);

		if (ImGuizmo::IsUsing())
		{
			switch (m_GizmoType)
			{
				case ImGuizmo::TRANSLATE:
				{
					CommandHistory::AddCommand(new ChangeValueCommand(translation, tc.LocalPosition));
					break;
				}
				case ImGuizmo::ROTATE:
				{
					// Do this in Euler in an attempt to preserve any full revolutions (> 360)
					glm::vec3 originalRotationEuler = glm::radians(tc.GetLocalRotation());

					// Map original rotation to range [-180, 180] which is what ImGuizmo gives us
					originalRotationEuler.x = fmodf(originalRotationEuler.x + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
					originalRotationEuler.y = fmodf(originalRotationEuler.y + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
					originalRotationEuler.z = fmodf(originalRotationEuler.z + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();

					glm::vec3 deltaRotationEuler = glm::eulerAngles(rotation) - originalRotationEuler;

					// Try to avoid drift due numeric precision
					if (fabs(deltaRotationEuler.x) < 0.001) deltaRotationEuler.x = 0.0f;
					if (fabs(deltaRotationEuler.y) < 0.001) deltaRotationEuler.y = 0.0f;
					if (fabs(deltaRotationEuler.z) < 0.001) deltaRotationEuler.z = 0.0f;

					glm::vec3 rotationEuler = tc.GetLocalRotation();
					CommandHistory::AddCommand(new ChangeValueCommand(rotationEuler + glm::degrees(deltaRotationEuler), tc.LocalRotation));
					break;
				}
				case ImGuizmo::SCALE:
				{
					CommandHistory::AddCommand(new ChangeValueCommand(scale, tc.LocalScale));
					break;
				}
			}
		}
	}

	void LocusEditorLayer::ProcessViewportDragDrop()
	{
		LOCUS_PROFILE_FUNCTION();

		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_ITEM_PATH"))
			{
				if (m_SceneState == SceneState::Play)
					OnSceneStop();
				const wchar_t* path = (const wchar_t*)payload->Data;
				OpenScene(m_ProjectPath / path);
			}
			ImGui::EndDragDropTarget();
		}
	}

	void LocusEditorLayer::OnScenePlay()
	{
		LOCUS_PROFILE_FUNCTION();

		m_SceneState = SceneState::Play;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		ScriptEngine::OnRuntimeStart(m_ActiveScene);
		m_ActiveScene->OnRuntimeStart();
		m_SceneHierarchyPanel->SetScene(m_ActiveScene);
		ImGui::SetWindowFocus("Viewport");
	}

	void LocusEditorLayer::OnPhysicsPlay()
	{
		LOCUS_PROFILE_FUNCTION();

		m_SceneState = SceneState::Physics;
		m_ActiveScene = Scene::Copy(m_EditorScene);
		m_ActiveScene->OnPhysicsStart();
		m_SceneHierarchyPanel->SetScene(m_ActiveScene);
		ImGui::SetWindowFocus("Viewport");
	}

	void LocusEditorLayer::OnSceneStop()
	{
		LOCUS_PROFILE_FUNCTION();

		g_SelectedEntity = {};
		m_SceneState = SceneState::Edit;
		ScriptEngine::OnRuntimeStop();
		m_ActiveScene->OnRuntimeStop();

		m_ActiveScene = m_EditorScene;
		m_SceneHierarchyPanel->SetScene(m_ActiveScene);
	}

	void LocusEditorLayer::DrawDefaultLayout()
	{
		LOCUS_PROFILE_FUNCTION();

		// --- Dockspace ---
		static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;
		ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoScrollbar |
			ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
			ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus | ImGuiWindowFlags_NoScrollWithMouse;
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::SetNextWindowPos(viewport->Pos);
		ImGui::SetNextWindowSize(viewport->Size);
		ImGui::SetNextWindowViewport(viewport->ID);

		if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
			window_flags |= ImGuiWindowFlags_NoBackground;

		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 13.0f, 13.0f });
		ImGui::PushStyleColor(ImGuiCol_WindowBg, LocusColors::DarkGrey);
		ImGui::Begin("Layout", false, window_flags);
		ImGui::PopStyleVar(2);

		ImGuiDockNodeFlags docknodeFlags = ImGuiDockNodeFlags_None;
		//docknodeFlags |= ImGuiDockNodeFlags_TabBarCollapsed; // Behaves the same as showing tab bar
		docknodeFlags |= ImGuiDockNodeFlags_WindowRounding;
		ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
		ImGui::PushStyleColor(ImGuiCol_Separator, LocusColors::Transparent);
		ImGui::PushStyleColor(ImGuiCol_SeparatorHovered, LocusColors::Transparent);
		ImGui::PushStyleColor(ImGuiCol_SeparatorActive, LocusColors::Transparent);
		ImGui::DockSpace(dockspace_id, { 0.0f, 0.0f }, docknodeFlags);
		ImGui::PopStyleColor(4);


		// --- Panels ---
		DrawToolbar();
		DrawViewport();
		DrawDebugPanel();
		m_SceneHierarchyPanel->OnImGuiRender();
		m_PropertiesPanel->OnImGuiRender();
		m_ProjectBrowserPanel->OnImGuiRender();
		m_ConsolePanel->OnImGuiRender();
		m_ResourceInspectorPanel->OnImGuiRender();
		//ImGui::ShowDemoWindow();


		ProcessSavePopup();

		ImGui::End();
	}

	void LocusEditorLayer::DrawViewport()
	{
		LOCUS_PROFILE_FUNCTION();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_TabBarAlignLeft | ImGuiWindowFlags_DockedWindowBorder;
		if (!m_IsSaved)
			windowFlags |= ImGuiWindowFlags_UnsavedDocument;
		ImGui::Begin("Viewport", false, windowFlags);

		// --- Draw Viewport ---
		m_ViewportSize = { ImGui::GetContentRegionAvail().x, ImGui::GetContentRegionAvail().y };
		uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImVec2 topLeft = { ImGui::GetCursorPosX() + ImGui::GetWindowPos().x,ImGui::GetCursorPosY() + ImGui::GetWindowPos().y };
		ImVec2 botRight = { topLeft.x + m_ViewportSize.x, topLeft.y + m_ViewportSize.y };
		draw_list->AddImageRounded((void*)(uintptr_t)textureID, topLeft, botRight, { 0, 1 }, { 1, 0 }, ImGui::GetColorU32(LocusColors::White), 10.0f, ImDrawFlags_RoundCornersBottom);
		
		m_ViewportBounds[0] = { topLeft.x, topLeft.y };
		m_ViewportBounds[1] = { botRight.x, botRight.y };
		m_ViewportHovered = ImGui::IsMouseHoveringRect({ topLeft.x, topLeft.y }, { topLeft.x + m_ViewportSize.x, topLeft.y + m_ViewportSize.y });
		m_ViewportFocused = ImGui::IsWindowFocused();

		ProcessViewportDragDrop();

		// --- Gizmo ---
		// Checks for first click to prevent moving the object when selecting an entity.
		if (g_SelectedEntity.IsValid() && m_GizmoType != -1)
		{
			if (!m_GizmoFirstClick)
				DrawGizmo();
			else
				m_GizmoFirstClick = false;
		}

		// --- Viewport Toolbar ---
		glm::vec2 toolbarPos = { 10.0f, 10.0f };
		DrawViewportToolbar(toolbarPos);

		// --- Active camera view ---
		if (g_SelectedEntity.IsValid() && m_SceneState == SceneState::Edit)
		{
			if (g_SelectedEntity.HasComponent<CameraComponent>())
			{
				// Draw view of camera if a camera component is selected
				float margin = 10.0f;
				ImVec2 viewTopLeft = { ImGui::GetWindowPos().x + m_ViewportSize.x - m_ActiveCameraViewportSize.x - margin,
					ImGui::GetWindowPos().y + m_ViewportSize.y - m_ActiveCameraViewportSize.y - margin + ImGui::CalcTextSize("A").y + ImGui::GetStyle().FramePadding.y * 2.0f };
				ImVec2 viewBotRight = { viewTopLeft.x + m_ActiveCameraViewportSize.x, viewTopLeft.y + m_ActiveCameraViewportSize.y };
				ImGui::SetCursorScreenPos(viewTopLeft);
				ImGui::BeginChild("ActiveCameraView", { m_ActiveCameraViewportSize.x, m_ActiveCameraViewportSize.y }, true);

				uint32_t texID = m_ActiveCameraFramebuffer->GetColorAttachmentRendererID();
				draw_list->AddImageRounded((void*)(uintptr_t)texID, viewTopLeft, viewBotRight, { 0, 1 }, { 1, 0 }, 
					ImGui::GetColorU32(LocusColors::White), ImGui::GetStyle().WindowRounding);

				ImGui::EndChild();
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void LocusEditorLayer::DrawViewportToolbar(const glm::vec2& position)
	{
		LOCUS_PROFILE_FUNCTION();

		ImGui::PushStyleColor(ImGuiCol_Button, LocusColors::Transparent);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, LocusColors::Transparent);
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, LocusColors::Transparent);
		ImGui::PushStyleColor(ImGuiCol_ChildBg, { 0.0f, 0.0f, 0.0f, 0.3f });
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, ImGui::GetStyle().ItemSpacing.y });
		
		float toolbarHeight = 35.0f;

		ImGui::SetCursorPos({ position.x, ImGui::GetCurrentWindow()->DockTabItemRect.GetHeight() + position.y });
		ImGui::BeginChild("Gizmo toolbar", { toolbarHeight * 4.0f, toolbarHeight }, true);

		int imagePadding = 5;
		float imageWidth = ImGui::GetContentRegionAvail().y - imagePadding * 2.0f;
		ImVec4 tintColor;

		// --- Gizmo buttons ---
		// Pointer
		tintColor = LocusColors::Orange;
		if (m_GizmoType == -1)
			tintColor = LocusColors::White;
		if (ImGui::ImageButton((ImTextureID)(uint64_t)m_PointerIcon->GetRendererID(), { imageWidth, imageWidth }, { 0, 1 }, { 1, 0 }, imagePadding, LocusColors::Transparent, tintColor))
			m_GizmoType = -1;

		ImGui::SameLine();

		// Translate
		tintColor = LocusColors::Orange;
		if (m_GizmoType == ImGuizmo::OPERATION::TRANSLATE)
			tintColor = LocusColors::White;
		if (ImGui::ImageButton((ImTextureID)(uint64_t)m_TranslateIcon->GetRendererID(), { imageWidth, imageWidth }, { 0, 1 }, { 1, 0 }, imagePadding, LocusColors::Transparent, tintColor))
			m_GizmoType = ImGuizmo::OPERATION::TRANSLATE;

		ImGui::SameLine();

		// Rotate
		tintColor = LocusColors::Orange;
		if (m_GizmoType == ImGuizmo::OPERATION::ROTATE)
			tintColor = LocusColors::White;
		if (ImGui::ImageButton((ImTextureID)(uint64_t)m_RotateIcon->GetRendererID(), { imageWidth, imageWidth }, { 0, 1 }, { 1, 0 }, imagePadding, LocusColors::Transparent, tintColor))
			m_GizmoType = ImGuizmo::OPERATION::ROTATE;

		ImGui::SameLine();

		// Scale
		tintColor = LocusColors::Orange;
		if (m_GizmoType == ImGuizmo::OPERATION::SCALE)
			tintColor = LocusColors::White;
		if (ImGui::ImageButton((ImTextureID)(uint64_t)m_ScaleIcon->GetRendererID(), { imageWidth, imageWidth }, { 0, 1 }, { 1, 0 }, imagePadding, LocusColors::Transparent, tintColor))
			m_GizmoType = ImGuizmo::OPERATION::SCALE;

		ImGui::EndChild();

		if (ImGui::IsItemHovered())
			m_ViewportHovered = false;

		
		ImGui::SameLine(ImGui::GetItemRectSize().x + 20.0f);


		// --- Editor settings ---
		ImGui::BeginChild("Editor toolbar", { 55.0f, toolbarHeight }, true);

		if (ImGui::Button("View", { -1.0f, -1.0f }))
			ImGui::OpenPopup("ViewSettings");

		ProcessViewSettingsPopup();

		ImGui::EndChild();

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(4);
	}

	void LocusEditorLayer::DrawToolbar()
	{
		LOCUS_PROFILE_FUNCTION();

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
		ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 0.0f, 0.0f });
		ImGui::PushStyleColor(ImGuiCol_WindowBg, LocusColors::Transparent);
		ImGui::Begin("Toolbar", false, windowFlags);
		ImGui::GetCurrentWindow()->DockNode->LocalFlags |= ImGuiDockNodeFlags_TabBarCollapsed | ImGuiDockNodeFlags_NoResize;

		// --- Settings bar ---
		ImGui::SetCursorPos({ ImGui::GetCursorPosX() + 1.0f, ImGui::GetCursorPosY() + 1.0f });
		ImGui::BeginChild("SettingsBar", { 250.0f, -1.0f }, true, ImGuiWindowFlags_None);

		ImGui::PushStyleColor(ImGuiCol_Button, LocusColors::Transparent);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, ImGui::GetStyle().ItemSpacing.y });
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);

		float settingsBarWidth = ImGui::GetWindowWidth();
		if (ImGui::Button("File", { settingsBarWidth / 3.0f, -1.0f }))
			ImGui::OpenPopup("File Settings");

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6.0f, 6.0f });
		ImGui::SetNextWindowPos({ ImGui::GetCursorPos().x + ImGui::GetWindowPos().x, ImGui::GetCursorPos().y + ImGui::GetWindowPos().y - ImGui::GetStyle().ItemSpacing.y });
		if (ImGui::BeginPopup("File Settings"))
		{
			if (ImGui::MenuItem("New", "Ctrl+N"))
				NewScene();

			if (ImGui::MenuItem("Open...", "Ctrl+O"))
				OpenScene();

			if (ImGui::MenuItem("Save", "Ctrl+S"))
				SaveScene();

			if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S"))
				SaveSceneAs();

			if (ImGui::MenuItem("Exit"))
			{
				if (m_IsSaved)
					Application::Get().Close();
				else
					m_OpenSavePopup = true;
			}
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();

		ImGui::SameLine();

		if (ImGui::Button("Project", { settingsBarWidth / 3.0f, -1.0f }))
		{
			ImGui::OpenPopup("Project Settings");
		}

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 6.0f, 6.0f });
		ImGui::SetNextWindowPos({ ImGui::GetCursorPos().x + ImGui::GetWindowPos().x + settingsBarWidth / 3.0f, 
			ImGui::GetCursorPos().y + ImGui::GetWindowPos().y - ImGui::GetStyle().ItemSpacing.y });
		if (ImGui::BeginPopup("Project Settings"))
		{
			if (ImGui::MenuItem("Reload Scripts", "Ctrl+Shift+R"))
			{
				ScriptEngine::ReloadScripts();
				m_PropertiesPanel->m_ScriptClasses = ScriptEngine::GetClassNames();
			}

			if (ImGui::MenuItem("Rescan Resources"))
			{
				ResourceManager::Rescan();
			}
			ImGui::EndPopup();
		}
		ImGui::PopStyleVar();

		ImGui::SameLine();

		if (ImGui::Button("Help", { settingsBarWidth / 3.0f, -1.0f }))
		{

		}

		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor();
		ImGui::EndChild();


		ImGui::SameLine(ImGui::GetContentRegionAvail().x - ImGui::GetWindowHeight() * 4.0f + 5.0f); // 5.0f is imagePadding


		// --- Runtime buttons ---
		ImGui::BeginChild("RuntimeButtons", { -1.0f, -1.0f }, true);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, { 0.0f, ImGui::GetStyle().ItemSpacing.y });
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);
		ImGui::PushStyleColor(ImGuiCol_Button, LocusColors::Transparent);

		int imagePadding = 5;
		float imageWidth = ImGui::GetWindowHeight() - imagePadding * 2.0f;
		ImVec4 tintColor;

		// Play button
		tintColor = LocusColors::LightGrey;
		if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::Pause)
			tintColor = LocusColors::White;
		if (ImGui::ImageButton((ImTextureID)(uint64_t)m_PlayIcon->GetRendererID(), { imageWidth, imageWidth }, { 0, 1 }, { 1, 0 }, imagePadding, LocusColors::Transparent, tintColor))
		{
			if (m_SceneState == SceneState::Edit)
				OnScenePlay();
			else if (m_SceneState == SceneState::Pause)
				m_SceneState = SceneState::Play;
		}

		ImGui::SameLine();

		// Physics button
		tintColor = LocusColors::LightGrey;
		if (m_SceneState == SceneState::Edit || m_SceneState == SceneState::PhysicsPause)
			tintColor = LocusColors::White;
		if (ImGui::ImageButton((ImTextureID)(uint64_t)m_PhysicsIcon->GetRendererID(), { imageWidth, imageWidth }, { 0, 1 }, { 1, 0 }, imagePadding, LocusColors::Transparent, tintColor))
		{
			if (m_SceneState == SceneState::Edit)
				OnPhysicsPlay();
			else if (m_SceneState == SceneState::PhysicsPause)
				m_SceneState = SceneState::Physics;
		}

		ImGui::SameLine();

		// Pause button
		tintColor = LocusColors::LightGrey;
		if (m_SceneState == SceneState::Play || m_SceneState == SceneState::Physics)
			tintColor = LocusColors::White;
		if (ImGui::ImageButton((ImTextureID)(uint64_t)m_PauseIcon->GetRendererID(), { imageWidth, imageWidth }, { 0, 1 }, { 1, 0 }, imagePadding, LocusColors::Transparent, tintColor))
		{
			if (m_SceneState == SceneState::Play)
				m_SceneState = SceneState::Pause;
			else if (m_SceneState == SceneState::Physics)
				m_SceneState = SceneState::PhysicsPause;
		}

		ImGui::SameLine();

		// Stop button
		tintColor = LocusColors::LightGrey;
		if (m_SceneState != SceneState::Edit)
			tintColor = LocusColors::White;
		if (ImGui::ImageButton((ImTextureID)(uint64_t)m_StopIcon->GetRendererID(), { imageWidth, imageWidth }, { 0, 1 }, { 1, 0 }, imagePadding, LocusColors::Transparent, tintColor))
		{
			if (m_SceneState != SceneState::Edit)
				OnSceneStop();
		}

		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
		ImGui::EndChild();

		ImGui::End();
		ImGui::PopStyleColor();
		ImGui::PopStyleVar(2);
	}
	
	void LocusEditorLayer::ProcessSavePopup()
	{
		LOCUS_PROFILE_FUNCTION();

		if (m_OpenSavePopup)
			ImGui::OpenPopup("You have unsaved changes...");

		ImVec2 center = { ImGui::GetWindowPos().x + m_WindowSize.x / 2, ImGui::GetWindowPos().y + m_WindowSize.y / 2};
		ImGui::SetNextWindowPos(center, ImGuiCond_None, { 0.5f, 0.5f });
		if (ImGui::BeginPopupModal("You have unsaved changes...", &m_OpenSavePopup, ImGuiWindowFlags_AlwaysAutoResize))
		{
			g_SelectedEntity = {};

			ImGui::Text("Save Current Project?");

			if (ImGui::Button("Save and Close", { 140.0f, 40.0f }))
			{
				if (SaveScene())
					Application::Get().Close();
			}

			ImGui::SameLine();

			if (ImGui::Button("Don't Save", { 140.0f, 40.0f }))
			{
				Application::Get().Close();
			}

			ImGui::SameLine();

			if (ImGui::Button("Cancel", { 140.0f, 40.0f }))
			{
				m_OpenSavePopup = false;
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void LocusEditorLayer::ProcessViewSettingsPopup()
	{
		LOCUS_PROFILE_FUNCTION();

		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, { 10.0f, 10.0f });

		ImGui::SetNextWindowPos({ ImGui::GetWindowPos().x, ImGui::GetWindowPos().y + ImGui::GetWindowSize().y});
		if (ImGui::BeginPopup("ViewSettings"))
		{
			float labelWidth = 150.0f;
			// Grid visibility
			Widgets::DrawControlLabel("Collision Mesh", { labelWidth, 0.0f });
			ImGui::SameLine();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsMouseDoubleClicked(0))
					m_ShowAllCollisionMesh = false;
			}
			bool collisionMesh = m_ShowAllCollisionMesh;
			if (ImGui::Checkbox("##Collision Mesh", &collisionMesh))
				m_ShowAllCollisionMesh = !m_ShowAllCollisionMesh;

			// Background color
			Widgets::DrawControlLabel("Background Color", { labelWidth, 0.0f });
			ImGui::SameLine();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsMouseDoubleClicked(0))
					m_EditorCamera.SetBackgroundColor({ 0.25f, 0.5f, 0.5f, 1.0f });
			}
			glm::vec4 bgColor = m_EditorCamera.GetBackgroundColor();
			if (ImGui::ColorEdit4("##Background Color", glm::value_ptr(bgColor)))
				m_EditorCamera.SetBackgroundColor(bgColor);

			// Grid visibility
			Widgets::DrawControlLabel("Grid Visibility", { labelWidth, 0.0f });
			ImGui::SameLine();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsMouseDoubleClicked(0))
					m_EditorCamera.SetGridVisibility(true);
			}
			bool gridEnabled = m_EditorCamera.GetGridVisibility();
			if (ImGui::Checkbox("##Grid Visibility", &gridEnabled))
				m_EditorCamera.SetGridVisibility(gridEnabled);

			// Grid scale
			Widgets::DrawControlLabel("Grid Scale", { labelWidth, 0.0f });
			ImGui::SameLine();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsMouseDoubleClicked(0))
					m_EditorCamera.SetGridScale(1.0f);
			}
			float gridScale = m_EditorCamera.GetGridScale();
			if (ImGui::DragFloat("##Grid Scale", &gridScale, 0.01f, 0.01f, FLT_MAX))
				m_EditorCamera.SetGridScale(gridScale);

			// Grid color
			Widgets::DrawControlLabel("Grid Color", { labelWidth, 0.0f });
			ImGui::SameLine();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsMouseDoubleClicked(0))
					m_EditorCamera.SetGridColor({ 0.8f, 0.8f, 0.8f, 1.0f });
			}
			glm::vec4 gridColor = m_EditorCamera.GetGridColor();
			if (ImGui::ColorEdit4("##Grid Color", glm::value_ptr(gridColor)))
				m_EditorCamera.SetGridColor(gridColor);

			// Near Clip
			Widgets::DrawControlLabel("Near Clip", { labelWidth, 0.0f });
			ImGui::SameLine();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsMouseDoubleClicked(0))
					m_EditorCamera.SetNearClip(0.1f);;
			}
			float nearClip = m_EditorCamera.GetNearClip();
			if (ImGui::DragFloat("##Near Clip", &nearClip, 0.01f, 0.0f, FLT_MAX))
				m_EditorCamera.SetNearClip(nearClip);

			// Far Clip
			Widgets::DrawControlLabel("Far Clip", { labelWidth, 0.0f });
			ImGui::SameLine();
			if (ImGui::IsItemHovered())
			{
				ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
				if (ImGui::IsMouseDoubleClicked(0))
					m_EditorCamera.SetFarClip(10000.0f);;
			}
			float farClip = m_EditorCamera.GetFarClip();
			if (ImGui::DragFloat("##Far Clip", &farClip, 5.0f, 0.0f, FLT_MAX))
				m_EditorCamera.SetFarClip(farClip);

			ImGui::EndPopup();
		}

		ImGui::PopStyleVar();
	}

	void LocusEditorLayer::DrawDebugPanel()
	{
		LOCUS_PROFILE_FUNCTION();

		ImGuiWindowFlags windowFlags = ImGuiWindowFlags_TabBarAlignLeft | ImGuiWindowFlags_DockedWindowBorder;
		ImGui::Begin("Debug", false, windowFlags);

		// FPS
		ImGui::Text("FPS: %i", Application::Get().GetFPS());

		auto& stats = RendererStats::GetStats();
		ImGui::Text("Renderer Stats:");
		ImGui::Text("Draw Calls: %d", stats.DrawCalls);
		ImGui::Text("Quads: %d", stats.QuadCount);
		ImGui::Text("Vertices: %d", stats.GetTotalVertexCount());
		ImGui::Text("Indices: %d", stats.GetTotalIndexCount());

		// IDs
		ImGui::Text("Entity Value: %d", (entt::entity)g_SelectedEntity);

		// Collision
		if (g_SelectedEntity.IsValid())
		{
			if (g_SelectedEntity.HasComponent<BoxCollider2DComponent>())
				ImGui::Text("Collision category: %d", g_SelectedEntity.GetComponent<BoxCollider2DComponent>().CollisionCategory);
		}
		// Child debug
		if (g_SelectedEntity.IsValid())
		{
			if (g_SelectedEntity.HasComponent<ChildComponent>())
			{
				auto& cc = g_SelectedEntity.GetComponent<ChildComponent>();
				ImGui::Separator();
				ImGui::Text("Children:");
				ImGui::Indent();
				for (auto& id : cc.ChildEntities)
				{
					Entity entity = m_ActiveScene->GetEntityByUUID(id);
					auto& tag = entity.GetComponent<TagComponent>().Tag;
					ImGui::Text(tag.c_str());
				}
			}
		}

		// Transforms
		if (g_SelectedEntity.IsValid())
		{
			ImGui::Separator();
			ImGui::Text("Transforms");

			auto& tc = g_SelectedEntity.GetComponent<TransformComponent>();
			if (tc.Parent)
				ImGui::Text("Parent: %s", m_ActiveScene->GetEntityByUUID(tc.Parent).GetComponent<TagComponent>().Tag.c_str());
			else
				ImGui::Text("Parent: Entity::Null");

			ImGui::Text("Self: %s", m_ActiveScene->GetEntityByUUID(tc.Self).GetComponent<TagComponent>().Tag.c_str());

			glm::mat4 worldTransform = m_ActiveScene->GetWorldTransform(g_SelectedEntity);
			glm::vec3 worldPosition, worldScale;
			glm::quat worldRotationQuat;
			Math::Decompose(worldTransform, worldScale, worldRotationQuat, worldPosition);
			glm::vec3 worldRotation = glm::eulerAngles(worldRotationQuat);

			ImGui::Text("LocalPosition: %f, %f, %f", tc.LocalPosition.x, tc.LocalPosition.y, tc.LocalPosition.z);
			ImGui::Text("WorldPosition: %f, %f, %f", worldPosition.x, worldPosition.y, worldPosition.z);

			ImGui::Text("LocalRotation: %f, %f, %f", tc.LocalRotation.x, tc.LocalRotation.y, tc.LocalRotation.z);
			ImGui::Text("WorldRotation: %f, %f, %f", glm::degrees(worldRotation.x), glm::degrees(worldRotation.y), glm::degrees(worldRotation.z));

			ImGui::Text("LocalScale: %f, %f, %f", tc.LocalScale.x, tc.LocalScale.y, tc.LocalScale.z);
			ImGui::Text("WorldScale: %f, %f, %f", worldScale.x, worldScale.y, worldScale.z);
		}

		ImGui::End();
	}

	void LocusEditorLayer::DrawActiveCameraView()
	{
		LOCUS_PROFILE_FUNCTION();

		if (g_SelectedEntity.IsValid())
		{
			if (g_SelectedEntity.HasComponent<CameraComponent>())
			{
				m_ActiveCameraFramebuffer->Bind();
				m_ActiveScene->OnPreviewUpdate(g_SelectedEntity);
				m_ActiveCameraFramebuffer->Unbind();
			}
		}
	}

	void LocusEditorLayer::DrawToMaskFramebuffer()
	{
		LOCUS_PROFILE_FUNCTION();

		// Mask framebuffer
		m_MaskFramebuffer->ClearAttachmentInt(0, 0);
		if (g_SelectedEntity.IsValid())
		{
			m_MaskFramebuffer->Bind();
			if (m_SceneState == SceneState::Play || m_SceneState == SceneState::Pause)
			{
				if (g_SelectedEntity.HasComponent<CameraComponent>())
					Renderer::BeginScene(g_SelectedEntity.GetComponent<CameraComponent>().Camera, m_ActiveScene->GetWorldTransform(g_SelectedEntity));
			}
			else
			{
				Renderer::BeginScene(m_EditorCamera);
			}
			
			m_MaskFramebuffer->ClearAttachmentInt(0, 0);

			if (g_SelectedEntity.HasComponent<CubeRendererComponent>())
				Renderer3D::DrawCubeMask(m_ActiveScene->GetWorldTransform(g_SelectedEntity), m_MaskShader);
			if (g_SelectedEntity.HasComponent<SpriteRendererComponent>() || g_SelectedEntity.HasComponent<CircleRendererComponent>())
				Renderer2D::DrawQuadMask(m_ActiveScene->GetWorldTransform(g_SelectedEntity), m_MaskShader);

			Renderer::EndScene();
			m_MaskFramebuffer->Unbind();
		}
	}
}
