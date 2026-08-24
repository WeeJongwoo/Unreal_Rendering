#include "MyViewExtension.h"
//#include "ViewExtension.h"
#include "TriangleShader.h"
#include "PixelShaderUtils.h"
#include "PostProcess/PostProcessing.h"
#include "PostProcess/PostProcessMaterial.h"
#include "SceneTextureParameters.h"
#include "ShaderParameterStruct.h"

DECLARE_GPU_DRAWCALL_STAT(TrianglePass);
FMyViewExtension::FMyViewExtension(const FAutoRegister& AutoRegister) : FSceneViewExtensionBase(AutoRegister) {

}

void FMyViewExtension::SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray&
InOutPassCallbacks, bool bIsPassEnabled)
{
	if (Pass == EPostProcessingPass::Tonemap)
	{
	// Create Raw Delegate Here, see later on
        InOutPassCallbacks.Add(FAfterPassCallbackDelegate::CreateRaw(this, &FMyViewExtension::TrianglePass_RenderThread));
	}
}

template <typename TShaderClass>
void FMyViewExtension::AddFullscreenPass(
    FRDGBuilder& GraphBuilder,
    const FGlobalShaderMap* GlobalShaderMap,
    FRDGEventName&& PassName,
    const TShaderRef<TShaderClass>& PixelShader,
    typename TShaderClass::FParameters* Parameters,
    const FIntRect& Viewport,
    FRHIBlendState* BlendState,
    FRHIRasterizerState* RasterizerState,
    FRHIDepthStencilState* DepthStencilState,
    uint32 StencilRef)
{
    check(PixelShader.IsValid());
    ClearUnusedGraphResources(PixelShader, Parameters);

    GraphBuilder.AddPass(
        Forward<FRDGEventName>(PassName),
        Parameters,
        ERDGPassFlags::Raster,
        [Parameters, GlobalShaderMap, PixelShader, Viewport, BlendState, RasterizerState, DepthStencilState, StencilRef]
        (FRHICommandList& RHICmdList)
        {
            FMyViewExtension::DrawFullscreenPixelShader<TShaderClass>(
                RHICmdList, GlobalShaderMap, PixelShader, *Parameters, Viewport,
                BlendState, RasterizerState, DepthStencilState, StencilRef);
        });
}

template <typename TShaderClass>
void FMyViewExtension::DrawFullscreenPixelShader(
    FRHICommandList& RHICmdList,
    const FGlobalShaderMap* GlobalShaderMap,
    const TShaderRef<TShaderClass>& PixelShader,
    const typename TShaderClass::FParameters& Parameters,
    const FIntRect& Viewport,
    FRHIBlendState* BlendState,
    FRHIRasterizerState* RasterizerState,
    FRHIDepthStencilState* DepthStencilState,
    uint32 StencilRef)
{
    check(PixelShader.IsValid());

    RHICmdList.SetViewport(
        (float)Viewport.Min.X, (float)Viewport.Min.Y, 0.0f,
        (float)Viewport.Max.X, (float)Viewport.Max.Y, 1.0f);

    // Begin Setup Gpu Pipeline for this Pass
    FGraphicsPipelineStateInitializer GraphicsPSOInit;
    TShaderMapRef<FTriangleVS> VertexShader(GlobalShaderMap);

    RHICmdList.ApplyCachedRenderTargets(GraphicsPSOInit);

    GraphicsPSOInit.BlendState = TStaticBlendState<>::GetRHI();
    GraphicsPSOInit.RasterizerState = TStaticRasterizerState<>::GetRHI();
    GraphicsPSOInit.DepthStencilState = TStaticDepthStencilState<false, CF_Always>::GetRHI();
    GraphicsPSOInit.BoundShaderState.VertexDeclarationRHI = GTriangleVertexDeclaration.VertexDeclarationRHI;
    GraphicsPSOInit.BoundShaderState.VertexShaderRHI = VertexShader.GetVertexShader();
    GraphicsPSOInit.BoundShaderState.PixelShaderRHI = PixelShader.GetPixelShader();
    GraphicsPSOInit.PrimitiveType = PT_TriangleList;

    GraphicsPSOInit.BlendState = BlendState ? BlendState : GraphicsPSOInit.BlendState;
    GraphicsPSOInit.RasterizerState = RasterizerState ? RasterizerState : GraphicsPSOInit.RasterizerState;
    GraphicsPSOInit.DepthStencilState = DepthStencilState ? DepthStencilState : GraphicsPSOInit.DepthStencilState;

    // End Gpu Pipeline setup
    SetGraphicsPipelineState(RHICmdList, GraphicsPSOInit, StencilRef);
    SetShaderParameters(RHICmdList, PixelShader, PixelShader.GetPixelShader(), Parameters);
    DrawFullScreenTriangle(RHICmdList, 1);
}

void FMyViewExtension::RenderTriangle(
    FRDGBuilder& GraphBuilder,
    const FGlobalShaderMap* ViewShaderMap,
    const FIntRect& ViewInfo,
    const FScreenPassTexture& SceneColor)
{
    // Begin Setup
    // Shader Parameter Setup
    FTrianglePSParams* PassParams = GraphBuilder.AllocParameters<FTrianglePSParams>();

    // Set the Render Target In this case is the Scene Color
    PassParams->RenderTargets[0] = FRenderTargetBinding(SceneColor.Texture, ERenderTargetLoadAction::ENoAction);

    // Create FTrianglePS Pixel Shader
    TShaderMapRef<FTrianglePS> PixelShader(ViewShaderMap);

    // Add Pass
    AddFullscreenPass<FTrianglePS>(GraphBuilder,
        ViewShaderMap,
        RDG_EVENT_NAME("TranglePass"),
        PixelShader,
        PassParams,
        ViewInfo);
}

inline void FMyViewExtension::DrawFullScreenTriangle(FRHICommandList& RHICmdList, uint32 InstanceCount)
{
    RHICmdList.SetStreamSource(0, GTriangleVertexBuffer.VertexBufferRHI, 0);
    RHICmdList.DrawIndexedPrimitive(
        GTriangleIndexBuffer.IndexBufferRHI,
        /*BaseVertexIndex=*/ 0,
        /*FirstInstance=*/ 0,
        /*NumVertices=*/ 3,
        /*StartIndex=*/ 0,
        /*NumPrimitives=*/ 1,
        InstanceCount);
}

FScreenPassTexture FMyViewExtension::TrianglePass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& InOutInputs)
{
    const FScreenPassTexture SceneColor = FScreenPassTexture::CopyFromSlice(GraphBuilder, InOutInputs.GetInput(EPostProcessMaterialInput::SceneColor));

    RDG_GPU_STAT_SCOPE(GraphBuilder, TrianglePass)
        RDG_EVENT_SCOPE(GraphBuilder, "TrianglePass");

    // Casting the FSceneView to FViewInfo
    const FIntRect ViewInfo = static_cast<const FViewInfo&>(View).ViewRect;
    const FGlobalShaderMap* ViewShaderMap = static_cast<const FViewInfo&>(View).ShaderMap;

    RenderTriangle(GraphBuilder, ViewShaderMap, ViewInfo, SceneColor);

    return SceneColor;
}