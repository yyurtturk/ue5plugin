#include "MZActorProperties.h"
#include "UObject/UnrealType.h"
#include "UObject/UnrealTypePrivate.h"

MZProperty::MZProperty(UObject* container, FProperty* uproperty)
{
	Property = uproperty;
	Container = container;
	id = FGuid::NewGuid();
	PropertyName = uproperty->GetFName().ToString();
	if (Container->IsA<UActorComponent>())
	{
		PropertyName = *FString(Container->GetFName().ToString() + "" + PropertyName);
	}
	
	auto metaDataMap = uproperty->GetMetaDataMap();
	if (!metaDataMap)
	{
		UE_LOG(LogTemp, Warning, TEXT("no_metadata_is_found"));
		return;
	}

	static const FName NAME_DisplayName(TEXT("DisplayName"));
	static const FName NAME_Category(TEXT("Category"));
	static const FName NAME_UIMin(TEXT("UIMin"));
	static const FName NAME_UIMax(TEXT("UIMax"));

	const auto& metaData = *metaDataMap;
	DisplayName = metaData.Contains(NAME_DisplayName) ? metaData[NAME_DisplayName] : uproperty->GetFName().ToString();
	CategoryName = metaData.Contains(NAME_Category) ? metaData[NAME_Category] : "Default";
	UIMinString = metaData.Contains(NAME_UIMin) ? metaData[NAME_UIMin] : "";
	UIMaxString = metaData.Contains(NAME_UIMax) ? metaData[NAME_UIMax] : "";

	IsAdvanced = uproperty->HasAllPropertyFlags(CPF_AdvancedDisplay);

	

	if (uproperty->IsA(FFloatProperty::StaticClass())){ //todo data
		data = std::vector<uint8_t>(4, 0);
		TypeName = "f32";
	}
	else if (uproperty->IsA(FDoubleProperty::StaticClass())){ //todo data
		data = std::vector<uint8_t>(8, 0);
		TypeName = "f64";
	}
	else if (uproperty->IsA(FInt8Property::StaticClass())) { //todo data
		data = std::vector<uint8_t>(1, 0);
		TypeName = "i8";
	}
	else if (uproperty->IsA(FInt16Property::StaticClass())) { //todo data
		data = std::vector<uint8_t>(2, 0);
		TypeName = "i16";
	}
	else if (uproperty->IsA(FIntProperty::StaticClass())) { //todo data
		data = std::vector<uint8_t>(4, 0);
		TypeName = "i32";
	}
	else if (uproperty->IsA(FInt64Property::StaticClass())){ //todo data
		data = std::vector<uint8_t>(8, 0);
		TypeName = "i64";
	}
	else if (uproperty->IsA(FByteProperty::StaticClass())) { //todo data
		data = std::vector<uint8_t>(1, 0);
		TypeName = "u8";
	}
	else if (uproperty->IsA(FUInt16Property::StaticClass())) { //todo data
		data = std::vector<uint8_t>(2, 0);
		TypeName = "u16";
	}
	else if (uproperty->IsA(FUInt32Property::StaticClass())) { //todo data
		data = std::vector<uint8_t>(4, 0);
		TypeName = "u32";
	}
	else if (uproperty->IsA(FUInt64Property::StaticClass())) { //todo data
		data = std::vector<uint8_t>(8, 0);
		TypeName = "u64";
	}
	else if (uproperty->IsA(FBoolProperty::StaticClass())){ //todo data
		data = std::vector<uint8_t>(1, 0);
		TypeName = "bool";
	}
	else if (uproperty->IsA(FEnumProperty::StaticClass())) { //todo data
		// how to iterate enums__??
		TypeName = "string";
	}
	else if (uproperty->IsA(FTextProperty::StaticClass())) { //todo data
		FString val = Property->ContainerPtrToValuePtr<FText>(container)->ToString();
		auto s = StringCast<ANSICHAR>(*val);
		data = std::vector<uint8_t>(s.Length() + 1, 0);
		memcpy(data.data(), s.Get(), s.Length());

		TypeName = "string";
	}
	else if (uproperty->IsA(FNameProperty::StaticClass())) { //todo data
		FString val = Property->ContainerPtrToValuePtr<FName>(container)->ToString();
		auto s = StringCast<ANSICHAR>(*val);
		data = std::vector<uint8_t>(s.Length() + 1, 0);
		memcpy(data.data(), s.Get(), s.Length());

		TypeName = "string";
	}
	//else if (uproperty->IsA(FArrayProperty::StaticClass())) { //Not supported by mediaz
	//	data = std::vector<uint8_t>(1, 0);
	//	TypeName = "bool";
	//}
	else if (uproperty->IsA(FStructProperty::StaticClass())) {
		FStructProperty* StructProp = Cast<FStructProperty>(uproperty);

		if (StructProp->Struct == TBaseStructure<FVector>::Get() || StructProp->Struct == TBaseStructure<FRotator>::Get()) //todo data
		{
			data = std::vector<uint8_t>(sizeof(FVector), 0);
			TypeName = "mz.fb.vec3d";
		}
		else if (StructProp->Struct == TBaseStructure<FVector4>::Get() || StructProp->Struct == TBaseStructure<FQuat>::Get()) //todo data
		{
			data = std::vector<uint8_t>(sizeof(FVector4), 0);
			TypeName = "mz.fb.vec4d";
		}
		else if (StructProp->Struct == TBaseStructure<FVector2D>::Get()) //todo data
		{
			data = std::vector<uint8_t>(sizeof(FVector2D), 0);
			TypeName = "mz.fb.vec2d";
		}
		else if (StructProp->Struct == TBaseStructure<FTransform>::Get()) //todo everything
		{
			//TODO
		}
		else
		{
			data = std::vector<uint8_t>(1, 0);
			TypeName = "mz.fb.Void";
		}
	}
	else{
		data = std::vector<uint8_t>(1, 0);
		TypeName = "mz.fb.Void";
	}

	if (TypeName != "mz.fb.Void" && TypeName != "string")
	{
		void* val = Property->ContainerPtrToValuePtr< void >(container);
		memcpy(data.data(), val, data.size());
	}

}

void MZProperty::SetValue(void* newval, size_t size) //called from game thread*
{
	if (TypeName != "mz.fb.Void" && TypeName != "string")
	{
		void* val = Property->ContainerPtrToValuePtr< void >(Container);
		memcpy(val, newval, size);
	}

	if (UActorComponent* Component = Cast<UActorComponent>(Container))
	{
		Component->MarkRenderStateDirty();
		Component->UpdateComponentToWorld();
	}
}

flatbuffers::Offset<mz::fb::Pin> MZProperty::Serialize(flatbuffers::FlatBufferBuilder& fbb)
{
	return mz::fb::CreatePinDirect(fbb, (mz::fb::UUID*)&id, TCHAR_TO_UTF8(*DisplayName), TypeName.c_str(),  PinShowAs, mz::fb::CanShowAs::INPUT_OUTPUT_PROPERTY, TCHAR_TO_UTF8(*CategoryName), 0, &data, 0, 0, 0, 0, 0, IsAdvanced);
}
