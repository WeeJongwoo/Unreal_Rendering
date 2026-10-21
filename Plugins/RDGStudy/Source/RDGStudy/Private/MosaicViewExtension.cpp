#include "MosaicViewExtension.h"
#include "MosaicShaders.h"
#include "PostProcess/PostProcessMaterialInputs.h"
#include "RenderGraphUtils.h"
#include "SceneRendering.h"
#include "SceneTexturesConfig.h"

static TAutoConsoleVariable<int32> CVarMosaicEnable(
	TEXT("r.Mosaic.Enable"), 0,
	TEXT("RDG 모자이크 패스 활성화 (0: off, 1: on)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMosaicMethod(
	TEXT("r.Mosaic.Method"), 0,
	TEXT("0: 블록 중심 포인트 샘플\n")
	TEXT("1: 픽셀마다 N*N 평균 (브루트포스)\n")
	TEXT("2: 저해상도 다운샘플 PS + 업스케일\n")
	TEXT("3: 컴퓨트 리덕션 + 업스케일"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMosaicBlockSize(
	TEXT("r.Mosaic.BlockSize"), 16,
	TEXT("블록 크기 (픽셀, 2~128, 짝수로 올림)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMosaicUseStencil(
	TEXT("r.Mosaic.UseStencil"), 0,
	TEXT("1 이면 CustomStencil == r.Mosaic.StencilValue 인 픽셀만 모자이크 (r.CustomDepth=3 필요)"),
	ECVF_RenderThreadSafe);

static TAutoConsoleVariable<int32> CVarMosaicStencilValue(
	TEXT("r.Mosaic.StencilValue"), 1,
	TEXT("모자이크 대상 CustomStencil 값 (0~255)"),
	ECVF_RenderThreadSafe);

DECLARE_GPU_DRAWCALL_STAT(Mosaic);

namespace
{
	const TCHAR* GetMethodName(Mosaic::EMethod Method)
	{
		switch (Method)
		{
		case Mosaic::EMethod::Point:		return TEXT("Point");
		case Mosaic::EMethod::BruteForce:	return TEXT("BruteForce");
		case Mosaic::EMethod::Downsample:	return TEXT("Downsample");
		case Mosaic::EMethod::Compute:		return TEXT("Compute");
		default:							return TEXT("Unknown");
		}
	}
}

FMosaicViewExtension::FMosaicViewExtension(const FAutoRegister& AutoRegister)
	: FSceneViewExtensionBase(AutoRegister)
{
}

bool FMosaicViewExtension::IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const
{
	return CVarMosaicEnable.GetValueOnAnyThread() != 0;
}

void FMosaicViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled)
{
	// 머티리얼 버전(Blendable Location = After Tonemapping)과 같은 위치
	if (Pass == EPostProcessingPass::Tonemap)
	{
		InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FMosaicViewExtension::MosaicPass_RenderThread));
	}
}

FScreenPassTexture FMosaicViewExtension::MosaicPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs)
{
	const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, Inputs.GetInput(EPostProcessMaterialInput::SceneColor));
	check(SceneColor.IsValid());

	const FViewInfo& ViewInfo = static_cast<const FViewInfo&>(View);

	const Mosaic::EMethod Method = (Mosaic::EMethod)FMath::Clamp(CVarMosaicMethod.GetValueOnRenderThread(), 0, (int32)Mosaic::EMethod::MAX - 1);
	// Downsample 의 bilinear 2x2 tap 이 블록 경계를 넘지 않도록 짝수로 맞춘다. 비교를 위해 모든 방식에 동일하게 적용.
	const int32 BlockSize = FMath::Clamp(Align(CVarMosaicBlockSize.GetValueOnRenderThread(), 2), 2, 128);

	// 스텐실 SRV 는 렌더 해상도 ViewRect 기준
	FMosaicStencilParameters StencilParameters;
	bool bUseStencil = false;
	if (CVarMosaicUseStencil.GetValueOnRenderThread() != 0 && Inputs.SceneTextures.SceneTextures)
	{
		const FSceneTextureUniformParameters* SceneTextureParameters = Inputs.SceneTextures.SceneTextures->GetContents();
		if (SceneTextureParameters && SceneTextureParameters->CustomStencilTexture)
		{
			StencilParameters.CustomStencilTexture = SceneTextureParameters->CustomStencilTexture;
			StencilParameters.StencilViewportMin = ViewInfo.ViewRect.Min;
			StencilParameters.StencilViewportSize = FVector2f(ViewInfo.ViewRect.Size());
			StencilParameters.StencilValue = (uint32)FMath::Clamp(CVarMosaicStencilValue.GetValueOnRenderThread(), 0, 255);
			bUseStencil = true;
		}
	}

	RDG_EVENT_SCOPE(GraphBuilder, "Mosaic %s Block=%d%s", GetMethodName(Method), BlockSize, bUseStencil ? TEXT(" Stencil") : TEXT(""));
	RDG_GPU_STAT_SCOPE(GraphBuilder, Mosaic);

	// 입력을 읽으면서 같은 텍스처에 쓸 수 없으므로 출력은 별도 텍스처다.
	FScreenPassRenderTarget Output = Inputs.OverrideOutput;
	if (!Output.IsValid())
	{
		Output = FScreenPassRenderTarget::CreateFromInput(GraphBuilder, SceneColor, ERenderTargetLoadAction::ENoAction, TEXT("Mosaic.Output"));
	}

	const FScreenPassTextureViewport InputViewport(SceneColor);
	const FScreenPassTextureViewport OutputViewport(Output);

	FMosaicCommonParameters CommonParameters;
	CommonParameters.Input = GetScreenPassTextureViewportParameters(InputViewport);
	CommonParameters.InputTexture = SceneColor.Texture;
	CommonParameters.InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
	CommonParameters.BlockSize = (float)BlockSize;

	FGlobalShaderMap* ShaderMap = ViewInfo.ShaderMap;

	if (Method == Mosaic::EMethod::Point || Method == Mosaic::EMethod::BruteForce)
	{
		FMosaicPS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FMosaicPS::FMethodDim>((int32)Method);
		PermutationVector.Set<FMosaicPS::FStencilDim>(bUseStencil);
		TShaderMapRef<FMosaicPS> PixelShader(ShaderMap, PermutationVector);

		FMosaicPS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMosaicPS::FParameters>();
		PassParameters->Common = CommonParameters;
		PassParameters->Stencil = StencilParameters;
		PassParameters->Output = GetScreenPassTextureViewportParameters(OutputViewport);
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Mosaic.%s", GetMethodName(Method)), View, OutputViewport, InputViewport, PixelShader, PassParameters);
		return MoveTemp(Output);
	}

	// C / D : 블록당 1텍셀인 저해상도 텍스처
	const FIntPoint LowResSize = FIntPoint::DivideAndRoundUp(InputViewport.Rect.Size(), BlockSize);
	const ETextureCreateFlags LowResFlags = TexCreate_ShaderResource
		| (Method == Mosaic::EMethod::Downsample ? TexCreate_RenderTargetable : TexCreate_UAV);
	FRDGTextureRef LowResTexture = GraphBuilder.CreateTexture(
		FRDGTextureDesc::Create2D(LowResSize, PF_FloatRGBA, FClearValueBinding::None, LowResFlags),
		TEXT("Mosaic.LowRes"));

	if (Method == Mosaic::EMethod::Downsample)
	{
		const FScreenPassTextureViewport LowResViewport(LowResTexture);
		TShaderMapRef<FMosaicDownsamplePS> PixelShader(ShaderMap);

		FMosaicDownsamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMosaicDownsamplePS::FParameters>();
		PassParameters->Common = CommonParameters;
		PassParameters->RenderTargets[0] = FRenderTargetBinding(LowResTexture, ERenderTargetLoadAction::ENoAction);

		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Mosaic.Downsample %dx%d", LowResSize.X, LowResSize.Y), View, LowResViewport, InputViewport, PixelShader, PassParameters);
	}
	else
	{
		TShaderMapRef<FMosaicReduceCS> ComputeShader(ShaderMap);

		FMosaicReduceCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMosaicReduceCS::FParameters>();
		PassParameters->Common = CommonParameters;
		PassParameters->RWLowResTexture = GraphBuilder.CreateUAV(LowResTexture);

		// 스레드 그룹 하나 = 블록 하나
		FComputeShaderUtils::AddPass(
			GraphBuilder,
			RDG_EVENT_NAME("Mosaic.Reduce %dx%d", LowResSize.X, LowResSize.Y),
			ComputeShader,
			PassParameters,
			FIntVector(LowResSize.X, LowResSize.Y, 1));
	}

	{
		FMosaicUpsamplePS::FPermutationDomain PermutationVector;
		PermutationVector.Set<FMosaicUpsamplePS::FStencilDim>(bUseStencil);
		TShaderMapRef<FMosaicUpsamplePS> PixelShader(ShaderMap, PermutationVector);

		FMosaicUpsamplePS::FParameters* PassParameters = GraphBuilder.AllocParameters<FMosaicUpsamplePS::FParameters>();
		PassParameters->Common = CommonParameters;
		PassParameters->Stencil = StencilParameters;
		PassParameters->Output = GetScreenPassTextureViewportParameters(OutputViewport);
		PassParameters->LowResTexture = LowResTexture;
		PassParameters->RenderTargets[0] = Output.GetRenderTargetBinding();

		AddDrawScreenPass(GraphBuilder, RDG_EVENT_NAME("Mosaic.Upsample"), View, OutputViewport, InputViewport, PixelShader, PassParameters);
	}

	return MoveTemp(Output);
}
