// Copyright MediaZ AS. All Rights Reserved.

#include "MZActorProperties.h"
#include "MZTextureShareManager.h"
#include "EditorCategoryUtils.h"
#include "ObjectEditorUtils.h"
#include "MZTrack.h"
#include "EngineUtils.h"
#include "Blueprint/UserWidget.h"
#include "MZSceneTreeManager.h"

#define CHECK_PROP_SIZE() {if (size != Property->ElementSize){UE_LOG(LogMZSceneTreeManager, Error, TEXT("Property size mismatch with mediaZ"));return;}}

bool PropertyVisibleExp(const FProperty* ueproperty)
{
	return !ueproperty->HasAllPropertyFlags(CPF_DisableEditOnInstance) &&
		!ueproperty->HasAllPropertyFlags(CPF_Deprecated) &&
		//!ueproperty->HasAllPropertyFlags(CPF_EditorOnly) && //? dont know what this flag does but it hides more than necessary
		ueproperty->HasAllPropertyFlags(CPF_Edit) &&
		//ueproperty->HasAllPropertyFlags(CPF_BlueprintVisible) && //? dont know what this flag does but it hides more than necessary
		ueproperty->HasAllFlags(RF_Public);
}

MZProperty::MZProperty(MZObjectReference* ObjectReference, FProperty* uproperty, FString parentCategory, uint8* structPtr, const MZStructProperty* parentProperty)
{
	UObject *container = nullptr;
	Property = uproperty;

	if(ObjectReference && ObjectReference->GetAsObject())
	{
		container = ObjectReference->GetAsObject();
	}
	
	if (Property->HasAnyPropertyFlags(CPF_OutParm))
	{
		ReadOnly = true;
	}

	StructPtr = structPtr;
	if (container && container->IsA<UActorComponent>())
	{
		ComponentContainer = static_cast<MZComponentReference *>(ObjectReference);
	}
	else if (container && container->IsA<AActor>())
	{
		ActorContainer = static_cast<MZActorReference *>(ObjectReference);
	}
	else
	{
		ObjectPtr = container;
	}

	
	Id = FGuid::NewGuid();
	PropertyName = uproperty->GetFName().ToString();
	PropertyNameAsReference = uproperty->GetFName();
	if (container && container->IsA<UActorComponent>())
	{
		PropertyName = *FString(container->GetFName().ToString() + "" + PropertyName);
	}

	auto metaDataMap = uproperty->GetMetaDataMap();
	if (metaDataMap)
	{
		static const FName NAME_DisplayName(TEXT("DisplayName"));
		static const FName NAME_Category(TEXT("Category"));
		static const FName NAME_UIMin(TEXT("UIMin"));
		static const FName NAME_UIMax(TEXT("UIMax"));

		const auto& metaData = *metaDataMap;
		DisplayName = metaData.Contains(NAME_DisplayName) ? metaData[NAME_DisplayName] : uproperty->GetFName().ToString();
		CategoryName = (metaData.Contains(NAME_Category) ? metaData[NAME_Category] : "Default");
		UIMinString = metaData.Contains(NAME_UIMin) ? metaData[NAME_UIMin] : "";
		UIMaxString = metaData.Contains(NAME_UIMax) ? metaData[NAME_UIMax] : "";
	}
	else
	{
		DisplayName = uproperty->GetFName().ToString();
		CategoryName = "Default";
		UIMinString = "";
		UIMaxString = "";
	}
	IsAdvanced = uproperty->HasAllPropertyFlags(CPF_AdvancedDisplay);
	
	// For properties inside a struct, add them to their own category unless they just take the name of the parent struct.  
	// In that case push them to the parent category
	FName PropertyCategoryName = FObjectEditorUtils::GetCategoryFName(Property);
	if (parentProperty && (PropertyCategoryName == parentProperty->GetStructProperty()->Struct->GetFName()))
	{
		CategoryName = parentCategory;
	}
	else
	{
		if (!parentCategory.IsEmpty())
		{
			if (CategoryName.IsEmpty())
			{
				CategoryName = parentCategory;
			}
			else
			{
				CategoryName = (parentCategory + "|" + CategoryName);
			}
		}
	}

	if (parentProperty)
	{
		DisplayName = parentProperty->DisplayName + "_" + DisplayName;
	}

}



std::vector<uint8> MZProperty::UpdatePinValue(uint8* customContainer)
{
	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	if (container)
	{
		void* val = Property->ContainerPtrToValuePtr<void>(container);
		memcpy(data.data(), val, data.size());
	}
	return data;
}

void MZProperty::MarkState()
{
	if (ComponentContainer)
	{
		ComponentContainer->Get()->MarkRenderStateDirty();
		ComponentContainer->Get()->UpdateComponentToWorld();
	}

}
void MZProperty::UpdatePropertyReference(FProperty *NewProperty)
{
	this->Property = NewProperty;
}
void MZProperty::RemovePortal()
{
	
}
void MZProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	IsChanged = true;
	CHECK_PROP_SIZE();

	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	SetProperty_InCont(container, val);
	
	if (!customContainer && container)
	{
		MarkState();
	}

}
void* MZProperty::GetRawContainer()
{
	if(auto Object = GetRawObjectContainer())
	{
		return Object;
	}
	return StructPtr;
}

UObject* MZProperty::GetRawObjectContainer()
{
	if (ActorContainer)
	{
		return ActorContainer->Get();
	}
	else if (ComponentContainer)
	{
		return ComponentContainer->Get();
	}
	else if (ObjectPtr && IsValid(ObjectPtr))
	{
		return ObjectPtr;
	}

	return nullptr;
}

void MZProperty::SetProperty_InCont(void* container, void* val) { return; }

void MZTrackProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	IsChanged = true;

	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	SetProperty_InCont(container, val);

	if (!customContainer && container)
	{
		MarkState();
	}

}

FStructProperty* MZTrackProperty::GetStructProperty() const
{
	return static_cast<FStructProperty*>(Property);
}
	
void MZTrackProperty::SetProperty_InCont(void* container, void* val)
{
	auto track = flatbuffers::GetRoot<mz::fb::Track>(val);
	FMZTrack* TrackData = GetStructProperty()->ContainerPtrToValuePtr<FMZTrack>(container);
	// TrackData.location = FVector(0);
	// TrackData.rotation = FVector(0);
	// TrackData.center_shift = FVector2d(0);
	// TrackData.sensor_size = FVector2d(0);

	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_LOCATION))
	{
		TrackData->location = FVector(track->location()->x(), track->location()->y(), track->location()->z());
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_ROTATION))
	{
		TrackData->rotation = FVector(track->rotation()->x(), track->rotation()->y(), track->rotation()->z());
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_FOV))
	{
		TrackData->fov = track->fov();
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_FOCUS))
	{
		TrackData->focus_distance = track->focus_distance();
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_CENTER_SHIFT))
	{
		TrackData->center_shift = FVector2D(track->center_shift()->x(), track->center_shift()->y());
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_ZOOM))
	{
		TrackData->zoom = track->zoom();
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_K1K2))
	{
		TrackData->k1 = track->k1k2()->x();
		TrackData->k2 = track->k1k2()->y();
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_RENDER_RATIO))
	{
		TrackData->render_ratio = track->render_ratio();
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_DISTORTION_SCALE))
	{
		TrackData->distortion_scale = track->distortion_scale();
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_SENSOR_SIZE))
	{
		TrackData->sensor_size = FVector2D(track->sensor_size()->x(), track->sensor_size()->y());
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_PIXEL_ASPECT_RATIO))
	{
		TrackData->pixel_aspect_ratio = track->pixel_aspect_ratio();
	}
	if (flatbuffers::IsFieldPresent(track, mz::fb::Track::VT_NODAL_OFFSET))
	{
		TrackData->nodal_offset = track->nodal_offset();
	}
	
	//structprop->CopyCompleteValue(structprop->ContainerPtrToValuePtr<void>(container), &TrackData); 
	//FMZTrack newTrack = *(FMZTrack*)val;

	//if (ActorContainer.Get())
	//{
	//	//ActorContainer->SetActorRelativeLocation(newTrack.location);
	//	//actor->SetActorRelativeRotation(newTrack.rotation.Rotation());
	//	//actor->SetActorRelativeRotation(newTrack.rotation.Rotation());
	//}
}


std::vector<uint8> MZTrackProperty::UpdatePinValue(uint8* customContainer)
{
	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	if (container)
	{
		FMZTrack TrackData = *Property->ContainerPtrToValuePtr<FMZTrack>(container);
		
		flatbuffers::FlatBufferBuilder fb;
		mz::fb::TTrack TempTrack;
		TempTrack.location = mz::fb::vec3d(TrackData.location.X, TrackData.location.Y, TrackData.location.Z);
		TempTrack.rotation = mz::fb::vec3d(TrackData.rotation.X, TrackData.rotation.Y, TrackData.rotation.Z);
		TempTrack.fov = TrackData.fov;
		TempTrack.focus = TrackData.focus_distance;
		TempTrack.center_shift = mz::fb::vec2d(TrackData.center_shift.X, TrackData.center_shift.Y);
		TempTrack.zoom = TrackData.zoom;
		TempTrack.k1k2 = mz::fb::vec2d(TrackData.k1, TrackData.k2);
		TempTrack.render_ratio = TrackData.render_ratio;
		TempTrack.distortion_scale = TrackData.distortion_scale;
		TempTrack.sensor_size = mz::fb::vec2d(TrackData.sensor_size.X, TrackData.sensor_size.Y);
		TempTrack.pixel_aspect_ratio = TrackData.pixel_aspect_ratio;
		TempTrack.nodal_offset = TrackData.nodal_offset;
		
		auto offset = mz::fb::CreateTrack(fb, &TempTrack);
		fb.Finish(offset);
		mz::Buffer buffer = fb.Release();
		data = buffer;
	}
	return data;
}

FStructProperty* MZRotatorProperty::GetStructProperty() const
{
	return static_cast<FStructProperty*>(Property);
}

void MZRotatorProperty::SetProperty_InCont(void* container, void* val)
{
	double x = ((double*)val)[0];
	double y = ((double*)val)[1];
	double z = ((double*)val)[2];
	FRotator rotator = FRotator(y,z,x);
	GetStructProperty()->CopyCompleteValue(GetStructProperty()->ContainerPtrToValuePtr<void>(container), &rotator);
}

std::vector<uint8> MZRotatorProperty::UpdatePinValue(uint8* customContainer)
{
	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	if (container)
	{
		void* val = Property->ContainerPtrToValuePtr<void>(container);
		double x = ((double*)val)[0];
		double y = ((double*)val)[1];
		double z = ((double*)val)[2];
		FRotator rotator = FRotator(z, x, y);


		memcpy(data.data(), &rotator, data.size());
	}
	return data;
}

flatbuffers::Offset<mz::fb::Pin> MZProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{

	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata = SerializeMetaData(fbb);

	if (TypeName == "mz.fb.Void" || TypeName.size() < 1)
	{
		return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*DisplayName), "mz.fb.Void", mz::fb::ShowAs::NONE, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, &default_val, 0, ReadOnly, IsAdvanced, transient, &metadata, 0, mz::fb::PinContents::JobPin);
	}
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&Id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(),  PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, &min_val, &max_val, &default_val, 0, ReadOnly, IsAdvanced, transient, &metadata, 0, mz::fb::PinContents::JobPin);
}

std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> MZProperty::SerializeMetaData(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata;
	for (auto [key, value] : mzMetaDataMap)
	{
		metadata.push_back(mz::fb::CreateMetaDataEntryDirect(fbb, TCHAR_TO_UTF8(*key), TCHAR_TO_UTF8(*value)));
	}
	return metadata;
}

MZStructProperty::MZStructProperty(MZObjectReference* ObjectReference, FStructProperty* uproperty, FString parentCategory, uint8* StructPtr, MZStructProperty* parentProperty)
	: MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty)
{
	uint8* StructInst = nullptr;
	UClass* Class = nullptr;
	UObject *container = nullptr;

	FStructProperty *structprop = GetStructProperty();

	if(ObjectReference && ObjectReference->GetAsObject())
	{
		container = ObjectReference->GetAsObject();
	}
	
	if (UObject* Container = GetRawObjectContainer())
	{
		StructInst = structprop->ContainerPtrToValuePtr<uint8>(Container);
		Class = Container->GetClass();
	}
	else if (StructPtr)
	{
		StructInst = structprop->ContainerPtrToValuePtr<uint8>(StructPtr);
		Class = structprop->Struct->GetClass();
	}
	class FProperty* AProperty = structprop->Struct->PropertyLink;
	while (AProperty != nullptr)
	{
		FName CategoryNamek = FObjectEditorUtils::GetCategoryFName(AProperty);

		if (Class && FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CategoryNamek.ToString()) || !PropertyVisibleExp(AProperty))
		{
			AProperty = AProperty->PropertyLinkNext;
			continue;
		}

		UE_LOG(LogTemp, Warning, TEXT("The property name in struct: %s"), *(AProperty->GetAuthoredName()));
		auto mzprop = MZPropertyFactory::CreateProperty(nullptr, AProperty, nullptr, nullptr, CategoryName + "|" + DisplayName, StructInst, this);
		if (mzprop)
		{
			if(mzprop->mzMetaDataMap.Contains("ContainerPath"))
			{
				auto ContainerPath = mzprop->mzMetaDataMap.Find("ContainerPath");
				ContainerPath->InsertAt(0, structprop->GetNameCPP() + FString("/") );
			}
			else
			{
				mzprop->mzMetaDataMap.Add("ContainerPath", structprop->GetNameCPP());	
			}
			
			mzprop->mzMetaDataMap.Remove("component");
			mzprop->mzMetaDataMap.Remove("actorId");
			if (auto component = Cast<USceneComponent>(container))
			{
				mzprop->mzMetaDataMap.Add("component", component->GetFName().ToString());
				if (auto actor = component->GetOwner())
				{
					mzprop->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
				}
			}
			else if (auto actor = Cast<AActor>(container))
			{
				mzprop->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
			}

			childProperties.push_back(mzprop);
			
			for (auto it : mzprop->childProperties)
			{
				if(it->mzMetaDataMap.Contains("ContainerPath"))
				{
					auto ContainerPath = it->mzMetaDataMap.Find("ContainerPath");
					ContainerPath->InsertAt(0, structprop->GetNameCPP() + FString("/") );
				}
				else
				{
					it->mzMetaDataMap.Add("ContainerPath", structprop->GetNameCPP());	
				}
				it->mzMetaDataMap.Remove("component");
				it->mzMetaDataMap.Remove("actorId");
				if (auto component = Cast<USceneComponent>(container))
				{
					it->mzMetaDataMap.Add("component", component->GetFName().ToString());
					if (auto actor = component->GetOwner())
					{
						it->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
					}
				}
				else if (auto actor = Cast<AActor>(container))
				{
					it->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
				}
				childProperties.push_back(it);
			}
		}

		AProperty = AProperty->PropertyLinkNext;
	}

	data = std::vector<uint8_t>(1, 0);
	TypeName = "mz.fb.Void";
}

FStructProperty* MZStructProperty::GetStructProperty() const
{
	return static_cast<FStructProperty*>(Property);
}

void MZStructProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	//empty
}

bool PropertyVisible(FProperty* ueproperty);

MZObjectProperty::MZObjectProperty(MZObjectReference* ObjectReference, FObjectProperty* uproperty, FString parentCategory, uint8* StructPtr, MZStructProperty* parentProperty)
	: MZProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty)
{
	UObject *container = nullptr;

	if(ObjectReference)
	{
		container = ObjectReference->GetAsObject();
	}
	
	if (GetObjectProperty()->PropertyClass->IsChildOf<UTextureRenderTarget2D>()) // We only support texturetarget2d from object properties
	{
		TypeName = "mz.fb.Texture"; 
		auto tex = MZTextureShareManager::GetInstance()->AddTexturePin(this);
		data = mz::Buffer::From(tex);
	}
	else if (GetObjectProperty()->PropertyClass->IsChildOf<UUserWidget>())
	{
		UObject* Container = ActorContainer->Get();
		if (!Container)
		{
			Container = ComponentContainer->Get();
		}
		auto Widget = Cast<UObject>(GetObjectProperty()->GetObjectPropertyValue(GetObjectProperty()->ContainerPtrToValuePtr<UUserWidget>(Container)));
		if(!Widget)
		{
			data = std::vector<uint8_t>(1, 0);
			TypeName = "mz.fb.Void";
			return;
		}
	
		auto WidgetClass = Widget->GetClass();

		
		FProperty* WProperty = WidgetClass->PropertyLink;
		parentCategory = parentCategory + "|" + Widget->GetFName().ToString();
		MZObjectReference *OnlyWidgetReference = nullptr;
		
		while (WProperty != nullptr)
		{
			FName CCategoryName = FObjectEditorUtils::GetCategoryFName(WProperty);

			UClass* Class = WidgetClass;

			if(!OnlyWidgetReference)
			{
				OnlyWidgetReference = new MZObjectReference(Widget);
			}

			if (FEditorCategoryUtils::IsCategoryHiddenFromClass(Class, CCategoryName.ToString()) || !PropertyVisible(WProperty))
			{
				WProperty = WProperty->PropertyLinkNext;
				continue;
			}
			TSharedPtr<MZProperty> mzprop = MZPropertyFactory::CreateProperty(OnlyWidgetReference, WProperty, 0, 0, parentCategory); // ToDo check for memory concer

			if(mzprop->mzMetaDataMap.Contains("ContainerPath"))
			{
				auto propPath = mzprop->mzMetaDataMap.Find("ContainerPath");
				propPath->InsertAt(0, GetObjectProperty()->GetFName().ToString() + FString("/") );
			}
			else
			{
				mzprop->mzMetaDataMap.Add("ContainerPath", GetObjectProperty()->GetFName().ToString());
			}
			
			
			mzprop->mzMetaDataMap.Remove("component");
			mzprop->mzMetaDataMap.Remove("actorId");
			if (auto component = Cast<USceneComponent>(container))
			{
				mzprop->mzMetaDataMap.Add("component", component->GetFName().ToString());
				if (auto actor = component->GetOwner())
				{
					mzprop->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
				}
			}
			else if (auto actor = Cast<AActor>(container))
			{
				mzprop->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
			}

			
			if (!mzprop)
			{
				WProperty = WProperty->PropertyLinkNext;
				continue;
			}
			//RegisteredProperties.Add(mzprop->Id, mzprop);
			childProperties.push_back(mzprop);

			for (auto It : mzprop->childProperties)
			{
				
				if(It->mzMetaDataMap.Contains("ContainerPath"))
				{
					auto propPath = It->mzMetaDataMap.Find("ContainerPath");
					propPath->InsertAt(0, GetObjectProperty()->GetFName().ToString() + FString("/") );
				}
				else
				{
					It->mzMetaDataMap.Add("ContainerPath", GetObjectProperty()->GetFName().ToString());
				}
				//RegisteredProperties.Add(it->Id, it);
				It->mzMetaDataMap.Remove("component");
				It->mzMetaDataMap.Remove("actorId");
				if (auto component = Cast<USceneComponent>(container))
				{
					It->mzMetaDataMap.Add("component", component->GetFName().ToString());
					if (auto actor = component->GetOwner())
					{
						It->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
					}
				}
				else if (auto actor = Cast<AActor>(container))
				{
					It->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
				}
				childProperties.push_back(It);
			}

			WProperty = WProperty->PropertyLinkNext;
		}

		data = std::vector<uint8_t>(1, 0);
		TypeName = "mz.fb.Void";
	}
	else
	{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "mz.fb.Void";
	}
}

FObjectProperty* MZObjectProperty::GetObjectProperty() const
{
	return static_cast<FObjectProperty*>(Property);
}
void MZObjectProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
}

FStrProperty* MZStringProperty::GetStringProperty() const
{
	return static_cast<FStrProperty*>(Property);
}
void MZStringProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	IsChanged = true;

	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	if (container)
	{
		FString newval((char*)val);
		GetStringProperty()->SetPropertyValue_InContainer(container, newval);
	}
	if (!customContainer && container)
	{
		MarkState();
	}
	return;
}

std::vector<uint8> MZStringProperty::UpdatePinValue(uint8* customContainer)
{
	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	FString val(" ");
	if (container)
	{
		val = GetStringProperty()->GetPropertyValue_InContainer(container);
	}
	auto s = StringCast<ANSICHAR>(*val);
	data = std::vector<uint8_t>(s.Length() + 1, 0);
	memcpy(data.data(), s.Get(), s.Length());

	return data;
}
FNameProperty* MZNameProperty::GetNameProperty() const
{
	return static_cast<FNameProperty*>(Property);
}
void MZNameProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	IsChanged = true;

	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	if (container)
	{
		FString newval((char*)val);
		GetNameProperty()->SetPropertyValue_InContainer(container, FName(newval));
	}
	if (!customContainer && container)
	{
		MarkState();
	}
	return;
}

std::vector<uint8> MZNameProperty::UpdatePinValue(uint8* customContainer)
{
	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	FString val(" ");
	if (container)
	{
		val = GetNameProperty()->GetPropertyValue_InContainer(container).ToString();
	}
	auto s = StringCast<ANSICHAR>(*val);
	data = std::vector<uint8_t>(s.Length() + 1, 0);
	memcpy(data.data(), s.Get(), s.Length());
	
	return data;
}
FTextProperty* MZTextProperty::GetTextProperty() const
{
	return static_cast<FTextProperty*>(Property);
}
void MZTextProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	IsChanged = true;

	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	if (container)
	{
		FString newval((char*)val);
		GetTextProperty()->SetPropertyValue_InContainer(container, FText::FromString(newval));
	}

	if (!customContainer && container)
	{
		MarkState();
	}

	return;
}

std::vector<uint8> MZTextProperty::UpdatePinValue(uint8* customContainer)
{
	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	FString val(" ");
	if (container)
	{
		val = GetTextProperty()->GetPropertyValue_InContainer(container).ToString();
	}

	auto s = StringCast<ANSICHAR>(*val);
	data = std::vector<uint8_t>(s.Length() + 1, 0);
	memcpy(data.data(), s.Get(), s.Length());

	return data;
}
flatbuffers::Offset<mz::fb::Visualizer> MZEnumProperty::SerializeVisualizer(flatbuffers::FlatBufferBuilder& fbb)
{
	return mz::fb::CreateVisualizerDirect(fbb, mz::fb::VisualizerType::COMBO_BOX, TCHAR_TO_UTF8(*GetEnum()->GetFName().ToString()));
}

flatbuffers::Offset<mz::fb::Pin> MZEnumProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	std::vector<flatbuffers::Offset<mz::fb::MetaDataEntry>> metadata = SerializeMetaData(fbb);
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&(MZProperty::Id), TCHAR_TO_UTF8(*DisplayName), TCHAR_TO_ANSI(TEXT("string")), PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), SerializeVisualizer(fbb), &data, 0, 0, 0, 0, 0, ReadOnly, IsAdvanced, transient, &metadata, 0,  mz::fb::PinContents::JobPin);
}

void MZEnumProperty::SetPropValue(void* val, size_t size, uint8* customContainer)
{
	//TODO
	
	IsChanged = true;

	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	if (container)
	{
		FString newval((char*)val);
		FNumericProperty *PropertyAsNumeric = GetNumericProperty();
		
		auto NewVal = GetEnum()->GetValueByName(FName(newval));
		if(NewVal == INDEX_NONE)
		{
			return;
		}
		
		uint8* PropData = PropertyAsNumeric->ContainerPtrToValuePtr<uint8>(container);
		PropertyAsNumeric->SetIntPropertyValue(PropData, NewVal);
	}

	if (!customContainer && container)
	{
		MarkState();
	}

	return;
}

std::vector<uint8> MZEnumProperty::UpdatePinValue(uint8* customContainer)
{
	void* container = nullptr;
	if (customContainer) container = customContainer;
	else if (ComponentContainer) container = ComponentContainer->Get();
	else if (ActorContainer) container = ActorContainer->Get();
	else if (ObjectPtr && IsValid(ObjectPtr)) container = ObjectPtr;
	else if (StructPtr) container = StructPtr;

	FString val(" ");
	const FNumericProperty *IndexProp = GetNumericProperty();
	const UEnum* Enum = GetEnum();
	if(container && Enum && IndexProp)
	{
		CurrentValue = IndexProp->GetSignedIntPropertyValue_InContainer(container);
		CurrentName = Enum->GetNameByValue(CurrentValue).ToString();
		val = CurrentName;
	}
	
	auto s = StringCast<ANSICHAR>(*val);
	data = std::vector<uint8_t>(s.Length() + 1, 0);
	memcpy(data.data(), s.Get(), s.Length());

	return data;
}

TFunction<void(MZObjectReference* ObjectReference, TSharedPtr<MZProperty> MzProperty)> MZPropertyFactory::OnPropertyCreatedCallback ;

TSharedPtr<MZProperty> MZPropertyFactory::CreateProperty(MZObjectReference* ObjectReference,
                                                         FProperty* uproperty, 
                                                         TMap<FGuid, TSharedPtr<MZProperty>>* registeredProperties, 
                                                         TMap<FProperty*, TSharedPtr<MZProperty>>* propertiesMap,
                                                         FString parentCategory, 
                                                         uint8* StructPtr, 
                                                         MZStructProperty* parentProperty)
{
	TSharedPtr<MZProperty> prop = nullptr;

	UObject *container = nullptr;
	//CAST THE PROPERTY ACCORDINGLY
	uproperty->GetClass();
	if((CastField<FNumericProperty>(uproperty) && CastField<FNumericProperty>(uproperty)->IsEnum()) 
			 || CastField<FEnumProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZEnumProperty(ObjectReference, uproperty, parentCategory, StructPtr, parentProperty));
	}
	else if (FFloatProperty* floatprop = CastField<FFloatProperty>(uproperty) ) 
	{
		prop = TSharedPtr<MZProperty>(new MZFloatProperty(ObjectReference, floatprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FDoubleProperty* doubleprop = CastField<FDoubleProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZDoubleProperty(ObjectReference, doubleprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FInt8Property* int8prop = CastField<FInt8Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZInt8Property(ObjectReference, int8prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FInt16Property* int16prop = CastField<FInt16Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZInt16Property(ObjectReference, int16prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FIntProperty* intprop = CastField<FIntProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZIntProperty(ObjectReference, intprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FInt64Property* int64prop = CastField<FInt64Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZInt64Property(ObjectReference, int64prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FByteProperty* byteprop = CastField<FByteProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZByteProperty(ObjectReference, byteprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FUInt16Property* uint16prop = CastField<FUInt16Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZUInt16Property(ObjectReference, uint16prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FUInt32Property* uint32prop = CastField<FUInt32Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZUInt32Property(ObjectReference, uint32prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FUInt64Property* uint64prop = CastField<FUInt64Property>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZUInt64Property(ObjectReference, uint64prop, parentCategory, StructPtr, parentProperty));
	}
	else if (FBoolProperty* boolprop = CastField<FBoolProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZBoolProperty(ObjectReference, boolprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FTextProperty* textprop = CastField<FTextProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZTextProperty(ObjectReference, textprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FNameProperty* nameprop = CastField<FNameProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZNameProperty(ObjectReference, nameprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FStrProperty* stringProp = CastField<FStrProperty>(uproperty))
	{
		prop = TSharedPtr<MZProperty>(new MZStringProperty(ObjectReference, stringProp, parentCategory, StructPtr, parentProperty));
	}
	else if (FObjectProperty* objectprop = CastField<FObjectProperty>(uproperty))
	{
		if (!ObjectReference || !ObjectReference->GetAsObject()) // TODO: Handle inside MZObjectProperty
		{
			return nullptr;
		}
		prop = TSharedPtr<MZProperty>(new MZObjectProperty(ObjectReference, objectprop, parentCategory, StructPtr, parentProperty));
	}
	else if (FStructProperty* structprop = CastField<FStructProperty>(uproperty))
	{
		
		//TODO ADD SUPPORT FOR FTRANSFORM
		if (structprop->Struct == TBaseStructure<FVector2D>::Get()) //vec2
		{
			prop = TSharedPtr<MZProperty>(new MZVec2Property(ObjectReference, structprop, parentCategory, StructPtr, parentProperty));
		}
		else if (structprop->Struct == TBaseStructure<FVector>::Get()) //vec3
		{
			prop = TSharedPtr<MZProperty>(new MZVec3Property(ObjectReference, structprop, parentCategory, StructPtr, parentProperty));
		}
		else if (structprop->Struct == TBaseStructure<FRotator>::Get())
		{
			prop = TSharedPtr<MZProperty>(new MZRotatorProperty(ObjectReference, structprop, parentCategory, StructPtr, parentProperty));
			FVector min(0, 0, 0);
			FVector max(359.999, 359.999, 359.999);
			prop->min_val = prop->data;
			prop->max_val = prop->data;
			memcpy(prop->min_val.data(), &min, sizeof(FVector));
			memcpy(prop->max_val.data(), &max, sizeof(FVector));
		}
		else if (structprop->Struct == TBaseStructure<FVector4>::Get() || structprop->Struct == TBaseStructure<FQuat>::Get()) //vec4
		{
			prop = TSharedPtr<MZProperty>(new MZVec4Property(ObjectReference, structprop, parentCategory, StructPtr, parentProperty));

		}
		else if (structprop->Struct == FMZTrack::StaticStruct()) //track
		{
			prop = TSharedPtr<MZProperty>(new MZTrackProperty(ObjectReference, structprop, parentCategory, StructPtr, parentProperty));
		}
		else //auto construct
		{
			prop = TSharedPtr<MZProperty>(new MZStructProperty(ObjectReference, structprop, parentCategory, StructPtr, parentProperty));
		}
	}

	if (!prop)
	{
		return nullptr; //for properties that we do not support
	}

	prop->UpdatePinValue();
	prop->default_val = prop->data;
	if (registeredProperties)
	{
		registeredProperties->Add(prop->Id, prop);
		for (auto& it : prop->childProperties)
		{
			registeredProperties->Add(it->Id, it);
		}
	}
	if (propertiesMap)
	{
		propertiesMap->Add(prop->Property, prop);
		for (auto& it : prop->childProperties)
		{
			propertiesMap->Add(it->Property, it);
		}
	}
	if (prop->TypeName == "mz.fb.Void")
	{
		prop->data.clear();
		prop->default_val.clear();
	}
#if 0 //default properties from objects
	if (prop->TypeName == "mz.fb.Void")
	{
		prop->data.clear();
		prop->default_val.clear();
	}
	else if (auto actor = Cast<AActor>(container))
	{
		if (prop->TypeName != "bool")
		{
			auto defobj = actor->GetClass()->GetDefaultObject();
			if (defobj)
			{
				auto val = uproperty->ContainerPtrToValuePtr<void>(defobj);
				if (prop->default_val.size() != uproperty->GetSize())
				{
					prop->default_val = std::vector<uint8>(uproperty->GetSize(), 0);	
				}
				memcpy(prop->default_val.data(), val, uproperty->GetSize());
			}

		}
		else
		{
			auto defobj = actor->GetClass()->GetDefaultObject();
			if (defobj)
			{
				auto val = !!( *uproperty->ContainerPtrToValuePtr<bool>(defobj) );
				if (prop->default_val.size() != uproperty->GetSize())
				{
					prop->default_val = std::vector<uint8>(uproperty->GetSize(), 0);
				}
				memcpy(prop->default_val.data(), &val, uproperty->GetSize());
			}

		}
			//uproperty->ContainerPtrToValuePtrForDefaults()
	}
	else if (auto sceneComponent = Cast<USceneComponent>(container))
	{
		if (prop->TypeName != "bool")
		{
			auto defobj = sceneComponent->GetClass()->GetDefaultObject();
			if (defobj)
			{
				auto val = uproperty->ContainerPtrToValuePtr<void>(defobj);
				if (prop->default_val.size() != uproperty->GetSize())
				{
					prop->default_val = std::vector<uint8>(uproperty->GetSize(), 0);
				}
				memcpy(prop->default_val.data(), val, uproperty->GetSize());
			}
		}
		else
		{
			auto defobj = sceneComponent->GetClass()->GetDefaultObject();
			if (defobj)
			{
				auto val = !!( *uproperty->ContainerPtrToValuePtr<bool>(defobj) );

				if (prop->default_val.size() != uproperty->GetSize())
				{
					prop->default_val = std::vector<uint8>(uproperty->GetSize(), 0);
				}
				memcpy(prop->default_val.data(), &val, uproperty->GetSize());
			}

		}
		//uproperty->ContainerPtrToValuePtrForDefaults()
	}
#endif

	//update metadata
	// prop->mzMetaDataMap.Add("property", uproperty->GetFName().ToString());

	if(ObjectReference)
	{
		container = ObjectReference->GetAsObject();
	}
	prop->mzMetaDataMap.Add("PropertyPath", uproperty->GetPathName());

	auto actor = Cast<AActor>(container);
	auto component = Cast<USceneComponent>(container);
	
	if (component)
	{
		prop->mzMetaDataMap.Add("component", component->GetFName().ToString());
		if (auto OwnerActor = component->GetOwner())
		{
			prop->mzMetaDataMap.Add("actorId", OwnerActor->GetActorGuid().ToString());
		}
	}
	else if (actor)
	{
		prop->mzMetaDataMap.Add("actorId", actor->GetActorGuid().ToString());
	}
	
	FProperty* tryprop = FindFProperty<FProperty>(*uproperty->GetPathName());
	UE_LOG(LogMZSceneTreeManager, Warning, TEXT("name of the prop before %s, found property name %s"),*uproperty->GetFName().ToString(),  *tryprop->GetFName().ToString());

	if(MZPropertyFactory::OnPropertyCreatedCallback)
	{
		if (actor)
		{
			MZPropertyFactory::OnPropertyCreatedCallback(ObjectReference, prop);
		}else if(component)
		{
			MZPropertyFactory::OnPropertyCreatedCallback(ObjectReference, prop);
		}
	}

	return prop;
}

MZObjectReference::MZObjectReference(TObjectPtr<UObject> Object)
{
	ObjectRef = Object;
}
void MZObjectReference::AddProperty(FName PropertyName, TSharedPtr<class MZProperty>MzProperty)
{
	PropertiesMap.Add(PropertyName, MzProperty);
}
UObject *MZObjectReference::GetAsObject() const
{
	return ObjectRef.Get(true);
}
bool MZObjectReference::UpdateObjectPointer(UObject *Object)
{
	ObjectRef = Object;
	return true;
}

MZActorReference::MZActorReference(TObjectPtr<AActor> actor) : MZObjectReference(actor)
{
	if (actor)
	{
		ActorGuid = actor->GetActorGuid();
		ActorClass = actor->GetClass();		// This is a snapshot, should be taken when Actor is assigned
	}
}

MZActorReference::MZActorReference()
{

}

AActor* MZActorReference::Get()
{
	if(ObjectRef.IsValid())
	{
		return Cast<AActor>(ObjectRef.Get());
	}
	else if (UpdateActualActorPointer())
	{
		return Cast<AActor>(ObjectRef.Get());
	}
	return nullptr;
}

UClass* MZActorReference::GetActorClass() const
{
	return ActorClass;
}
bool MZActorReference::UpdateClass(UClass *NewActorClass)
{
	this->ActorClass = NewActorClass;
	// ToDo more checks before assignment

	return true;
}

void MZActorReference::UpdateActorReference(AActor *NewActor)
{
	UpdateObjectPointer(NewActor);
	UpdateClass(NewActor->GetClass());
}
bool MZActorReference::UpdateActorPointer(const UWorld* World)
{
	if (!ActorGuid.IsValid())
	{
		InvalidReference = true;
		return false;
	}

	TMap<FGuid, AActor*> sceneActorMap;
	if (IsValid(World))
	{
		for (TActorIterator<AActor> ActorItr(World); ActorItr; ++ActorItr)
		{
			if (ActorItr->GetActorGuid() == ActorGuid)
			{
				ObjectRef = TWeakObjectPtr<AActor>(*ActorItr);
				return true;
			}
		}
	}
	InvalidReference = true;
	return false;
}

bool MZActorReference::UpdateActualActorPointer()
{
	UWorld* World;
	if (FMZSceneTreeManager::daWorld)
	{
		World = FMZSceneTreeManager::daWorld;
	}
	else
	{
		World = GEngine->GetWorldContextFromGameViewport(GEngine->GameViewport)->World();
	}
	return UpdateActorPointer(World);
}


MZComponentReference::MZComponentReference(TObjectPtr<UActorComponent> actorComponent)
: MZObjectReference(actorComponent)
{
	if (actorComponent)
	{
		const TWeakObjectPtr<UActorComponent> Component = TWeakObjectPtr<UActorComponent>(actorComponent);
		ObjectRef = Component;
		ComponentProperty = Component->GetFName();
		PathToComponent = Component->GetPathName(GetOwnerActor());
	}
}

MZComponentReference::MZComponentReference()
{

}

UActorComponent* MZComponentReference::Get()
{
	if (ObjectRef.IsValid())
	{
		return Cast<UActorComponent>(ObjectRef.Get());
	}
	else if (UpdateActualComponentPointer())
	{
		return Cast<UActorComponent>(ObjectRef.Get());
	}

	return nullptr;

}

AActor* MZComponentReference::GetOwnerActor()
{
	return Get()->GetOwner();
}

bool MZComponentReference::UpdateActualComponentPointer()
{
	AActor *Actor = Cast<UActorComponent>(ObjectRef.Get())->GetOwner();
	
	if (!Actor  || IsValid(Actor) || PathToComponent.IsEmpty())
	{
		InvalidReference = true;
		return false;
	}

	auto comp = FindObject<UActorComponent>(Actor, *PathToComponent);
	if (comp)
	{
		ObjectRef = TWeakObjectPtr<UActorComponent>(comp);
		return true;
	}

	InvalidReference = true;
	return false;
}


