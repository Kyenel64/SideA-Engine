#include "Lpch.h"
#include "Application.h"

#include "Locus/Renderer/Renderer.h"


namespace Locus
{

// Binds to an event function. Ex: OnWindowClose()
//#define BIND_EVENT_FN(x) std::bind(&Application::x, this, std::placeholders::_1)

	Application* Application::s_Instance = nullptr;

	Application::Application(const std::string& name, ApplicationCommandLineArgs args)
		: m_CommandLineArgs(args)
	{
		LOCUS_PROFILE_FUNCTION();

		LOCUS_CORE_ASSERT(!s_Instance, "Application already exists");
		s_Instance = this;

		m_Window = Window::Create(name);
		m_Window->SetEventCallback(LOCUS_BIND_EVENT_FN(Application::OnEvent));

		Renderer::Init();

		m_ImGuiLayer = new ImGuiLayer();
		PushOverlay(m_ImGuiLayer);
	}

	void Application::PushLayer(Layer* layer)
	{
		LOCUS_PROFILE_FUNCTION();

		m_LayerStack.PushLayer(layer);
	}

	void Application::PushOverlay(Layer* overlay)
	{
		LOCUS_PROFILE_FUNCTION();

		m_LayerStack.PushOverlay(overlay);
	}

	// Passes event function to dispatcher.
	void Application::OnEvent(Event& e)
	{
		LOCUS_PROFILE_FUNCTION();

		EventDispatcher dispatcher(e);
		// Dispatch event if event class type matches event type
		dispatcher.Dispatch<WindowCloseEvent>(LOCUS_BIND_EVENT_FN(Application::OnWindowClose));
		dispatcher.Dispatch<WindowResizeEvent>(LOCUS_BIND_EVENT_FN(Application::OnWindowResize));

		// Iterate each layer's events backwards
		for (auto it = m_LayerStack.rbegin(); it != m_LayerStack.rend(); ++it)
		{
			if (e.m_Handled)
				break;
			(*it)->OnEvent(e);

		}
	}

	void Application::Run()
	{
		LOCUS_PROFILE_FUNCTION();

		while (m_Running)
		{
			LOCUS_PROFILE_SCOPE("Run loop");
			// Calculate deltaTime
			float time = m_Window->GetTime();
			Timestep timestep = time - m_LastFrameTime;
			m_LastFrameTime = time;

			if (!m_Minimized)
			{
				{
					LOCUS_PROFILE_SCOPE("LayerStack OnUpdate");
					// Iterate through layers on update BEFORE window update.
					for (Layer* layer : m_LayerStack)
					layer->OnUpdate(timestep);
				}
				
			}

			// Render ImGui
			m_ImGuiLayer->Begin();
			{
				LOCUS_PROFILE_SCOPE("LayerStack OnImGuiRender");
				for (Layer* layer : m_LayerStack)
					layer->OnImGuiRender();
			}
			m_ImGuiLayer->End();


			m_Window->OnUpdate();
		}
	}

	void Application::Close()
	{
		Renderer::Shutdown();
		m_Running = false;
	}

	bool Application::OnWindowClose(WindowCloseEvent& e)
	{
		if (m_IsSaved)
			Close();
		else
			m_OpenSaveChangesPopup = true;
		return true;
	}

	bool Application::OnWindowResize(WindowResizeEvent& e)
	{
		LOCUS_PROFILE_FUNCTION();

		if (e.GetWidth() == 0 || e.GetHeight() == 0)
		{
			m_Minimized = true;
			return false;
		}
		m_Minimized = false;
		Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());

		return false;
	}
}