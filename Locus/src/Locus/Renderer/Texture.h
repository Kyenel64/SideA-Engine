// --- Texture ----------------------------------------------------------------
// Texture interface.
#pragma once

#include "Locus/Core/Core.h"

namespace Locus
{
	class Texture
	{
	public:
		virtual ~Texture() = default;

		virtual uint32_t GetWidth() const = 0;
		virtual uint32_t GetHeight() const = 0;
		virtual void SetWidth(uint32_t width) = 0;
		virtual void SetHeight(uint32_t height) = 0;
		virtual uint32_t GetRendererID() const = 0;

		virtual const std::string& GetTexturePath() const = 0;
		virtual const std::string GetTextureName() const = 0;

		virtual void SetData(void* data, uint32_t size) = 0;

		virtual void Bind(uint32_t slot = 0) const = 0;

		virtual bool operator==(const Texture& other) const = 0;
	};
	


	class Texture2D : public Texture
	{
	public:
		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(uint32_t width, uint32_t height, uint32_t rendererID);
		static Ref<Texture2D> Create(const std::string& path);
	};
}
