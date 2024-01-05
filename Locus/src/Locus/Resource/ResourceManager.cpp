#include "Lpch.h"
#include "ResourceManager.h"

#define YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

#include "Locus/Core/Application.h"
#include "Locus/Resource/TextureManager.h"
#include "Locus/Resource/MaterialManager.h"

namespace Locus
{
	struct ResourceManagerData
	{
		std::vector<std::filesystem::path> TexturePaths;
		std::vector<std::filesystem::path> MaterialPaths;

		std::filesystem::path ProjectDirectory;
	};

	static ResourceManagerData s_RMData;

	void ResourceManager::Init()
	{
		LOCUS_CORE_INFO("Initializing resource manager");
		s_RMData.ProjectDirectory = Application::Get().GetProjectPath();

		for (const auto& dirEntry : std::filesystem::recursive_directory_iterator(s_RMData.ProjectDirectory / "Assets"))
		{
			if (dirEntry.path().extension() == ".jpg" || dirEntry.path().extension() == ".png")
				s_RMData.TexturePaths.push_back(dirEntry);
			else if (dirEntry.path().extension() == ".lmat")
				s_RMData.MaterialPaths.push_back(dirEntry);
		}

		// Initialize sub resource managers
		TextureManager::Init();
		MaterialManager::Init();
	}

	// Getters
	const std::vector<std::filesystem::path>& ResourceManager::GetTexturePaths() { return s_RMData.TexturePaths; }
	const std::vector<std::filesystem::path>& ResourceManager::GetMaterialPaths() { return s_RMData.MaterialPaths; }
	std::filesystem::path ResourceManager::GetAssetsDirectory() { return s_RMData.ProjectDirectory / "Assets"; }

	UUID ResourceManager::GetResourceUUID(const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path))
			return 0;
		YAML::Node data = YAML::LoadFile(path.string());
		if (data["UUID"])
			return data["UUID"].as<uint64_t>();
		else
			return 0;
	}

}
