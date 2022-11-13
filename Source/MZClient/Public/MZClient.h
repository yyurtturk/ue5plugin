#pragma once

#if WITH_EDITOR
#include "Engine/EngineCustomTimeStep.h"

#include "CoreMinimal.h"
#include "Misc/MessageDialog.h"
#include <queue>
#include <map>
#include "Containers/Queue.h"
#include "Logging/LogMacros.h"
#include "SceneTree.h"
#include "Subsystems/PlacementSubsystem.h"
//#include "mediaz.h"
//#include "Engine/TextureRenderTarget2D.h"
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)


//void MemoryBarrier();
//#include "Windows/AllowWindowsPlatformTypes.h"
//#pragma intrinsic(_InterlockedCompareExchange64)
//#define InterlockedCompareExchange64 _InterlockedCompareExchange64
//#include <d3d12.h>
//#include "Windows/HideWindowsPlatformTypes.h"

//#include "D3D12RHIPrivate.h"
//#include "D3D12RHI.h"
//#include "D3D12Resources.h"

#include "MZCustomTimeStep.h"
#include "SceneTree.h"

#include "AppClient.h"
#include <mzFlatBuffersCommon.h>



#include <functional> 
typedef std::function<void()> Task;



DECLARE_LOG_CATEGORY_EXTERN(LogMediaZ, Log, All);
/**
 * Implements communication with the MediaZ Engine
 */

class FMZClient;

class MZCLIENT_API ClientImpl : public mz::app::AppClient
{
public:
	using mz::app::AppClient::AppClient;

	virtual void OnAppConnected(mz::app::AppConnectedEvent const& event) override;


	virtual void OnNodeUpdate(mz::NodeUpdated const& archive) override;


	void OnTextureCreated(mz::app::TextureCreated const& texture);


	virtual void Done(grpc::Status const& Status) override;


	virtual void OnNodeRemoved(mz::app::NodeRemovedEvent const& action) override;


	virtual void OnPinValueChanged(mz::PinValueChanged const& action) override;


	virtual void OnPinShowAsChanged(mz::PinShowAsChanged const& action) override;


	virtual void OnFunctionCall(mz::app::FunctionCall const& action) override;


	virtual void OnExecute(mz::app::AppExecute const& aE) override;


	virtual void OnNodeSelected(mz::NodeSelected const& action) override;


	virtual void OnMenuFired(mz::ContextMenuRequest const& request) override;


	virtual void OnCommandFired(mz::ContextMenuAction const& action) override;

	
	FMZClient* PluginClient;
	FGuid nodeId;
	std::atomic_bool shutdown = true;
};



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

	 //Tries to initialize connection with the MediaZ engine
	 void InitConnection();

	 //Sends node updates to the MediaZ
	 void SendNodeUpdate(FGuid nodeId);

	 //Sends the spawnable actor list to MediaZ
	 void SendAssetList();

	 //Sends pin value changed event to MediaZ (now only used for function return values)
	 void SendPinValueChanged(FGuid propertyId, std::vector<uint8> data);

	 //Sends pin updates to the root node 
	 void SendPinUpdate();

	 //Adds the node to scene tree and sends it to mediaZ
	 void SendActorAdded(AActor* actor);
	 
	 //Deletes the node from scene tree and sends it to mediaZ
	 void SendActorDeleted(FGuid id);

	 //Called when pie is started
	 void HandleBeginPIE(bool bIsSimulating);

	 //Called when pie is ending
	 void HandleEndPIE(bool bIsSimulating);

	 //Fills the root graph with first level information (Only the names of the actors without parents) 
	 void PopulateSceneTree();

	 //Fills the specified node information to the root graph
	 bool PopulateNode(FGuid nodeId);

	 //Tick is called every frame once and handles the tasks queued from grpc threads
	 bool Tick(float dt);

	 //Test action to test wheter debug menu works
	 void TestAction();

	 //Set a properties value
	 void SetPropertyValue(FGuid pinId, void* newval, size_t size);
	 
	 //Populate root graph using sceneTree 
	 //void PopulateRootGraphWithSceneTree();

	 //Called when the level is initiated
	 void OnPostWorldInit(UWorld* world, const UWorld::InitializationValues initValues);

	 //Called when an actor is spawned into the world
	 void OnActorSpawned(AActor* InActor);

	 //Called when an actor is destroyed from the world
	 void OnActorDestroyed(AActor* InActor);

	 //Called when the actor is selected on the mediaZ hierarchy pane
	 void OnNodeSelected(FGuid nodeId);

	 //Called when a pin show as change action is fired from mediaZ 
	 //We make that property a pin in the root node with the same GUID
	 void OnPinShowAsChanged(FGuid nodeId, mz::fb::ShowAs newShowAs);

	 //Called when a function is called from mediaZ
	 void OnFunctionCall(FGuid funcId, TMap<FGuid, std::vector<uint8>> properties);

	 //Called when a context menu is fired
	 void OnContexMenuFired(FGuid itemId);

	 //Called when a action from a context menu is fired
	 void OnContexMenuActionFired(FGuid itemId, uint32 actionId);

	 //Sends pin to add to a node
	 void SendPinAdded(FGuid nodeId, MZProperty* mzprop);

	 //Grpc client to communicate
	 class ClientImpl* Client = 0;

	 //Task queue
	 TQueue<Task, EQueueMode::Mpsc> TaskQueue;

	 //Scene tree holds the information to mimic the outliner in mediaz
	 SceneTree sceneTree;

	 //all the properties registered 
	 TMap<FGuid, MZProperty*> RegisteredProperties;
	 
	 //all the functions registered
	 TMap<FGuid, MZFunction*> RegisteredFunctions;
	 
	 //in/out pins of the mediaz node
	 TMap<FGuid, MZProperty*> Pins;

	 //custom functions like spawn actor
	 TMap<FGuid, MZCustomFunction*> CustomFunctions;

	 //Spawnable class list to spawn them from mediaZ
	 TMap<FString, UObject*> SpawnableClasses;
	 TMap<FString, FAssetPlacementInfo> ActorPlacementParamMap;




protected: 

};






#endif


