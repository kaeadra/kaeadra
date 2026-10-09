#include "IronSiegeEditorUtils.h"
#include "Engine/Font.h"
#include "Fonts/CompositeFont.h"
#include "UObject/UnrealType.h"
#include "Engine/Blueprint.h"
#if WITH_EDITOR
#include "EdGraph/EdGraph.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#endif

int32 UIronSiegeEditorUtils::CopyEditableProperties(UObject* Source, UObject* Dest)
{
	if (!Source || !Dest || !Dest->IsA(Source->GetClass()))
	{
		return 0;
	}
	int32 Copied = 0;
	for (TFieldIterator<FProperty> It(Source->GetClass()); It; ++It)
	{
		FProperty* Prop = *It;
		if (!Prop->HasAnyPropertyFlags(CPF_Edit) || Prop->HasAnyPropertyFlags(CPF_Transient | CPF_EditConst))
		{
			continue;
		}
		Prop->CopyCompleteValue_InContainer(Dest, Source);
		++Copied;
	}
	Dest->Modify();
	return Copied;
}

bool UIronSiegeEditorUtils::SetupCompositeFont(UFont* Font, UObject* RegularFace, UObject* BoldFace)
{
	if (!Font || !RegularFace)
	{
		return false;
	}
	Font->FontCacheType = EFontCacheType::Runtime;
	FCompositeFont& Composite = Font->GetMutableInternalCompositeFont();
	Composite.DefaultTypeface.Fonts.Reset();
	FTypefaceEntry& Regular = Composite.DefaultTypeface.Fonts.AddDefaulted_GetRef();
	Regular.Name = TEXT("Regular");
	Regular.Font = FFontData(RegularFace);
	FTypefaceEntry& Bold = Composite.DefaultTypeface.Fonts.AddDefaulted_GetRef();
	Bold.Name = TEXT("Bold");
	Bold.Font = FFontData(BoldFace ? BoldFace : RegularFace);
	Font->MarkPackageDirty();
	return true;
}

int32 UIronSiegeEditorUtils::ClearEventGraphs(UBlueprint* Blueprint)
{
#if WITH_EDITOR
	if (!Blueprint)
	{
		return 0;
	}
	int32 Removed = 0;
	for (UEdGraph* Graph : Blueprint->UbergraphPages)
	{
		if (!Graph)
		{
			continue;
		}
		const TArray<UEdGraphNode*> Nodes = Graph->Nodes;
		for (UEdGraphNode* Node : Nodes)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
			++Removed;
		}
	}
	FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
	FKismetEditorUtilities::CompileBlueprint(Blueprint);
	return Removed;
#else
	return 0;
#endif
}
