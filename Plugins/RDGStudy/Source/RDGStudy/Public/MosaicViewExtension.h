#pragma once

#include "SceneViewExtension.h"
#include "ScreenPass.h"

struct FPostProcessMaterialInputs;

/**
 * Tonemap 이후에 모자이크 패스를 추가한다.
 * r.Mosaic.Enable / Method / BlockSize / UseStencil / StencilValue 로 제어한다.
 */
class RDGSTUDY_API FMosaicViewExtension : public FSceneViewExtensionBase
{
public:
	FMosaicViewExtension(const FAutoRegister& AutoRegister);

	virtual void SetupViewFamily(FSceneViewFamily& InViewFamily) override {}
	virtual void SetupView(FSceneViewFamily& InViewFamily, FSceneView& InView) override {}
	virtual void BeginRenderViewFamily(FSceneViewFamily& InViewFamily) override {}

	virtual void SubscribeToPostProcessingPass(EPostProcessingPass Pass, FAfterPassCallbackDelegateArray& InOutPassCallbacks, bool bIsPassEnabled) override;

protected:
	// r.Mosaic.Enable 0 이면 익스텐션 자체가 비활성화되어 패스 비용이 0이 된다.
	virtual bool IsActiveThisFrame_Internal(const FSceneViewExtensionContext& Context) const override;

private:
	FScreenPassTexture MosaicPass_RenderThread(FRDGBuilder& GraphBuilder, const FSceneView& View, const FPostProcessMaterialInputs& Inputs);
};
