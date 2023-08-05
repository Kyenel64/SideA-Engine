#pragma once

#include "SideA.h"
#include "Panels/SceneHierarchyPanel.h"
#include "SideA/Renderer/EditorCamera.h"

namespace SideA
{
	class SideAEditorLayer : public Layer
	{
	public:
		SideAEditorLayer();
		virtual ~SideAEditorLayer() {}

		virtual void OnAttach() override;
		virtual void OnDetach() override;
		virtual void OnUpdate(Timestep deltaTime) override;
		virtual void OnEvent(Event& event) override;
		virtual void OnImGuiRender() override;

	private:
		// Events
		bool OnKeyPressed(KeyPressedEvent& e);
		bool OnMouseButtonPressed(MouseButtonPressedEvent& e);
		bool OnMouseButtonReleased(MouseButtonReleasedEvent& e);

		void NewScene();
		void OpenScene();
		void SaveSceneAs();
		void SaveScene();

		void showGizmoUI();

	private:
		std::string m_SavePath;

		Ref<Scene> m_ActiveScene;

		// Viewport
		Ref<Framebuffer> m_Framebuffer;
		EditorCamera m_EditorCamera;
		bool m_ViewportFocused = false, m_ViewportHovered = false;
		glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
		glm::vec2 m_ViewportBounds[2];
		Entity m_HoveredEntity;
		Entity m_SelectedEntity;


		// Viewport Gizmo
		int m_GizmoType = -1;

		// Panels
		SceneHierarchyPanel m_SceneHierarchyPanel;

	};
}
