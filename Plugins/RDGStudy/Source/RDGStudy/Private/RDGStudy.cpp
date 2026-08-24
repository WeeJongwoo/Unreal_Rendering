// Copyright Epic Games, Inc. All Rights Reserved.

#include "RDGStudy.h"
#include "Interfaces/IPluginManager.h"

#define LOCTEXT_NAMESPACE "FRDGStudyModule"

void FRDGStudyModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
	FString BaseDir = IPluginManager::Get().FindPlugin(TEXT("RDGStudy"))->GetBaseDir();
	FString PluginShaderDir = FPaths::Combine(BaseDir, TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/CustomShaders"), PluginShaderDir);
}

void FRDGStudyModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FRDGStudyModule, RDGStudy)