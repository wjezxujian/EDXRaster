#pragma once

#include "RendererState.h"
#include "Memory/RefPtr.h"

#include "EDXPrerequisites.h"

namespace EDX
{
	namespace RasterRenderer
	{
		class Renderer
		{
		private:
			RendererState mGlobalRenderStates;
			RefPtr<class FrameBuffer> mpFrameBuffer;
			RefPtr<class VertexShader> mpVertexShader;

			RefPtr<class Scene> mpScene;

		public:
			void Initialize(uint iScreenWidth, uint iScreenHeight);
			void SetRenderState(const class Matrix& mModelView, const Matrix& mProj, const Matrix& mToRaster);
			void RenderMesh(const class Mesh& mesh);
		};

	}
}