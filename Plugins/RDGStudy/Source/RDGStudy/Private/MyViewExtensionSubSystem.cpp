#include "MyViewExtensionSubSystem.h"
#include "SceneViewExtension.h"
#include "MyViewExtension.h"

void UMyViewExtensionSubSystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	UE_LOG(LogTemp, Warning, TEXT("View Extension SubSystem Init"));
	// This is the Pointer to the FSceneViewExenstion you will see later on
	// You need this line to run your shader.
	this->ShaderTest = FSceneViewExtensions::NewExtension<FMyViewExtension>().ToSharedPtr();
}