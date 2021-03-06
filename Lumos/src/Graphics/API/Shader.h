#pragma once

#include "lmpch.h"

#include "ShaderUniform.h"
#include "ShaderResource.h"

#define SHADER_VERTEX_INDEX		0
#define SHADER_UV_INDEX			1
#define SHADER_MASK_UV_INDEX	2
#define SHADER_TID_INDEX		3
#define SHADER_MID_INDEX		4
#define SHADER_COLOR_INDEX		5

#define SHADER_UNIFORM_PROJECTION_MATRIX_NAME	"sys_ProjectionMatrix"
#define SHADER_UNIFORM_VIEW_MATRIX_NAME			"sys_ViewMatrix"
#define SHADER_UNIFORM_MODEL_MATRIX_NAME		"sys_ModelMatrix"
#define SHADER_UNIFORM_TEXTURE_MATRIX_NAME		"sys_TextureMatrix"

namespace Lumos
{
	namespace Graphics
	{
		enum class ShaderType : int
		{
			VERTEX,
            FRAGMENT,
			GEOMETRY,
			TESSELLATION_CONTROL,
			TESSELLATION_EVALUATION,
			COMPUTE,
            UNKNOWN
		};

		struct ShaderEnumClassHash
		{
			template <typename T>
			std::size_t operator()(T t) const
			{
				return static_cast<std::size_t>(t);
			}
		};

		template <typename Key>
		using HashType = typename std::conditional<std::is_enum<Key>::value, ShaderEnumClassHash, std::hash<Key>>::type;

		template <typename Key, typename T>
		using UnorderedMap = std::unordered_map<Key, T, HashType<Key>>;

		class LUMOS_EXPORT Shader
		{
		public:
			static const Shader* s_CurrentlyBound;
		public:
			virtual void Bind() const = 0;
			virtual void Unbind() const = 0;

			virtual ~Shader() = default;

			virtual void SetSystemUniformBuffer(ShaderType type, u8* data, u32 size, u32 slot = 0) = 0;
			virtual void SetUserUniformBuffer(ShaderType type, u8* data, u32 size) = 0;

			virtual const ShaderUniformBufferList GetSystemUniforms(ShaderType type) const = 0;
			virtual const ShaderUniformBufferDeclaration* GetUserUniformBuffer(ShaderType type) const = 0;

			virtual const std::vector<ShaderType> GetShaderTypes() const = 0;

			virtual const String& GetName() const = 0;
			virtual const String& GetFilePath() const = 0;

		public:
			static Shader* CreateFromFile(const String& name, const String& filepath);
			static bool TryCompile(const String& source, String& error, const String& name);
			static bool TryCompileFromFile(const String& filepath, String& error);
            
        protected:
            static Shader* (*CreateFunc)(const String&, const String&);
		};
	}
}
