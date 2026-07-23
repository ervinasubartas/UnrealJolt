#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "JoltSettings.generated.h"

USTRUCT(BlueprintType)
struct UNREALJOLT_API FJoltBroadphaseLayer
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Layers")
	FName Name;

	FJoltBroadphaseLayer() = default;
	explicit FJoltBroadphaseLayer(FName InName) : Name(InName) {}
};

USTRUCT(BlueprintType)
struct UNREALJOLT_API FJoltObjectLayer
{
	GENERATED_BODY()

	UPROPERTY(Config, EditAnywhere, Category = "Layers")
	FName Name;


	/**
	 * Which broadphase layer this object layer maps to.
	 * Must match a name in UJoltSettings::BroadphaseLayers.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (GetOptions = "GetBroadphaseLayerNames"))
	FName BroadphaseLayer;

	/**
	 * Names of other object layers this layer collides with.
	 * Collision is symmetric: UJoltSettings::PostEditChangeProperty mirrors
	 * edits so both directions stay in sync.
	 */
	UPROPERTY(Config, VisibleAnywhere, Category = "Layers")
	TSet<FName> CollidesWith;
};

UCLASS(Config = Jolt, DefaultConfig, DisplayName = "Jolt Physics")
class UNREALJOLT_API UJoltSettings : public UDeveloperSettings
{
	GENERATED_BODY()
	
public:
	UJoltSettings(const FObjectInitializer& ObjectInitializer);

	// Returns the names of all broadphase layers. Used by the editor to populate the BroadphaseLayer dropdown.
	UFUNCTION()
	TArray<FString> GetBroadphaseLayerNames() const;

	// Simulation

	/** Physics simulation tick rate, in Hz. Each tick is divided into CollisionSteps sub-steps. */
	UPROPERTY(Config, EditAnywhere, Category = "Simulation", meta = (Units = hz))
	int TickRate = 60;

	/**
	 * Number of sub-steps (collision steps) per simulation tick.
	 * The world advances a total of FixedDeltaTime seconds per tick,
	 * split evenly across this many sub-steps.
	 */
	UPROPERTY(Config, EditAnywhere, BlueprintReadOnly, Category = "Simulation")
	int32 CollisionSteps = 1;

	/** Delta time between each physics frame, calculated as 1 / TickRate. */
	UPROPERTY(Config, VisibleAnywhere, Category = "Simulation")
	float FixedDeltaTime = 1.f / 60.f;

	/** Default world gravity, in Unreal units. Can be overridden at runtime via SetGravity. */
	UPROPERTY(Config, EditAnywhere, Category = "Simulation")
	FVector DefaultGravity = FVector(0.f, 0.f, -980.f);

	// Bodies & Memory

	/**
	 * Maximum number of bodies to support.
	 * This is divided by 3, with each chunk reserved for custom, static,
	 * and dynamic bodies respectively.
	 * Increasing this increases the memory used for simulation.
	 * @see https://github.com/jrouwe/JoltPhysics/discussions/917
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bodies & Memory")
	int32 MaxBodies = 65536;
	
	/**
	 * First BodyID available for manually-assigned (non-automatic) bodies.
	 * Always starts at 0. Use this range if you are not relying on
	 * automatic BodyID allocation.
	 */
	UPROPERTY(Config, VisibleAnywhere, BlueprintReadWrite, Category = "Bodies & Memory", meta = (DisplayName = "Custom BodyID Start"))
	int32 CustomBodyIDStart = 0;

	/**
	 * First BodyID reserved for static bodies.
	 * Recalculated automatically whenever MaxBodies changes.
	 */
	UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category = "Bodies & Memory", meta = (DisplayName = "Static BodyID Start"))
	int32 StaticBodyIDStart = 21845;

	/**
	 * First BodyID reserved for dynamic bodies.
	 * Recalculated automatically whenever MaxBodies changes.
	 */
	UPROPERTY(Config, VisibleAnywhere, BlueprintReadOnly, Category = "Bodies & Memory", meta = (DisplayName = "Dynamic BodyID Start"))
	int32 DynamicBodyIDStart = 43690;

	/**
	 * Number of mutexes used to guard bodies from concurrent access during
	 * the physics update. More mutexes reduce thread contention.
	 * Should be a power of 2 in the range [1, 64]. Use 0 to auto-detect.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bodies & Memory", meta = (DisplayName = "Number of Body Mutexes"))
	int32 NumBodyMutexes = 0;

	/**
	 * Maximum number of body pairs to process; any excess pairs are dropped
	 * from simulation. Should generally be set much higher than
	 * MaxContactConstraints, since many nearby bodies will not actually
	 * be touching.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bodies & Memory")
	int32 MaxBodyPairs = 65536;

	/**
	 * Maximum number of contact constraints to process; any excess
	 * constraints are dropped from simulation.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bodies & Memory")
	int32 MaxContactConstraints = 10240;

	/**
	 * Size of the temp allocator used for scratch allocations during the
	 * physics update, in MB. Pre-allocated up front to avoid allocations
	 * during simulation.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Bodies & Memory", meta = (Units = MB))
	int PreAllocatedMemory = 10;
	
	// Threading

	/**
	 * Enables multithreaded simulation.
	 * Currently uses Jolt's example job system implementation, which works
	 * but may need to be replaced with a proper task-graph-based
	 * implementation, as suggested by Jolt's own documentation.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Threading")
	bool bEnableMultithreading = false;

	/** Maximum number of jobs that can be allocated at any one time. */
	UPROPERTY(Config, EditAnywhere, Category = "Threading")
	int MaxPhysicsJobs = 2048;

	/** Maximum number of barriers that can be allocated at any one time. */
	UPROPERTY(Config, EditAnywhere, Category = "Threading", meta = (EditCondition = "bEnableMultithreading"))
	int MaxPhysicsBarriers = 8;

	/**
	 * Number of worker threads to start. The number of concurrent jobs is
	 * one more than this, since the main thread also runs jobs while
	 * waiting on a barrier to complete.
	 * Use -1 to auto-detect the number of CPU cores.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Threading", meta = (EditCondition = "bEnableMultithreading"))
	int MaxThreads = 2;

	// Debug Rendering

	/**
	 * Enables Jolt's built-in debug renderer.
	 * Currently very slow when rendering landscape shapes.
	 * @todo Update the draw-triangle-batch function for a faster debug renderer.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Rendering")
	bool bEnableDebugRenderer = true;

	/** Draws debug shapes for static bodies. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Rendering", meta = (EditCondition = "bEnableDebugRenderer"))
	bool bDebugDrawStaticBodies = true;

	/** Draws debug shapes for dynamic bodies. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Rendering", meta = (EditCondition = "bEnableDebugRenderer"))
	bool bDebugDrawDynamicBodies = true;

	/** Draws debug shapes for kinematic bodies. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Rendering", meta = (EditCondition = "bEnableDebugRenderer"))
	bool bDebugDrawKinematicBodies = true;

	/** Draws debug shapes for height fields. */
	UPROPERTY(Config, EditAnywhere, Category = "Debug Rendering", meta = (EditCondition = "bEnableDebugRenderer"))
	bool bDebugDrawHeightFields = true;

	// Layers

	/**
	 * Broadphase layers. Each entry becomes a separate bounding-volume tree
	 * in Jolt's broadphase. Typical setups use 2 (Static / Dynamic).
	 * Jolt's internal type is uint8, so the maximum is 255 layers.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (TitleProperty = "Name"))
	TArray<FJoltBroadphaseLayer> BroadphaseLayers;

	/**
	 * Object layers. Each body is assigned to exactly one.
	 * An entry's array index becomes its JPH::ObjectLayer (uint16) at
	 * runtime, so order is load-bearing for any code that caches layer IDs.
	 * Prefer name-based lookup (UJoltSubsystem::ResolveObjectLayer) in new code.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Layers", meta = (TitleProperty = "Name"))
	TArray<FJoltObjectLayer> ObjectLayers;

	/**
	 * Default layer name used by AddDynamicBody / AddKinematicBody when no
	 * explicit layer is specified. Must match an entry in ObjectLayers.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Layers")
	FName DefaultDynamicLayer;

	/**
	 * Default layer name used by AddStaticBody when no explicit layer is
	 * specified. Must match an entry in ObjectLayers.
	 */
	UPROPERTY(Config, EditAnywhere, Category = "Layers")
	FName DefaultStaticLayer;

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif
};