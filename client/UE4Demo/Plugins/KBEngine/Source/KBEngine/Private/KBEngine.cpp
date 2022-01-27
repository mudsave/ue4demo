// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "KBEnginePrivatePCH.h"
#include "KBEngine.h"
#include "KBEngineApp.h"
#include "KBEngineArgs.h"

#define LOCTEXT_NAMESPACE "FKBEngineModule"

void FKBEngineModule::StartupModule()
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module

}

void FKBEngineModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module.  For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE
	
IMPLEMENT_MODULE(FKBEngineModule, KBEngine)