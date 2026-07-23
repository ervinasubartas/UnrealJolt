#include "JoltSettings.h"

static const FName StaticLayerName(TEXT("Static"));
static const FName DynamicLayerName(TEXT("Dynamic"));

constexpr int32 MaxBroadphaseLayerCount = 255; // Jolt's JPH::BroadPhaseLayer is a uint8
constexpr int32 MaxObjectLayerCount = 65535; // Jolt's JPH::ObjectLayer is a uint16

UJoltSettings::UJoltSettings(const FObjectInitializer& obj)
{
	CategoryName = "Plugins";
	SectionName = "Jolt";
	
	// Default layer setup: two broadphase layers, two object layers. Dynamic collides with
	// everything; Static only collides with Dynamic.
	if (BroadphaseLayers.Num() == 0)
	{
		BroadphaseLayers.Add(FJoltBroadphaseLayer(StaticLayerName));
		BroadphaseLayers.Add(FJoltBroadphaseLayer(DynamicLayerName));
	}

	if (ObjectLayers.Num() == 0)
	{
		FJoltObjectLayer StaticLayer;
		StaticLayer.Name = StaticLayerName;
		StaticLayer.BroadphaseLayer = StaticLayerName;
		StaticLayer.CollidesWith.Add(DynamicLayerName);
		ObjectLayers.Add(StaticLayer);

		FJoltObjectLayer DynamicLayer;
		DynamicLayer.Name = DynamicLayerName;
		DynamicLayer.BroadphaseLayer = DynamicLayerName;
		DynamicLayer.CollidesWith.Add(StaticLayerName);
		DynamicLayer.CollidesWith.Add(DynamicLayerName);
		ObjectLayers.Add(DynamicLayer);
	}

	if (DefaultDynamicLayer.IsNone()) DefaultDynamicLayer = DynamicLayerName;
	if (DefaultStaticLayer.IsNone()) DefaultStaticLayer = StaticLayerName;
}

TArray<FString> UJoltSettings::GetBroadphaseLayerNames() const
{
	TArray<FString> Names;
	Names.Reserve(BroadphaseLayers.Num());
	for (const FJoltBroadphaseLayer& Layer : BroadphaseLayers)
	{
		if (!Layer.Name.IsNone())
		{
			Names.Add(Layer.Name.ToString());
		}
	}
	return Names;
}

#if WITH_EDITOR
void UJoltSettings::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UJoltSettings, MaxBodies))
	{
		StaticBodyIDStart = MaxBodies / 3;
		DynamicBodyIDStart = MaxBodies / 3 * 2;
	}

	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UJoltSettings, TickRate))
	{
		FixedDeltaTime = 1.0f / static_cast<float>(TickRate);
	}

	if (PropertyChangedEvent.Property != nullptr && PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UJoltSettings, bEnableDebugRenderer))
	{
		if (bEnableDebugRenderer)
		{
			bDebugDrawStaticBodies = true;
			bDebugDrawDynamicBodies = true;
			bDebugDrawKinematicBodies = true;
			bDebugDrawHeightFields = true;
		}
	}

	// Layer validation / symmetry enforcement. Runs on any edit under BroadphaseLayers / ObjectLayers
	// (including nested struct member edits and array add/remove/clear). Cheap enough to run on every
	// layer edit since the arrays are tiny.
	const FName ChangedPropName = PropertyChangedEvent.Property != nullptr ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	const FName MemberPropName = PropertyChangedEvent.MemberProperty != nullptr ? PropertyChangedEvent.MemberProperty->GetFName() : NAME_None;
	static const FName BroadphaseLayersName = GET_MEMBER_NAME_CHECKED(UJoltSettings, BroadphaseLayers);
	static const FName ObjectLayersName = GET_MEMBER_NAME_CHECKED(UJoltSettings, ObjectLayers);
	static const FName DefaultDynamicLayerName = GET_MEMBER_NAME_CHECKED(UJoltSettings, DefaultDynamicLayer);
	static const FName DefaultStaticLayerName = GET_MEMBER_NAME_CHECKED(UJoltSettings, DefaultStaticLayer);

	const bool bLayersTouched = MemberPropName == BroadphaseLayersName || MemberPropName == ObjectLayersName || MemberPropName == DefaultDynamicLayerName || MemberPropName == DefaultStaticLayerName || ChangedPropName == BroadphaseLayersName || ChangedPropName == ObjectLayersName;

	if (!bLayersTouched) return;
	
	if (BroadphaseLayers.Num() > MaxBroadphaseLayerCount)
	{
		BroadphaseLayers.SetNum(MaxBroadphaseLayerCount);
	}

	if (ObjectLayers.Num() > MaxObjectLayerCount)
	{
		ObjectLayers.SetNum(MaxObjectLayerCount);
	}

	// Static and Dynamic are required layers — restore them silently if deleted.
	auto EnsureBroadphaseLayer = [this](FName LayerName)
	{
		const bool bExists = BroadphaseLayers.ContainsByPredicate(
			[&](const FJoltBroadphaseLayer& L) { return L.Name == LayerName; });
		if (!bExists)
		{
			BroadphaseLayers.Insert(FJoltBroadphaseLayer(LayerName), 0);
		}
	};
	EnsureBroadphaseLayer(StaticLayerName);
	EnsureBroadphaseLayer(DynamicLayerName);

	auto EnsureObjectLayer = [this](FName LayerName)
	{
		const bool bExists = ObjectLayers.ContainsByPredicate(
			[&](const FJoltObjectLayer& L) { return L.Name == LayerName; });
		if (!bExists)
		{
			FJoltObjectLayer Layer;
			Layer.Name = LayerName;
			Layer.BroadphaseLayer = LayerName;
			ObjectLayers.Insert(Layer, 0);
		}
	};
	EnsureObjectLayer(StaticLayerName);
	EnsureObjectLayer(DynamicLayerName);

	// Build a set of valid object layer names so we can filter out stale references.
	TSet<FName> ValidObjectLayerNames;
	ValidObjectLayerNames.Reserve(ObjectLayers.Num());
	for (const FJoltObjectLayer& layer : ObjectLayers)
	{
		if (!layer.Name.IsNone())
		{
			ValidObjectLayerNames.Add(layer.Name);
		}
	}

	TSet<FName> ValidBroadphaseNames;
	ValidBroadphaseNames.Reserve(BroadphaseLayers.Num());
	for (const FJoltBroadphaseLayer& layer : BroadphaseLayers)
	{
		if (!layer.Name.IsNone())
		{
			ValidBroadphaseNames.Add(layer.Name);
		}
	}

	// Drop any CollidesWith entries pointing at deleted/renamed layers, then mirror the edit so the
	// collision relation stays symmetric (A collides with B ⇒ B collides with A).
	for (FJoltObjectLayer& Layer : ObjectLayers)
	{
		TSet<FName> Cleaned;
		for (const FName& otherName : Layer.CollidesWith)
		{
			if (ValidObjectLayerNames.Contains(otherName))
			{
				Cleaned.Add(otherName);
			}
		}
		Layer.CollidesWith = MoveTemp(Cleaned);
	}

	TMap<FName, int32> NameToIndex;
	for (int32 i = 0; i < ObjectLayers.Num(); ++i)
	{
		if (!ObjectLayers[i].Name.IsNone())
		{
			NameToIndex.Add(ObjectLayers[i].Name, i);
		}
	}

	for (int32 i = 0; i < ObjectLayers.Num(); ++i)
	{
		FJoltObjectLayer& Layer = ObjectLayers[i];
		for (const FName& OtherName : Layer.CollidesWith)
		{
			const int32* OtherIdx = NameToIndex.Find(OtherName);
			if (OtherIdx != nullptr && ObjectLayers.IsValidIndex(*OtherIdx))
			{
				ObjectLayers[*OtherIdx].CollidesWith.Add(Layer.Name);
			}
		}
	}

	// If a layer's broadphase reference is missing, fall back to the first broadphase layer so the
	// filter table still builds cleanly. User will see the reverted value in the details panel.
	const FName FallbackBroadphase = BroadphaseLayers.Num() > 0 ? BroadphaseLayers[0].Name : NAME_None;
	for (FJoltObjectLayer& Layer : ObjectLayers)
	{
		if (!ValidBroadphaseNames.Contains(Layer.BroadphaseLayer))
		{
			Layer.BroadphaseLayer = FallbackBroadphase;
		}
	}

	// Default layer names must also point at valid object layers.
	if (!ValidObjectLayerNames.Contains(DefaultDynamicLayer))
	{
		DefaultDynamicLayer = ValidObjectLayerNames.Contains(DynamicLayerName) ? DynamicLayerName : (ObjectLayers.Num() > 0 ? ObjectLayers[0].Name : NAME_None);
	}

	if (!ValidObjectLayerNames.Contains(DefaultStaticLayer))
	{
		DefaultStaticLayer = ValidObjectLayerNames.Contains(StaticLayerName) ? StaticLayerName : (ObjectLayers.Num() > 0 ? ObjectLayers[0].Name : NAME_None);
	}
}
#endif