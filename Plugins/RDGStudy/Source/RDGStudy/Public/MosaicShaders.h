#pragma once

#include "CoreMinimal.h"
#include "DataDrivenShaderPlatformInfo.h"
#include "GlobalShader.h"
#include "ShaderParameterStruct.h"
#include "ScreenPass.h"

namespace Mosaic
{
	enum class EMethod : int32
	{
		Point = 0,		// A: 블록 중심 1-tap
		BruteForce = 1,	// B: 픽셀마다 N*N 평균
		Downsample = 2,	// C: 저해상도 다운샘플 PS + 업스케일
		Compute = 3,	// D: 컴퓨트 리덕션 + 업스케일
		MAX
	};

	constexpr int32 ReduceThreadGroupSize = 8;
}

// 모든 모자이크 쉐이더가 공유하는 입력
BEGIN_SHADER_PARAMETER_STRUCT(FMosaicCommonParameters, )
	SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Input)
	SHADER_PARAMETER_RDG_TEXTURE(Texture2D, InputTexture)
	SHADER_PARAMETER_SAMPLER(SamplerState, InputSampler)
	SHADER_PARAMETER(float, BlockSize)
END_SHADER_PARAMETER_STRUCT()

// MOSAIC_USE_STENCIL 퍼뮤테이션에서만 사용
BEGIN_SHADER_PARAMETER_STRUCT(FMosaicStencilParameters, )
	SHADER_PARAMETER_RDG_TEXTURE_SRV(Texture2D<uint2>, CustomStencilTexture)
	SHADER_PARAMETER(FIntPoint, StencilViewportMin)
	SHADER_PARAMETER(FVector2f, StencilViewportSize)
	SHADER_PARAMETER(uint32, StencilValue)
END_SHADER_PARAMETER_STRUCT()

class FMosaicShaderBase : public FGlobalShader
{
public:
	FMosaicShaderBase() = default;
	FMosaicShaderBase(const ShaderMetaType::CompiledShaderInitializerType& Initializer) : FGlobalShader(Initializer) {}

	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}
};

// A / B
class FMosaicPS : public FMosaicShaderBase
{
public:
	DECLARE_GLOBAL_SHADER(FMosaicPS);
	SHADER_USE_PARAMETER_STRUCT(FMosaicPS, FMosaicShaderBase);

	class FMethodDim : SHADER_PERMUTATION_RANGE_INT("MOSAIC_METHOD", 0, 2);
	class FStencilDim : SHADER_PERMUTATION_BOOL("MOSAIC_USE_STENCIL");
	using FPermutationDomain = TShaderPermutationDomain<FMethodDim, FStencilDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMosaicCommonParameters, Common)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMosaicStencilParameters, Stencil)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

// C : 다운샘플
class FMosaicDownsamplePS : public FMosaicShaderBase
{
public:
	DECLARE_GLOBAL_SHADER(FMosaicDownsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FMosaicDownsamplePS, FMosaicShaderBase);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMosaicCommonParameters, Common)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};

// D : 컴퓨트 리덕션
class FMosaicReduceCS : public FMosaicShaderBase
{
public:
	DECLARE_GLOBAL_SHADER(FMosaicReduceCS);
	SHADER_USE_PARAMETER_STRUCT(FMosaicReduceCS, FMosaicShaderBase);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMosaicCommonParameters, Common)
		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D<float4>, RWLowResTexture)
	END_SHADER_PARAMETER_STRUCT()

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FMosaicShaderBase::ModifyCompilationEnvironment(Parameters, OutEnvironment);
		OutEnvironment.SetDefine(TEXT("THREADGROUP_SIZE"), Mosaic::ReduceThreadGroupSize);
	}
};

// C / D 공용 : 업스케일
class FMosaicUpsamplePS : public FMosaicShaderBase
{
public:
	DECLARE_GLOBAL_SHADER(FMosaicUpsamplePS);
	SHADER_USE_PARAMETER_STRUCT(FMosaicUpsamplePS, FMosaicShaderBase);

	class FStencilDim : SHADER_PERMUTATION_BOOL("MOSAIC_USE_STENCIL");
	using FPermutationDomain = TShaderPermutationDomain<FStencilDim>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_STRUCT_INCLUDE(FMosaicCommonParameters, Common)
		SHADER_PARAMETER_STRUCT_INCLUDE(FMosaicStencilParameters, Stencil)
		SHADER_PARAMETER_STRUCT(FScreenPassTextureViewportParameters, Output)
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D, LowResTexture)
		RENDER_TARGET_BINDING_SLOTS()
	END_SHADER_PARAMETER_STRUCT()
};
