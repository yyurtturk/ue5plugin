/*
 * Copyright MediaZ AS. All Rights Reserved.
 */

#pragma once

#include "Engine/TextureRenderTarget2D.h"
#pragma warning (disable : 4800)
#pragma warning (disable : 4668)

void MemoryBarrier();
#include "Windows/AllowWindowsPlatformTypes.h"
#pragma intrinsic(_InterlockedCompareExchange64)
#define InterlockedCompareExchange64 _InterlockedCompareExchange64
#include <d3d12.h>
#include "Windows/HideWindowsPlatformTypes.h"

#include <shared_mutex>
#include "MediaZ/MediaZ.h"

#include "MZActorProperties.h"
#include "MediaZ/AppInterface.h" 
#include <mzFlatBuffersCommon.h>
#include "MZClient.h"
#include "RHI.h"

#define MZ_D3D12_ASSERT_SUCCESS(expr)                                                               \
    {                                                                                               \
        HRESULT re = (expr);                                                                        \
        while (FAILED(re))                                                                          \
        {                                                                                           \
            std::string __err = std::system_category().message(re);                                 \
            char errbuf[1024];                                                                      \
            std::snprintf(errbuf, 1024, "[%lx] %s (%s:%d)", re, __err.c_str(), __FILE__, __LINE__); \
            MZ_ABORT;                                                                           \
        }                                                                                           \
    }

struct ResourceInfo
{
	MZProperty* SrcMzp = 0;
	ID3D12Resource* DstResource = 0;
	ID3D12Fence* Fence = 0;
	u64 FenceValue = 0;
};

enum CmdState
{
	Pending,
	Recording,
	Running,
};

struct CmdStruct
{
	struct ID3D12CommandAllocator* CmdAlloc;
	struct ID3D12GraphicsCommandList* CmdList;
	struct ID3D12Fence* CmdFence;
	uint64_t CmdFenceValue;
	CmdState State;
};



//This class manages copy operations between textures of MediaZ and unreal 2d texture target
class MZSCENETREEMANAGER_API MZTextureShareManager
{
//protected:
public:
	MZTextureShareManager();
	static MZTextureShareManager* singleton;

	static MZTextureShareManager* GetInstance();

	~MZTextureShareManager();
	
	mz::fb::TTexture AddTexturePin(MZProperty*);
	void UpdateTexturePin(MZProperty*, mz::fb::ShowAs, void* data, uint32_t size);
	void UpdatePinShowAs(MZProperty* MzProperty, mz::fb::ShowAs NewShowAs);
	void Reset();
	void WaitCommands();
	void ExecCommands(CmdStruct* cmdData, bool bIsInput, TMap<ID3D12Fence*, u64>& SignalGroup);
	void TextureDestroyed(MZProperty* texture);
	void AllocateCommandLists();
	CmdStruct* GetNewCommandList();
	void ProcessCopies(bool bIsInput, TMap<MZProperty*, ResourceInfo>& CopyMap);
	void OnBeginFrame();
	void OnEndFrame();
	
	class FMZClient* MZClient;
	
	struct ID3D12Device* Dev;
	struct ID3D12CommandQueue* CmdQueue;
	size_t CommandListCount = 10;
	std::vector<CmdStruct*> Cmds;

	TMap<FGuid, MZProperty*> PendingCopyQueue;

	TSet<ID3D12Resource*> ResourcesToDelete;
	
	TMap<MZProperty*, ResourceInfo> CopyOnTick;

	TMap<MZProperty*, ResourceInfo> InputCopies;

	TMap<MZProperty*, ResourceInfo> OutputCopies;
	
	TMap<MZProperty*, ResourceInfo> Copies;

private:
	void Initiate();
};

