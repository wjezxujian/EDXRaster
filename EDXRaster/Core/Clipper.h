#pragma once

#include "EDXPrerequisites.h"
#include <ppl.h>

#define CLIP_ALL_PLANES 1

namespace EDX
{
	namespace RasterRenderer
	{
		struct Polygon
		{
			struct Vertex
			{
				Vector4 pos;
				Vector3 clipWeights;

				Vertex(const Vector4& p = Vector4::ZERO, const Vector3& w = Vector3::ZERO)
					: pos(p)
					, clipWeights(w)
				{
				}
			};

			Array<Vertex> vertices;
			void FromTriangle(const Vector4& v0, const Vector4& v1, const Vector4& v2)
			{
				vertices.Resize(3);
				vertices[0].pos = v0; vertices[1].pos = v1; vertices[2].pos = v2;
				vertices[0].clipWeights = Vector3::UNIT_X;
				vertices[1].clipWeights = Vector3::UNIT_Y;
				vertices[2].clipWeights = Vector3::UNIT_Z;
			}
		};

		class Clipper
		{
		private:
			static const uint INSIDE_BIT = 0;
			static const uint LEFT_BIT = 1 << 0;
			static const uint RIGHT_BIT = 1 << 1;
			static const uint BOTTOM_BIT = 1 << 2;
			static const uint TOP_BIT = 1 << 3;
			static const uint FAR_BIT = 1 << 5;
			static const uint NEAR_BIT = 1 << 4;

			static uint ComputeClipCode(const Vector4& v)
			{
				uint code = INSIDE_BIT;

#if CLIP_ALL_PLANES
				if (v.x < -v.w)
					code |= LEFT_BIT;
				if (v.x > v.w)
					code |= RIGHT_BIT;
				if (v.y < -v.w)
					code |= BOTTOM_BIT;
				if (v.y > v.w)
					code |= TOP_BIT;
				if (v.z > v.w)
					code |= FAR_BIT;
#endif
				if (v.z < 0.0f)
					code |= NEAR_BIT;

				return code;
			}

		public:
			static void Clip(Array<ProjectedVertex>& vertexBufferIn,
				const IndexBuffer* pIndexBuf,
				const Array<uint>& texIdBuf,
				Array<ProjectedVertex>* pProjVertices,
				Array<RasterTriangle>* pTrianglesBuf,
				int numCores)
			{
				concurrency::parallel_for(0, numCores, [&](int coreId)
				{
					auto interval = (pIndexBuf->GetTriangleCount() + numCores - 1) / numCores;
					auto startIdx = coreId * interval;
					auto endIdx = (coreId + 1) * interval;

					auto& currentVertexBuf = pProjVertices[coreId];

					for (auto i = startIdx; i < endIdx; i++)
					{
						if (i >= pIndexBuf->GetTriangleCount())
							return;

						const uint* pIndex = pIndexBuf->GetIndex(i);
						const Vector4& v0 = vertexBufferIn[pIndex[0]].projectedPos;
						const Vector4& v1 = vertexBufferIn[pIndex[1]].projectedPos;
						const Vector4& v2 = vertexBufferIn[pIndex[2]].projectedPos;
						const uint texId = texIdBuf[i];
						int idx0 = currentVertexBuf.Size();
						currentVertexBuf.Add(vertexBufferIn[pIndex[0]]);
						int idx1 = currentVertexBuf.Size();
						currentVertexBuf.Add(vertexBufferIn[pIndex[1]]);
						int idx2 = currentVertexBuf.Size();
						currentVertexBuf.Add(vertexBufferIn[pIndex[2]]);

						uint clipCode0 = ComputeClipCode(v0);
						uint clipCode1 = ComputeClipCode(v1);
						uint clipCode2 = ComputeClipCode(v2);

						if ((clipCode0 | clipCode1 | clipCode2))
						{
							if (!(clipCode0 & clipCode1 & clipCode2))
							{
								int clipVertIds[12];

								Polygon polygon0, polygon1;
								polygon0.FromTriangle(v0, v1, v2);

								Polygon* pCurrPoly = &polygon0;
								Polygon* pbuffPoly = &polygon1;
								ClipPolygon(pCurrPoly, pbuffPoly,
									(clipCode0 ^ clipCode1) | (clipCode1 ^ clipCode2) | (clipCode2 ^ clipCode0));

								for (int j = 0; j < pCurrPoly->vertices.Size(); j++)
								{
									Vector3 weight = pCurrPoly->vertices[j].clipWeights;
									if (weight.x == 1.0f)
									{
										clipVertIds[j] = idx0;
									}
									else if (weight.y == 1.0f)
									{
										clipVertIds[j] = idx1;
									}
									else if (weight.z == 1.0f)
									{
										clipVertIds[j] = idx2;
									}
									else
									{
										clipVertIds[j] = currentVertexBuf.Size();
										ProjectedVertex tmpVertex;
										tmpVertex.projectedPos = pCurrPoly->vertices[j].pos;
										tmpVertex.position = weight.x * currentVertexBuf[idx0].position +
											weight.y * currentVertexBuf[idx1].position +
											weight.z * currentVertexBuf[idx2].position;
										tmpVertex.normal = weight.x * currentVertexBuf[idx0].normal +
											weight.y * currentVertexBuf[idx1].normal +
											weight.z * currentVertexBuf[idx2].normal;
										tmpVertex.texCoord = weight.x * currentVertexBuf[idx0].texCoord +
											weight.y * currentVertexBuf[idx1].texCoord +
											weight.z * currentVertexBuf[idx2].texCoord;

										currentVertexBuf.Add(tmpVertex);
									}
								}

								// Simple triangulation
								for (int k = 2; k < pCurrPoly->vertices.Size(); k++)
								{
									uint idx[3] = { clipVertIds[0], clipVertIds[k - 1], clipVertIds[k] };

									RasterTriangle tri;
									if (tri.Setup(currentVertexBuf[clipVertIds[0]].projectedPos.HomogeneousProject(),
										currentVertexBuf[clipVertIds[k - 1]].projectedPos.HomogeneousProject(),
										currentVertexBuf[clipVertIds[k]].projectedPos.HomogeneousProject(),
										idx,
										coreId,
										texId))
									{
										pTrianglesBuf[coreId].Add(tri);
									}
								}
							}

							continue;
						}

						const uint index[3] = { idx0, idx1, idx2 };
						RasterTriangle tri;
						if (tri.Setup(currentVertexBuf[idx0].projectedPos.HomogeneousProject(),
							currentVertexBuf[idx1].projectedPos.HomogeneousProject(),
							currentVertexBuf[idx2].projectedPos.HomogeneousProject(),
							index,
							coreId,
							texId))
						{
							pTrianglesBuf[coreId].Add(tri);
						}
					}
				});
			}

		private:
			template<typename PredicateFunc, typename ComputeTFunc, typename ClipFunc>
			static void ClipByPlane(Polygon*& pInput, Polygon*& pBuffer, PredicateFunc predicate, ComputeTFunc computeT, ClipFunc clip)
			{
				pBuffer->vertices.Clear();
				for (int i = 0; i < pInput->vertices.Size(); i++)
				{
					int i1 = i + 1;
					if (i1 == pInput->vertices.Size()) i1 = 0;
					Vector4 v0 = pInput->vertices[i].pos;
					Vector4 v1 = pInput->vertices[i1].pos;
					if (predicate(v0))
					{
						if (predicate(v1))
						{
							pBuffer->vertices.Add(Polygon::Vertex(v1, pInput->vertices[i1].clipWeights));
						}
						else
						{
							float t = computeT(v0, v1);
							Vector4 pos = v0 * (1 - t) + v1 * t;
							clip(pos);
							Vector3 weight = pInput->vertices[i].clipWeights * (1 - t) + pInput->vertices[i1].clipWeights * t;
							pBuffer->vertices.Add(Polygon::Vertex(pos, weight));
						}
					}
					else
					{
						if (predicate(v1))
						{
							float t = computeT(v0, v1);
							Vector4 pos = v0 * (1 - t) + v1 * t;
							clip(pos);
							Vector3 weight = pInput->vertices[i].clipWeights * (1 - t) + pInput->vertices[i1].clipWeights * t;
							pBuffer->vertices.Add(Polygon::Vertex(pos, weight));
							pBuffer->vertices.Add(Polygon::Vertex(v1, pInput->vertices[i1].clipWeights));
						}
					}
				}

				Swap(pInput, pBuffer);
			}

		public:
			static void ClipPolygon(Polygon*& pInput, Polygon*& pBuffer, const uint planeCode)
			{
#if CLIP_ALL_PLANES
				if (planeCode & LEFT_BIT)
				{
					ClipByPlane(pInput, pBuffer, [](const Vector4& v) -> bool { return v.x >= -v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (v0.w + v0.x) / ((v0.x + v0.w) - (v1.x + v1.w)); },
						[](Vector4& v) { v.x = -v.w; });
				}

				if (planeCode & RIGHT_BIT)
				{
					ClipByPlane(pInput, pBuffer, [](const Vector4& v) -> bool { return v.x <= v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (-v0.w + v0.x) / ((v0.x - v0.w) - (v1.x - v1.w)); },
						[](Vector4& v) { v.x = v.w; });
				}

				if (planeCode & BOTTOM_BIT)
				{
					ClipByPlane(pInput, pBuffer, [](const Vector4& v) -> bool { return v.y >= -v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (v0.w + v0.y) / ((v0.y + v0.w) - (v1.y + v1.w)); },
						[](Vector4& v) { v.y = -v.w; });
				}

				if (planeCode & TOP_BIT)
				{
					ClipByPlane(pInput, pBuffer, [](const Vector4& v) -> bool { return v.y <= v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (-v0.w + v0.y) / ((v0.y - v0.w) - (v1.y - v1.w)); },
						[](Vector4& v) { v.y = v.w; });
				}

				if (planeCode & FAR_BIT)
				{
					ClipByPlane(pInput, pBuffer, [=](const Vector4& v) -> bool { return v.z <= v.w; },
						[](const Vector4& v0, const Vector4& v1) -> float { return (-v0.w + v0.z) / ((v0.z - v0.w) - (v1.z - v1.w)); },
						[=](Vector4& v) { v.z = v.w; });
				}
#endif
				if (planeCode & NEAR_BIT)
				{
					ClipByPlane(pInput, pBuffer, [=](const Vector4& v) -> bool { return v.z >= 0.0f; },
						[](const Vector4& v0, const Vector4& v1) -> float { return v0.z / (v0.z - v1.z); },
						[=](Vector4& v) { v.z = 0.0f; });
				}

				for (int i = 0; i<pInput->vertices.Size(); i++)
				{
					if (pInput->vertices[i].pos.w <= 0.0f)
					{
						pInput->vertices.Clear();
						return;
					}
				}
			}
		};
	}
}