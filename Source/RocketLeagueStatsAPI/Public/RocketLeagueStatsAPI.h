// Copyright (c) 2026 Sumatras Studios. All Rights Reserved.

#pragma once

#include "Modules/ModuleManager.h"

class FRocketLeagueStatsAPIModule : public IModuleInterface
{
public:
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};
