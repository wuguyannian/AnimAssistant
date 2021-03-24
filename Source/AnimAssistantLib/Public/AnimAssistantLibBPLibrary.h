// Copyright (C) 2021 Wuguyannian - All Rights Reserved

#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "AnimAssistantLibBPLibrary.generated.h"

UCLASS()
class UAnimAssistantLibBPLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_UCLASS_BODY()
private:
	static FString DefaultBoneList;
public:

	UFUNCTION(BlueprintCallable)
	static void PrintVirtualBone(class USkeleton* Skeleton);

	UFUNCTION(BlueprintCallable)
	static void RetargetAnim(class USkeleton* Source, USkeleton* Dest, class UAnimBlueprint* AnimBP, const FString FolderPath);

	UFUNCTION(BlueprintCallable)
	static void CreateVirtualBone(USkeleton* Dest, FString JsonBoneList);


};
