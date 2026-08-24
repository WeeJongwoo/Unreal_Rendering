#pragma once

#include "CoreMinimal.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ShaderParameterUtils.h"
#include "RenderGraphResources.h"

BEGIN_SHADER_PARAMETER_STRUCT(FTriangleVSParams, )

END_SHADER_PARAMETER_STRUCT()

struct FColorVertex
{
public:
	FVector2f Position;
	FVector4f Color;
};

class RDGSTUDY_API FTriangleVS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTriangleVS);
	SHADER_USE_PARAMETER_STRUCT(FTriangleVS, FGlobalShader)

	using FParameters = FTriangleVSParams;

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters) {
		return true;
	}
};

BEGIN_SHADER_PARAMETER_STRUCT(FTrianglePSParams, )
	RENDER_TARGET_BINDING_SLOTS()
END_SHADER_PARAMETER_STRUCT()

class FTrianglePS : public FGlobalShader
{
	DECLARE_GLOBAL_SHADER(FTrianglePS);
	using FParameters = FTrianglePSParams;
	SHADER_USE_PARAMETER_STRUCT(FTrianglePS, FGlobalShader);
};

class FTriangleVertexBuffer : public FVertexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI(FRHICommandListBase& RHICmdList) override {
		TResourceArray<FColorVertex, VERTEXBUFFER_ALIGNMENT> Vertices;
		Vertices.SetNumUninitialized(3);
		Vertices[0].Position = FVector2f(0.0f, 0.75f);
		Vertices[0].Color = FVector4f(1, 0, 0, 1);
		Vertices[1].Position = FVector2f(0.75, -0.75);
		Vertices[1].Color = FVector4f(0, 1, 0, 1);
		Vertices[2].Position = FVector2f(-0.75, -0.75);
		Vertices[2].Color = FVector4f(0, 0, 1, 1);
		FRHIResourceCreateInfo CreateInfo(TEXT("FScreenRectangleVertexBuffer"), &Vertices);
		VertexBufferRHI = RHICmdList.CreateVertexBuffer(Vertices.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};

class FTriangleIndexBuffer : public FIndexBuffer
{
public:
	/** Initialize the RHI for this rendering resource */
	void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		const uint16 Indices[] = { 0, 1, 2 };
		TResourceArray<uint16, INDEXBUFFER_ALIGNMENT> IndexBuffer;
		uint32 NumIndices = UE_ARRAY_COUNT(Indices);
		IndexBuffer.AddUninitialized(NumIndices);
		FMemory::Memcpy(IndexBuffer.GetData(), Indices, NumIndices * sizeof(uint16));
		FRHIResourceCreateInfo CreateInfo(TEXT("FTriangleIndexBuffer"), &IndexBuffer);
		IndexBufferRHI = RHICmdList.CreateIndexBuffer(sizeof(uint16), IndexBuffer.GetResourceDataSize(), BUF_Static, CreateInfo);
	}
};

class FTriangleVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;
	/** Destructor. */
	virtual ~FTriangleVertexDeclaration() {}
	virtual void InitRHI(FRHICommandListBase& RHICmdList) override
	{
		FVertexDeclarationElementList Elements;
		uint16 Stride = sizeof(FColorVertex);
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FColorVertex, Position), VET_Float2, 0, Stride));
		Elements.Add(FVertexElement(0, STRUCT_OFFSET(FColorVertex, Color), VET_Float4, 1, Stride));
		VertexDeclarationRHI = PipelineStateCache::GetOrCreateVertexDeclaration(Elements);
	}
	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern RDGSTUDY_API TGlobalResource<FTriangleVertexBuffer> GTriangleVertexBuffer;
extern RDGSTUDY_API TGlobalResource<FTriangleIndexBuffer> GTriangleIndexBuffer;
extern RDGSTUDY_API TGlobalResource<FTriangleVertexDeclaration> GTriangleVertexDeclaration;