// Copyright (C) 2021 Wuguyannian - All Rights Reserved

#include "AnimAssistantLibBPLibrary.h"
#include "AnimAssistantLib.h"
#include "Animation/Skeleton.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Animation/AnimationAsset.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimBlueprintGeneratedClass.h"
#include "EditorAnimUtils.h"
#include "ObjectEditorUtils.h"
#include "AssetToolsModule.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"

#include "Framework/Notifications/NotificationManager.h"

#include "ObjectEditorUtils.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "IContentBrowserSingleton.h"
#include "ContentBrowserModule.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Editor.h"


#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#define LOCTEXT_NAMESPACE "AnimAssistantLib"

DEFINE_LOG_CATEGORY_STATIC(LogAnimAssistantLib, Log, All);

UAnimAssistantLibBPLibrary::UAnimAssistantLibBPLibrary(const FObjectInitializer& ObjectInitializer)
: Super(ObjectInitializer)
{

}

void UAnimAssistantLibBPLibrary::PrintVirtualBone(class USkeleton* Skeleton)
{
	if (Skeleton)
	{
		const int32 NumVirtualBones = Skeleton->GetVirtualBones().Num();
		if (NumVirtualBones > 0)
		{
			for (int32 VBIndex = 0; VBIndex < NumVirtualBones; ++VBIndex)
			{
				const FVirtualBone& Bone = Skeleton->GetVirtualBones()[VBIndex];
				UE_LOG(LogAnimAssistantLib, Display, TEXT("Virtual Bone [%s] SourceBone [%s] TargetBone [%s]"), 
					*Bone.VirtualBoneName.ToString(), *Bone.SourceBoneName.ToString(), *Bone.TargetBoneName.ToString());
			}
		}
		else
		{
			UE_LOG(LogAnimAssistantLib, Display, TEXT("Skeleton [%s] Has no Virtual Bone"), *Skeleton->GetName());
		}
	}
}

/** Helper archive class to find all references, used by the cycle finder **/
class FFindAnimAssetRefs : public FArchiveUObject
{
public:
	/**
	* Constructor
	*
	* @param	Src		the object to serialize which may contain a references
	*/
	FFindAnimAssetRefs(UObject* Src, TArray<UAnimationAsset*>& OutAnimationAssets) : AnimationAssets(OutAnimationAssets)
	{
		// use the optimized RefLink to skip over properties which don't contain object references
		ArIsObjectReferenceCollector = true;

		ArIgnoreArchetypeRef = false;
		ArIgnoreOuterRef = true;
		ArIgnoreClassRef = false;

		Src->Serialize(*this);
	}

	virtual FString GetArchiveName() const { return TEXT("FFindAnimAssetRefs"); }

private:
	/** Serialize a reference **/
	FArchive& operator<<(class UObject*& Obj)
	{
		if (UAnimationAsset* Anim = Cast<UAnimationAsset>(Obj))
		{
			AnimationAssets.AddUnique(Anim);
		}
		return *this;
	}

	TArray<UAnimationAsset*>& AnimationAssets;
};

void GetAllAnimationSequencesReferredInBlueprint(UAnimBlueprint* AnimBlueprint, TArray<UAnimationAsset*>& AnimationAssets)
{
	UObject* DefaultObject = AnimBlueprint->GetAnimBlueprintGeneratedClass()->GetDefaultObject();
	FFindAnimAssetRefs AnimRefFinderObject(DefaultObject, AnimationAssets);

	// For assets referenced in the event graph (either pin default values or variable-get nodes)
	// we need to serialize the nodes in that graph
	for (UEdGraph* GraphPage : AnimBlueprint->UbergraphPages)
	{
		for (UEdGraphNode* Node : GraphPage->Nodes)
		{
			FFindAnimAssetRefs AnimRefFinderBlueprint(Node, AnimationAssets);
		}
	}

	// Gather references in functions
	for (UEdGraph* GraphPage : AnimBlueprint->FunctionGraphs)
	{
		for (UEdGraphNode* Node : GraphPage->Nodes)
		{
			FFindAnimAssetRefs AnimRefFinderBlueprint(Node, AnimationAssets);
		}
	}
}



//////////////////////////////////////////////////////////////////
// FAnimationRetargetContext
struct FNameDuplicationRule
{
	FString Prefix;
	FString Suffix;
	// replace string
	FString ReplaceFrom;
	FString ReplaceTo;
	// folder path
	FString FolderPath;

	FNameDuplicationRule()
		: Prefix(), Suffix(), ReplaceFrom(), ReplaceTo(), FolderPath(TEXT("/Game"))
	{}

	bool ShouldRename()
	{
		// should be able to replace "XXX" to ""
		return (!Prefix.IsEmpty() || !Suffix.IsEmpty() || !ReplaceFrom.IsEmpty());
	}

	FString Rename(const UObject* Asset) const;
};

FString FNameDuplicationRule::Rename(const UObject* Asset) const
{
	check(Asset);

	FString NewName = Asset->GetName();

	NewName = NewName.Replace(*ReplaceFrom, *ReplaceTo);
	return FString::Printf(TEXT("%s%s%s"), *Prefix, *NewName, *Suffix);
}

class FAnimationRetargetContext
{
public:
	FAnimationRetargetContext(const TArray<FAssetData>& AssetsToRetarget, bool bRetargetReferredAssets, bool bInConvertAnimationDataInComponentSpaces, const FNameDuplicationRule& NameRule = FNameDuplicationRule());
	FAnimationRetargetContext(TArray<TWeakObjectPtr<UObject>> AssetsToRetarget, bool bRetargetReferredAssets, bool bInConvertAnimationDataInComponentSpaces, const FNameDuplicationRule& NameRule = FNameDuplicationRule());

	/** Were we supplied anything that we can retarget */
	bool HasAssetsToRetarget() const;

	/** Did we duplicate any assets */
	bool HasDuplicates() const;

	/** Returns the UObject that was chosen to retarget if there was only one in the first place */
	UObject* GetSingleTargetObject() const;

	/** Returns the duplicate of the supplied object if there is one, otherwise NULL */
	UObject* GetDuplicate(const UObject* OriginalObject) const;

	/** return all duplicates */
	TArray<UObject*> GetAllDuplicates() const;

	/** Duplicates the assets stored for retargetting, populating maps of original assets to new asset */
	void DuplicateAssetsToRetarget(UPackage* DestinationPackage, const FNameDuplicationRule* NameRule);

	/** Retarget the contained assets */
	void RetargetAnimations(USkeleton* OldSkeleton, USkeleton* NewSkeleton);

	void AddRemappedAsset(UAnimationAsset* OriginalAsset, UAnimationAsset* NewAsset);

private:
	/** Lists of assets to retarget. Populated from FAssetData supplied to constructor */
	TArray<UAnimationAsset*>	AnimationAssetsToRetarget;
	TArray<UAnimBlueprint*>		AnimBlueprintsToRetarget;

	/** Lists of original assets map to duplicate assets */
	TMap<UAnimationAsset*, UAnimationAsset*>	DuplicatedAnimAssets;
	TMap<UAnimBlueprint*, UAnimBlueprint*>		DuplicatedBlueprints;

	TMap<UAnimationAsset*, UAnimationAsset*>	RemappedAnimAssets;

	/** If we only chose one object to retarget store it here */
	UObject* SingleTargetObject;

	/** whether to convert animation data in component spaces */
	bool bConvertAnimationDataInComponentSpaces;

	/** Initialize the object, only to be called by constructors */
	void Initialize(TArray<TWeakObjectPtr<UObject>> AssetsToRetarget, bool bRetargetReferredAssets);
};

FAnimationRetargetContext::FAnimationRetargetContext(const TArray<FAssetData>& AssetsToRetarget, bool bRetargetReferredAssets, bool bInConvertAnimationDataInComponentSpaces, const FNameDuplicationRule& NameRule)
	: SingleTargetObject(NULL)
	, bConvertAnimationDataInComponentSpaces(bInConvertAnimationDataInComponentSpaces)
{
	TArray<UObject*> Objects;
	for (auto Iter = AssetsToRetarget.CreateConstIterator(); Iter; ++Iter)
	{
		Objects.Add((*Iter).GetAsset());
	}
	auto WeakObjectList = FObjectEditorUtils::GetTypedWeakObjectPtrs<UObject>(Objects);
	Initialize(WeakObjectList, bRetargetReferredAssets);
}

FAnimationRetargetContext::FAnimationRetargetContext(TArray<TWeakObjectPtr<UObject>> AssetsToRetarget, bool bRetargetReferredAssets, bool bInConvertAnimationDataInComponentSpaces, const FNameDuplicationRule& NameRule)
	: SingleTargetObject(NULL)
	, bConvertAnimationDataInComponentSpaces(bInConvertAnimationDataInComponentSpaces)
{
	Initialize(AssetsToRetarget, bRetargetReferredAssets);
}

void FAnimationRetargetContext::Initialize(TArray<TWeakObjectPtr<UObject>> AssetsToRetarget, bool bRetargetReferredAssets)
{
	for (auto Iter = AssetsToRetarget.CreateConstIterator(); Iter; ++Iter)
	{
		UObject* Asset = (*Iter).Get();
		if (UAnimationAsset* AnimAsset = Cast<UAnimationAsset>(Asset))
		{
			AnimationAssetsToRetarget.AddUnique(AnimAsset);
		}
		else if (UAnimBlueprint* AnimBlueprint = Cast<UAnimBlueprint>(Asset))
		{

			// Add parents blueprint. 
			UAnimBlueprint* ParentBP = Cast<UAnimBlueprint>(AnimBlueprint->ParentClass->ClassGeneratedBy);
			while (ParentBP)
			{
				AnimBlueprintsToRetarget.AddUnique(ParentBP);
				ParentBP = Cast<UAnimBlueprint>(ParentBP->ParentClass->ClassGeneratedBy);
			}

			AnimBlueprintsToRetarget.AddUnique(AnimBlueprint);
		}
	}

	if (AssetsToRetarget.Num() == 1)
	{
		//Only chose one object to retarget, keep track of it
		SingleTargetObject = AssetsToRetarget[0].Get();
	}

	if (bRetargetReferredAssets)
	{
		// Grab assets from the blueprint. Do this first as it can add complex assets to the retarget array
		// which will need to be processed next.
		for (auto Iter = AnimBlueprintsToRetarget.CreateConstIterator(); Iter; ++Iter)
		{
			GetAllAnimationSequencesReferredInBlueprint((*Iter), AnimationAssetsToRetarget);
		}

		int32 AssetIndex = 0;
		while (AssetIndex < AnimationAssetsToRetarget.Num())
		{
			UAnimationAsset* AnimAsset = AnimationAssetsToRetarget[AssetIndex++];
			AnimAsset->HandleAnimReferenceCollection(AnimationAssetsToRetarget, true);
		}
	}
}

bool FAnimationRetargetContext::HasAssetsToRetarget() const
{
	return	AnimationAssetsToRetarget.Num() > 0 ||
		AnimBlueprintsToRetarget.Num() > 0;
}

bool FAnimationRetargetContext::HasDuplicates() const
{
	return	DuplicatedAnimAssets.Num() > 0 ||
		DuplicatedBlueprints.Num() > 0;
}

TArray<UObject*> FAnimationRetargetContext::GetAllDuplicates() const
{
	TArray<UObject*> Duplicates;

	if (AnimationAssetsToRetarget.Num() > 0)
	{
		Duplicates.Append(AnimationAssetsToRetarget);
	}

	if (AnimBlueprintsToRetarget.Num() > 0)
	{
		Duplicates.Append(AnimBlueprintsToRetarget);
	}
	return Duplicates;
}

UObject* FAnimationRetargetContext::GetSingleTargetObject() const
{
	return SingleTargetObject;
}

UObject* FAnimationRetargetContext::GetDuplicate(const UObject* OriginalObject) const
{
	if (HasDuplicates())
	{
		if (const UAnimationAsset* Asset = Cast<const UAnimationAsset>(OriginalObject))
		{
			if (DuplicatedAnimAssets.Contains(Asset))
			{
				return DuplicatedAnimAssets.FindRef(Asset);
			}
		}
		if (const UAnimBlueprint* AnimBlueprint = Cast<const UAnimBlueprint>(OriginalObject))
		{
			if (DuplicatedBlueprints.Contains(AnimBlueprint))
			{
				return DuplicatedBlueprints.FindRef(AnimBlueprint);
			}
		}
	}
	return NULL;
}

FString CreateDesiredName(UObject* Asset, const FNameDuplicationRule* NameRule)
{
	check(Asset);

	FString NewName = Asset->GetName();

	if (NameRule)
	{
		NewName = NameRule->Rename(Asset);
	}

	return NewName;
}

TMap<UObject*, UObject*> DuplicateAssetsInternal(const TArray<UObject*>& AssetsToDuplicate, UPackage* DestinationPackage, const FNameDuplicationRule* NameRule)
{
	FAssetToolsModule& AssetToolsModule = FModuleManager::LoadModuleChecked<FAssetToolsModule>("AssetTools");

	TMap<UObject*, UObject*> DuplicateMap;

	for (auto Iter = AssetsToDuplicate.CreateConstIterator(); Iter; ++Iter)
	{
		UObject* Asset = (*Iter);
		if (!DuplicateMap.Contains(Asset))
		{
			FString PathName = (NameRule) ? NameRule->FolderPath : FPackageName::GetLongPackagePath(DestinationPackage->GetName());

			FString ObjectName;
			FString NewPackageName;
			AssetToolsModule.Get().CreateUniqueAssetName(PathName + "/" + CreateDesiredName(Asset, NameRule), TEXT(""), NewPackageName, ObjectName);

			// create one on skeleton folder
			UObject* NewAsset = AssetToolsModule.Get().DuplicateAsset(ObjectName, PathName, Asset);
			if (NewAsset)
			{
				DuplicateMap.Add(Asset, NewAsset);
			}
		}
	}

	return DuplicateMap;
}
/**
	 * Duplicates the supplied AssetsToDuplicate and returns a map of original asset to duplicate. Templated wrapper that calls DuplicateAssetInternal.
	 *
	 * @param	AssetsToDuplicate	The animations to duplicate
	 * @param	DestinationPackage	The package that the duplicates should be placed in
	 *
	 * @return	TMap of original animation to duplicate
	 */
template<class AssetType>
TMap<AssetType*, AssetType*> DuplicateAssets(const TArray<AssetType*>& AssetsToDuplicate, UPackage* DestinationPackage, const FNameDuplicationRule* NameRule)
{
	TArray<UObject*> Assets;
	for (auto Iter = AssetsToDuplicate.CreateConstIterator(); Iter; ++Iter)
	{
		Assets.Add(*Iter);
	}

	TMap<UObject*, UObject*> AssetMap = DuplicateAssetsInternal(Assets, DestinationPackage, NameRule);

	TMap<AssetType*, AssetType*> ReturnMap;
	for (auto Iter = AssetMap.CreateIterator(); Iter; ++Iter)
	{
		ReturnMap.Add(Cast<AssetType>(Iter->Key), Cast<AssetType>(Iter->Value));
	}
	return ReturnMap;
}

void FAnimationRetargetContext::DuplicateAssetsToRetarget(UPackage* DestinationPackage, const FNameDuplicationRule* NameRule)
{
	if (!HasDuplicates())
	{
		TArray<UAnimationAsset*> AnimationAssetsToDuplicate = AnimationAssetsToRetarget;
		TArray<UAnimBlueprint*> AnimBlueprintsToDuplicate = AnimBlueprintsToRetarget;

		// We only want to duplicate unmapped assets, so we remove mapped assets from the list we're duplicating
		for (TPair<UAnimationAsset*, UAnimationAsset*>& Pair : RemappedAnimAssets)
		{
			AnimationAssetsToDuplicate.Remove(Pair.Key);
		}

		DuplicatedAnimAssets = DuplicateAssets<UAnimationAsset>(AnimationAssetsToDuplicate, DestinationPackage, NameRule);
		DuplicatedBlueprints = DuplicateAssets<UAnimBlueprint>(AnimBlueprintsToDuplicate, DestinationPackage, NameRule);

		// Remapped assets needs the duplicated ones added
		RemappedAnimAssets.Append(DuplicatedAnimAssets);

		DuplicatedAnimAssets.GenerateValueArray(AnimationAssetsToRetarget);
		DuplicatedBlueprints.GenerateValueArray(AnimBlueprintsToRetarget);
	}
}


void ReplaceReferredAnimationsInBlueprint(UAnimBlueprint* AnimBlueprint, const TMap<UAnimationAsset*, UAnimationAsset*>& AnimAssetReplacementMap)
{
	UObject* DefaultObject = AnimBlueprint->GetAnimBlueprintGeneratedClass()->GetDefaultObject();

	FArchiveReplaceObjectRef<UAnimationAsset> ReplaceAr(DefaultObject, AnimAssetReplacementMap, false, false, false);//bNullPrivateRefs, bIgnoreOuterRef, bIgnoreArchetypeRef);
	FArchiveReplaceObjectRef<UAnimationAsset> ReplaceAr2(AnimBlueprint, AnimAssetReplacementMap, false, false, false);//bNullPrivateRefs, bIgnoreOuterRef, bIgnoreArchetypeRef);

	// Replace event graph references
	for (UEdGraph* GraphPage : AnimBlueprint->UbergraphPages)
	{
		for (UEdGraphNode* Node : GraphPage->Nodes)
		{
			FArchiveReplaceObjectRef<UAnimationAsset> ReplaceGraphAr(Node, AnimAssetReplacementMap, false, false, false);
		}
	}

	// Replace references in functions
	for (UEdGraph* GraphPage : AnimBlueprint->FunctionGraphs)
	{
		for (UEdGraphNode* Node : GraphPage->Nodes)
		{
			FArchiveReplaceObjectRef<UAnimationAsset> ReplaceGraphAr(Node, AnimAssetReplacementMap, false, false, false);
		}
	}
}

void FAnimationRetargetContext::RetargetAnimations(USkeleton* OldSkeleton, USkeleton* NewSkeleton)
{
	check(!bConvertAnimationDataInComponentSpaces || OldSkeleton);
	check(NewSkeleton);

	if (bConvertAnimationDataInComponentSpaces)
	{
		// we need to update reference pose before retargeting. 
		// this is to ensure the skeleton has the latest pose you're looking at. 
		USkeletalMesh* PreviewMesh = NULL;
		if (OldSkeleton != NULL)
		{
			PreviewMesh = OldSkeleton->GetPreviewMesh(true);
			if (PreviewMesh)
			{
				OldSkeleton->UpdateReferencePoseFromMesh(PreviewMesh);
			}
		}

		PreviewMesh = NewSkeleton->GetPreviewMesh(true);
		if (PreviewMesh)
		{
			NewSkeleton->UpdateReferencePoseFromMesh(PreviewMesh);
		}
	}

	// anim sequences will be retargeted first becauseReplaceSkeleton forces it to change skeleton
	// @todo: please note that I think we can merge two loops 
	//(without separating two loops - one for AnimSequence and one for everybody else)
	// but if you have animation asssets that does replace skeleton, it will try fix up internal asset also
	// so I think you might be doing twice - look at AnimationAsset:ReplaceSkeleton
	// for safety, I'm doing Sequence first and then everything else
	// however this can be re-investigated and fixed better in the future
	for (auto Iter = AnimationAssetsToRetarget.CreateIterator(); Iter; ++Iter)
	{
		UAnimSequence* AnimSequenceToRetarget = Cast<UAnimSequence>(*Iter);
		if (AnimSequenceToRetarget)
		{
			// Copy curve data from source asset, preserving data in the target if present.
			if (OldSkeleton)
			{
				EditorAnimUtils::CopyAnimCurves(OldSkeleton, NewSkeleton, AnimSequenceToRetarget, USkeleton::AnimCurveMappingName, ERawCurveTrackTypes::RCT_Float);

				// clear transform curves since those curves won't work in new skeleton
				// since we're deleting curves, mark this rebake flag off
				AnimSequenceToRetarget->RawCurveData.TransformCurves.Empty();
				AnimSequenceToRetarget->bNeedsRebake = false;
				// I can't copy transform curves yet because transform curves need retargeting. 
				//EditorAnimUtils::CopyAnimCurves(OldSkeleton, NewSkeleton, AssetToRetarget, USkeleton::AnimTrackCurveMappingName, FRawCurveTracks::TransformType);
			}
		}
	}

	for (auto Iter = AnimationAssetsToRetarget.CreateIterator(); Iter; ++Iter)
	{
		UAnimationAsset* AssetToRetarget = (*Iter);
		if (HasDuplicates())
		{
			AssetToRetarget->ReplaceReferredAnimations(RemappedAnimAssets);
		}
		AssetToRetarget->ReplaceSkeleton(NewSkeleton, bConvertAnimationDataInComponentSpaces);
		AssetToRetarget->MarkPackageDirty();
	}

	// convert all Animation Blueprints and compile 
	for (auto AnimBPIter = AnimBlueprintsToRetarget.CreateIterator(); AnimBPIter; ++AnimBPIter)
	{
		UAnimBlueprint* AnimBlueprint = (*AnimBPIter);

		AnimBlueprint->TargetSkeleton = NewSkeleton;

		if (HasDuplicates())
		{
			// if they have parent blueprint, make sure to re-link to the new one also
			UAnimBlueprint* CurrentParentBP = Cast<UAnimBlueprint>(AnimBlueprint->ParentClass->ClassGeneratedBy);
			if (CurrentParentBP)
			{
				UAnimBlueprint* const* ParentBP = DuplicatedBlueprints.Find(CurrentParentBP);
				if (ParentBP)
				{
					AnimBlueprint->ParentClass = (*ParentBP)->GeneratedClass;
				}
			}
		}

		if (RemappedAnimAssets.Num() > 0)
		{
			ReplaceReferredAnimationsInBlueprint(AnimBlueprint, RemappedAnimAssets);
		}

		FBlueprintEditorUtils::RefreshAllNodes(AnimBlueprint);
		FKismetEditorUtilities::CompileBlueprint(AnimBlueprint, EBlueprintCompileOptions::SkipGarbageCollection);
		AnimBlueprint->PostEditChange();
		AnimBlueprint->MarkPackageDirty();
	}
}

void FAnimationRetargetContext::AddRemappedAsset(UAnimationAsset* OriginalAsset, UAnimationAsset* NewAsset)
{
	RemappedAnimAssets.Add(OriginalAsset, NewAsset);
}

void OpenAssetFromNotify(UObject* AssetToOpen)
{
	GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()->OpenEditorForAsset(AssetToOpen);
}

//////////////////////////////////////////////////////////////////


UObject* RetargetAnimations(USkeleton* OldSkeleton, USkeleton* NewSkeleton, FAnimationRetargetContext& RetargetContext, bool bRetargetReferredAssets, const FNameDuplicationRule* NameRule)
{
	check(NewSkeleton);
	UObject* OriginalObject = RetargetContext.GetSingleTargetObject();
	UPackage* DuplicationDestPackage = NewSkeleton->GetOutermost();

	if (RetargetContext.HasAssetsToRetarget())
	{
		if (NameRule)
		{
			RetargetContext.DuplicateAssetsToRetarget(DuplicationDestPackage, NameRule);
		}
		RetargetContext.RetargetAnimations(OldSkeleton, NewSkeleton);
	}

	FNotificationInfo Notification(FText::GetEmpty());
	Notification.ExpireDuration = 5.f;

	UObject* NotifyLinkObject = OriginalObject;
	if (OriginalObject && NameRule)
	{
		NotifyLinkObject = RetargetContext.GetDuplicate(OriginalObject);
	}

	if (!NameRule)
	{
		if (OriginalObject)
		{
			Notification.Text = FText::Format(LOCTEXT("SingleNonDuplicatedAsset", "'{0}' retargeted to new skeleton '{1}'"), FText::FromString(OriginalObject->GetName()), FText::FromString(NewSkeleton->GetName()));
		}
		else
		{
			Notification.Text = FText::Format(LOCTEXT("MultiNonDuplicatedAsset", "Assets retargeted to new skeleton '{0}'"), FText::FromString(NewSkeleton->GetName()));
		}

	}
	else
	{
		if (OriginalObject)
		{
			Notification.Text = FText::Format(LOCTEXT("SingleDuplicatedAsset", "'{0}' duplicated to '{1}' and retargeted"), FText::FromString(OriginalObject->GetName()), FText::FromString(DuplicationDestPackage->GetName()));
		}
		else
		{
			Notification.Text = FText::Format(LOCTEXT("MultiDuplicatedAsset", "Assets duplicated to '{0}' and retargeted"), FText::FromString(DuplicationDestPackage->GetName()));
		}
	}

	if (NotifyLinkObject)
	{
		Notification.Hyperlink = FSimpleDelegate::CreateStatic(&OpenAssetFromNotify, NotifyLinkObject);
		Notification.HyperlinkText = LOCTEXT("OpenAssetLink", "Open");
	}

	FSlateNotificationManager::Get().AddNotification(Notification);

	// sync newly created objects on CB
	if (NotifyLinkObject)
	{
		TArray<UObject*> NewObjects = RetargetContext.GetAllDuplicates();
		TArray<FAssetData> CurrentSelection;
		for (auto& NewObject : NewObjects)
		{
			CurrentSelection.Add(FAssetData(NewObject));
		}

		FContentBrowserModule& ContentBrowserModule = FModuleManager::Get().LoadModuleChecked<FContentBrowserModule>("ContentBrowser");
		ContentBrowserModule.Get().SyncBrowserToAssets(CurrentSelection);
	}
	if (OriginalObject && NameRule)
	{
		return RetargetContext.GetDuplicate(OriginalObject);
	}
	return NULL;
}

void UAnimAssistantLibBPLibrary::RetargetAnim(class USkeleton* Source, USkeleton* Dest, class UAnimBlueprint* AnimBP, const FString FolderPath)
{
	if (Source && Dest && AnimBP)
	{
		FNameDuplicationRule NameRule;
		NameRule.FolderPath = FolderPath;
		TArray<TWeakObjectPtr<UObject>> InAnimAssets;
		InAnimAssets.Add(AnimBP);
		FAnimationRetargetContext RetargetContext(InAnimAssets, true, true);
		RetargetAnimations(Source, Dest, RetargetContext, true, &NameRule);
	}
}


void UAnimAssistantLibBPLibrary::CreateVirtualBone(USkeleton* Dest, FString JsonBoneList)
{
	static const FString KeyBoneName = "BoneName";
	static const FString KeySourceName = "SourceBone";
	static const FString KeyTargetName = "TargetBone";

	const TSharedRef< TJsonReader<> >& Reader = TJsonReaderFactory<>::Create(JsonBoneList);

	TSharedPtr<FJsonObject> RootObject;
	if (FJsonSerializer::Deserialize(Reader, /*out*/ RootObject) && RootObject.IsValid())
	{
		const TArray< TSharedPtr<FJsonValue> >* BoneList;
		if (RootObject->TryGetArrayField(TEXT("BoneList"), BoneList))
		{	
			uint32 IndexBone = 0;
			for (TSharedPtr<FJsonValue> Bone : *BoneList)
			{
				TSharedPtr<FJsonObject> BoneObj = Bone->AsObject();
				IndexBone++;

				if (!BoneObj->HasField(KeyBoneName))
				{
					UE_LOG(LogAnimAssistantLib, Warning, TEXT("Bone[%d] No KeyWord: %s"), IndexBone, *KeyBoneName);
					continue;
				}

				if (!BoneObj->HasField(KeySourceName))
				{
					UE_LOG(LogAnimAssistantLib, Warning, TEXT("Bone[%d] No KeyWord: %s"), IndexBone, *KeySourceName);
					continue;
				}

				if (!BoneObj->HasField(KeyTargetName))
				{
					UE_LOG(LogAnimAssistantLib, Warning, TEXT("Bone[%d] No KeyWord: %s"), IndexBone, *KeyTargetName);
					continue;
				}

				FName BoneName;
				FName SourceName = *BoneObj->GetStringField(KeySourceName);
				FName TargetName = *BoneObj->GetStringField(KeyTargetName);
				if (!Dest->AddNewVirtualBone(SourceName, TargetName, BoneName))
				{
					UE_LOG(LogAnimAssistantLib, Warning, TEXT("Bone[%d] Create Failed  SourceName[%s] TargetName[%s]"), IndexBone, *BoneName.ToString(), *SourceName.ToString(), *TargetName.ToString());
					continue;
				}
				Dest->RenameVirtualBone(BoneName, *BoneObj->GetStringField(KeyBoneName));
			}
		}
		else
		{
			UE_LOG(LogAnimAssistantLib, Warning, TEXT("No KeyWord: BoneList"));
		}
	}
	else
	{
		UE_LOG(LogAnimAssistantLib, Warning, TEXT("Failed to parse Bone List"));
	}
}