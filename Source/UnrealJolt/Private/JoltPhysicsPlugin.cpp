// Copyright Epic Games, Inc. All Rights Reserved.

#include "JoltPhysicsPlugin.h"
#include "JoltSettings.h"
#include "Modules/ModuleManager.h"

#if WITH_EDITOR
#include "ISettingsModule.h"
#endif

#define LOCTEXT_NAMESPACE "FUnrealJoltModule "

void FUnrealJoltModule::StartupModule()
{
}

void FUnrealJoltModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealJoltModule, UnrealJolt)
