// --- RendererAPI ------------------------------------------------------------
// RendererAPI interface.
#pragma once

#include <glm/glm.hpp>

#include "Locus/Renderer/VertexArray.h"

namespace Locus
{
	class RendererAPI
	{
	public:
		enum class API
		{
			None = 0, OpenGL = 1
		};

	public:
		virtual ~RendererAPI() = default;

		virtual void Init() = 0;
		virtual void SetClearColor(const glm::vec4 color) = 0;
		virtual void Clear() = 0;

		virtual void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0) = 0;
		virtual void DrawArray(const Ref<VertexArray>& vertexArray, uint32_t vertexCount = 0) = 0;
		virtual void DrawIndexedInstanced(const Ref<VertexArray>& vertexArray, uint32_t indexCount, uint32_t instanceCount, uint32_t instanceBase = 0) = 0;
		virtual void DrawArrayInstanced(const Ref<VertexArray>& vertexArray, uint32_t vertexCount, uint32_t instanceCount, uint32_t instanceBase = 0) = 0;
		virtual void DrawLine(const Ref<VertexArray>& vertexArray, uint32_t vertexCount = 0) = 0;

		virtual void Resize(int x, int y, int width, int height) = 0;
		virtual void SetLineWidth(float width) = 0;

		inline static API GetAPI() { return s_API; }

		static Scope<RendererAPI> Create();

	private:
		static API s_API;
	};
}
