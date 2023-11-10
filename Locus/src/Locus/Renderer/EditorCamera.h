// --- EditorCamera -----------------------------------------------------------
// The editor camera is only used in the editor. 
#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

#include "Locus/Core/Timestep.h"
#include "Locus/Events/Event.h"
#include "Locus/Events/MouseEvent.h"
#include "Locus/Renderer/Camera.h"

namespace Locus
{
	class EditorCamera : public Camera
	{
	public:
		EditorCamera() = default;
		EditorCamera(float fov, float aspectRatio, float nearClip, float farClip);
		~EditorCamera() = default;

		void OnUpdate(Timestep deltaTime);
		void OnEvent(Event& e);


		inline float GetDistance() const { return m_Distance; }
		inline void SetDistance(float distance) { m_Distance = distance; }
		inline void SetFocalPoint(const glm::vec3& point) { m_FocalPoint = point; }
		inline void SetViewportSize(float width, float height) { m_ViewportWidth = width; m_ViewportHeight = height; CalculateProjection(); }
		inline void SetBackgroundColor(const glm::vec4& color) { m_BackgroundColor = color; }
		inline void SetGridColor(const glm::vec4& color) { m_GridColor = color; }
		inline void SetGridScale(float scale) { m_GridScale = scale; }
		inline void SetNearClip(float nearClip) { m_NearClip = nearClip; }
		inline void SetFarClip(float farClip) { m_FarClip = farClip; }
		inline void SetGridVisibility(bool visibility) { m_GridVisibility = visibility; }

		const glm::mat4& GetViewMatrix() const { return m_ViewMatrix; }
		glm::mat4 GetViewProjectionMatrix() const { return m_Projection * m_ViewMatrix; }
		glm::vec3 GetForwardDirection() const;
		glm::vec3 GetRightDirection() const;
		glm::vec3 GetUpDirection() const;
		const glm::vec3& GetPosition() const { return m_Position; }
		glm::quat GetOrientation() const;
		float GetNearClip() const { return m_NearClip; }
		float GetFarClip() const { return m_FarClip; }
		const glm::vec4& GetBackgroundColor() const { return m_BackgroundColor; }
		const glm::vec4& GetGridColor() const { return m_GridColor; }
		float GetGridScale() const { return m_GridScale; }
		bool GetGridVisibility() const { return m_GridVisibility; }

	private:
		void CalculateProjection();
		void CalculateView();

		bool OnMouseScroll(MouseScrolledEvent& e);

		void MouseRotate(const glm::vec2& delta);
		void MousePan(const glm::vec2& delta);
		void MouseZoom(float delta);


		glm::vec3 CalculatePosition() const;

	private:
		glm::mat4 m_ViewMatrix; // Projection in Camera class

		glm::vec4 m_BackgroundColor = { 0.25f, 0.5f, 0.5f, 1.0f };
		glm::vec4 m_GridColor = { 0.8f, 0.8f, 0.8f, 1.0f };

		glm::vec3 m_Position = { 0.0f, 0.0f, 0.0f };
		float m_Pitch = 0.5f;
		glm::vec3 m_FocalPoint = { 0.0f, 0.0f, 0.0f };
		float m_Yaw = -0.785f;

		float m_Distance = 20.0f;
		float m_GridScale = 1.0f;
		float m_FOV = 45.0f, m_AspectRatio = 1.778f;
		float m_NearClip = 0.1f, m_FarClip = 10000.0f;

		float m_ViewportWidth = 1920.0f, m_ViewportHeight = 1080.0f;
		glm::vec2 m_InitialMousePosition = { 0.0f, 0.0f };
		float m_MouseRotateSpeed = 10.0f, m_MousePanSpeed = 10.0f, m_MouseScrollSpeed = 10.0f;

		bool m_GridVisibility = true;
	};
}