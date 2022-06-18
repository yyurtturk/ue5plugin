#pragma once

#include "Modules/ModuleInterface.h"
#include "CoreMinimal.h"
#include "MZType.h"

class IMZRemoteControl : public IModuleInterface 
{
public:
    static inline IMZRemoteControl* Get() {
        static const FName ModuleName = "MZRemoteControl";
        if (IsInGameThread()) {
            return FModuleManager::LoadModulePtr<IMZRemoteControl>(ModuleName);
        }
        else {
            return FModuleManager::GetModulePtr<IMZRemoteControl>(ModuleName);
        }
    }
	virtual TMap<FGuid, MZEntity> const& GetExposedEntities() = 0;
};

