#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Device/Sensor/VirtualLidarVisualizationComp.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLidarPointCloudRendererDecisionTest,
    "MA0T10.SensorV2.Visualization.RendererDecision",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLidarPointCloudRendererDecisionTest::RunTest(const FString& Parameters)
{
    using State = ELidarPointCloudRendererState;
    using Policy = ELidarPointCloudRenderPolicy;

    TestEqual(TEXT("disabled world preview stays disabled"),
        UVirtualLidarVisualizationComp::ResolveRendererState(Policy::AutoPreferNiagara, false, 100, true, true), State::Disabled);
    TestEqual(TEXT("measurement misses are not visible points"),
        UVirtualLidarVisualizationComp::ResolveRendererState(Policy::AutoPreferNiagara, true, 0, true, true), State::Starting);
    TestEqual(TEXT("auto uses Niagara only after readiness"),
        UVirtualLidarVisualizationComp::ResolveRendererState(Policy::AutoPreferNiagara, true, 100, true, true), State::NiagaraActive);
    TestEqual(TEXT("auto falls back when Niagara is not ready"),
        UVirtualLidarVisualizationComp::ResolveRendererState(Policy::AutoPreferNiagara, true, 100, true, false), State::CpuFallback);
    TestEqual(TEXT("forced CPU never selects Niagara"),
        UVirtualLidarVisualizationComp::ResolveRendererState(Policy::ForceCpu, true, 100, true, true), State::CpuFallback);
    TestEqual(TEXT("forced Niagara exposes readiness failure"),
        UVirtualLidarVisualizationComp::ResolveRendererState(Policy::ForceNiagara, true, 100, true, false), State::Error);
    return true;
}

#endif
