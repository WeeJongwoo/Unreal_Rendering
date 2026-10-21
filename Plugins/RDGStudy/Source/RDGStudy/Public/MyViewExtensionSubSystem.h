#pragma once

#include "CoreMinimal.h"
#include "Subsystems/EngineSubsystem.h"
#include "MyViewExtensionSubSystem.generated.h"

class FMyViewExtension;
class FMosaicViewExtension;
/**
*
*/
UCLASS()
class UMyViewExtensionSubSystem: public UEngineSubsystem
{
	GENERATED_BODY()
protected:
	// Declaration of the Pointer delegate Unreal's FViewExenstion object gives us
	TSharedPtr<FMyViewExtension,ESPMode::ThreadSafe> ShaderTest;
	TSharedPtr<FMosaicViewExtension, ESPMode::ThreadSafe> MosaicExtension;
public:
	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
};

