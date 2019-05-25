#pragma once
#include "LM.h"
#include "Texture.h"

namespace lumos
{
	namespace graphics
	{
		class LUMOS_EXPORT TextureDepth : public Texture
		{
		public:
			static TextureDepth* Create(uint width, uint height);

			virtual void Resize(uint width, uint height) = 0;
		};
	}
}