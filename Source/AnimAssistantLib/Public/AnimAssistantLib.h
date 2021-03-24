// Copyright (C) 2021 Wuguyannian - All Rights Reserved

#pragma once

#include "Modules/ModuleManager.h"

class FAnimAssistantLibModule : public IModuleInterface
{
public:

	/** IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
