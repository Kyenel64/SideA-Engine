#include "Lpch.h"
#include "ModelManager.h"

#define YAML_CPP_STATIC_DEFINE
#include <yaml-cpp/yaml.h>

#include "Locus/Resource/ResourceManager.h"
#include "Locus/Core/Application.h"

namespace Locus
{
	ModelHandle ModelHandle::Null = ModelHandle();

	struct ModelManagerData
	{
		std::unordered_map<ModelHandle, Ref<Model>> Models;
		uint32_t ModelCount = 0;
	};

	static ModelManagerData s_MMData;

	void ModelManager::Init()
	{
		LOCUS_CORE_INFO("Model Manager:");

		for (auto& ModelPath : ResourceManager::GetModelPaths())
		{
			LoadModel(ModelPath);
		}
	}

	ModelHandle ModelManager::LoadModel(const std::filesystem::path& modelPath)
	{
		std::filesystem::path metaPath = modelPath;
		std::filesystem::path metadataPath = Application::Get().GetProjectPath() / metaPath.replace_extension(".meta");
		if (std::filesystem::exists(metadataPath))
		{
			// Load model from metadata path. Temp. Load metadata when implemented
			YAML::Node data = YAML::LoadFile(metadataPath.string());
			ModelHandle matHandle = ModelHandle(modelPath);
			s_MMData.Models[matHandle] = CreateRef<Model>(Application::Get().GetProjectPath() / modelPath);
			s_MMData.ModelCount++;
			LOCUS_CORE_TRACE("  Loaded Model: {0}", modelPath);
			return matHandle;
		}
		else
		{
			ModelHandle matHandle = ModelHandle(modelPath);
			s_MMData.Models[matHandle] = CreateRef<Model>(Application::Get().GetProjectPath() / modelPath);
			s_MMData.ModelCount++;
			YAML::Emitter out;
			out << YAML::BeginMap; // Scene
			out << YAML::Key << "Path" << YAML::Value << Application::Get().GetProjectPath().string() + "/" + modelPath.string();
			out << YAML::EndMap; // End Scene
			std::ofstream fout(metadataPath);
			fout << out.c_str();
			LOCUS_CORE_TRACE("  Generated metadata: {0}", metadataPath);
			LOCUS_CORE_TRACE("  Loaded Model: {0}", modelPath);
			return matHandle;
		}

		return {};
	}

	Ref<Model> ModelManager::GetModel(ModelHandle handle)
	{
		if (s_MMData.Models.find(handle) != s_MMData.Models.end())
			return s_MMData.Models[handle];
		return nullptr;
	}
	const std::unordered_map<ModelHandle, Ref<Model>> ModelManager::GetModels() { return s_MMData.Models; }

	bool ModelManager::IsValid(ModelHandle handle)
	{
		return s_MMData.Models.find(handle) != s_MMData.Models.end();
	}

	Ref<Model> ModelHandle::Get() const
	{
		return ModelManager::GetModel(m_Path);
	}

	ModelHandle::operator bool() const
	{
		return ModelManager::IsValid(m_Path);
	}
}