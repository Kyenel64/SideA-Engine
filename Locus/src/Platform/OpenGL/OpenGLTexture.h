// --- OpenGLTexture ----------------------------------------------------------
// OpenGL texure class.
#pragma once

#include <glad/glad.h>

#include "Locus/Renderer/Texture.h"

namespace Locus
{
	class OpenGLTexture2D : public Texture2D
	{
	public:
		OpenGLTexture2D(uint32_t width, uint32_t height);
		OpenGLTexture2D(uint32_t width, uint32_t height, uint32_t rendererID);
		OpenGLTexture2D(const std::filesystem::path& path);
		virtual ~OpenGLTexture2D();

		virtual void SetData(void* data, uint32_t size) override;
		virtual void Bind(uint32_t slot = 0) const override;

		virtual inline uint32_t GetWidth() const override { return m_Width; }
		virtual inline uint32_t GetHeight() const override { return m_Height; }
		virtual inline void SetWidth(uint32_t width) override { m_Width = width; }
		virtual inline void SetHeight(uint32_t height) override { m_Height = height; }
		virtual inline uint32_t GetRendererID() const override { return m_RendererID; }
		virtual inline const std::filesystem::path& GetTexturePath() const override { return m_Path; }
		virtual const std::string GetTextureName() const override;

		virtual bool operator==(const Texture& other) const override
		{
			return m_RendererID == ((OpenGLTexture2D&)other).m_RendererID;
		}

	private:
		std::filesystem::path m_Path;
		uint32_t m_Width, m_Height;
		uint32_t m_RendererID = 0;
		GLenum m_InternalFormat, m_DataFormat;
	};
}
