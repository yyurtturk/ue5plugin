#pragma once

#include "Engine/EngineCustomTimeStep.h"

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include "Engine/TextureRenderTarget2D.h"
#include <queue>
#include <map>

#include "mediaz.h"

#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

//#include "DispelUnrealMadnessPrelude.h"
void MemoryBarrier();
#include "Windows/AllowWindowsPlatformTypes.h"
#pragma intrinsic(_InterlockedCompareExchange64)
#define InterlockedCompareExchange64 _InterlockedCompareExchange64
#include <d3d12.h>
#include "AppClient.h"
#include "Windows/HideWindowsPlatformTypes.h"
//#include "DispelUnrealMadnessPostlude.h"

#include "D3D12RHIPrivate.h"
#include "D3D12RHI.h"
#include "D3D12Resources.h"

#include "MZCustomTimeStep.h"

#include <mzFlatBuffersCommon.h>


using MessageBuilder = flatbuffers::grpc::MessageBuilder;

template<class T> requires((u32)mz::app::AppEventUnionTraits<T>::enum_value != 0)
static flatbuffers::grpc::Message<mz::app::AppEvent> MakeAppEvent(MessageBuilder& b, flatbuffers::Offset<T> event)
{
	b.Finish(mz::app::CreateAppEvent(b, mz::app::AppEventUnionTraits<T>::enum_value, event.Union()));
	auto msg = b.ReleaseMessage<mz::app::AppEvent>();
	assert(msg.Verify());
	return msg;
}


/**
 * Implements communication with the MediaZ Engine
 */
class MZCLIENT_API FMZClient : public IModuleInterface {

 public:
	 
	 //Empty constructor
	 FMZClient();

	 //Called on startup of the module on Unreal Engine start
	 virtual void StartupModule() override;

	 //Called on shutdown of the module on Unreal Engine exit
	 virtual void ShutdownModule() override;

	 //This function is called when the connection with the MediaZ Engine is started
	 virtual void Connected();

	 //This function is called when the connection with the MediaZ Engine is finished
	 virtual void Disconnected();
	 
	 /// @return Connection status with MediaZ Engine 
	 virtual bool IsConnected();

	 //This function is called when the Unreal Engine node is removed from the MediaZ engine
	 virtual void NodeRemoved();

	 //Tries to initialize connection with the MediaZ engine
	 void InitConnection();

	 //Sends node updates to the MediaZ
	 void SendNodeUpdate(FGuid nodeId); 
	 
	 //Fills the root graph with first level information (Only the names of the actors without parents) 
	 void PopulateRootGraph();

	 //Tick is called every frame once and handles the tasks queued from grpc threads
	 bool Tick(float dt);

	 //Test action to test wheter debug menu works
	 void TestAction();
	 

protected: 
	 //Carries the actor information of the scene
	 //It is not guaranteed to have all the information at any time
	 tbl<mz::fb::Node> RootGraph;

	 //Grpc client to communicate
	 class ClientImpl* Client = 0;

};



