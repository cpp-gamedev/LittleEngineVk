#pragma once
#include <filesystem>
#include <memory>
#include <glm/glm.hpp>
#include "core/flags.hpp"
#include "engine/assets/asset.hpp"

namespace stdfs = std::filesystem;

namespace le::gfx
{
class Texture;

struct Vertex final
{
	glm::vec3 position = {};
	glm::vec3 colour = {};
	glm::vec2 texCoord = {};
};

struct Albedo final
{
	glm::vec3 diffuse = glm::vec3(1.0f);
	glm::vec3 ambient = glm::vec3(1.0f);
	glm::vec3 specular = glm::vec3(1.0f);
	f32 shininess = 32.0f;
};

struct Material final
{
	enum class Flag : u8
	{
		eTextured = 0,
		eLit,
		eCOUNT_
	};

	using Flags = TFlags<Flag>;

	struct Inst final
	{
		Material* pMaterial = nullptr;
		Texture* pDiffuse = nullptr;
		Texture* pSpecular = nullptr;
	};

	Albedo albedo;
	Flags flags;
};

class Mesh final : public Asset
{
public:
	struct Info final
	{
		struct
		{
			Material::Inst material;
			std::vector<Vertex> vertices;
			std::vector<u32> indices;
		} geometry;
	};

public:
	Material::Inst m_material;
	std::unique_ptr<struct MeshImpl> m_uImpl;

public:
	Mesh(stdfs::path id, Info info);
	~Mesh() override;

public:
	Status update() override;
};
} // namespace le::gfx
