#if WITH_DEV_AUTOMATION_TESTS && WITH_EDITOR
#include "Misc/AutomationTest.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"
#include "Editor.h"
#include "EngineUtils.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "Device/Camera/VirtualCameraSensorActor.h"
#include "DeviceUI/DeviceSensorMonitorHostActor.h"
#include "DeviceUI/DxDeviceSensorMonitorPanel.h"
#include "Framework/Application/SlateApplication.h"
#include "Widgets/SWindow.h"
#include "ImageUtils.h"
#include "Misc/FileHelper.h"

class FCheckDTDeviceDemo final : public IAutomationLatentCommand
{
public:
    explicit FCheckDTDeviceDemo(FAutomationTestBase* In):Test(In),Started(FPlatformTime::Seconds()) {}
    bool Update() override
    {
        if (FPlatformTime::Seconds()-Started<5) return false;
        auto* World=GEditor->PlayWorld.Get();
        if (!Test->TestNotNull(TEXT("demo PIE world"),World)) return true;
        AVirtualLidarSensorActor* Lidar=nullptr; AVirtualCameraSensorActor* Camera=nullptr; ADeviceSensorMonitorHostActor* Host=nullptr;
        for (TActorIterator<AVirtualLidarSensorActor> It(World);It;++It) Lidar=*It;
        for (TActorIterator<AVirtualCameraSensorActor> It(World);It;++It) Camera=*It;
        for (TActorIterator<ADeviceSensorMonitorHostActor> It(World);It;++It) Host=*It;
        if (!Test->TestNotNull(TEXT("LiDAR"),Lidar) || !Test->TestNotNull(TEXT("camera"),Camera) || !Test->TestNotNull(TEXT("monitor host"),Host)) return true;
        auto* Scan=Lidar->ScanComp.Get();
        Test->TestTrue(TEXT("LiDAR hit > 0"),Scan->GetLastHitPointCount()>0);
        int32 SlabHits=0; for (const auto& Point:Scan->GetLastPoints()) if (Point.bHit && Point.SemanticLabel==TEXT("Slab")) ++SlabHits;
        Test->TestTrue(TEXT("Slab hit > 0"),SlabHits>0);
        auto* Panel=Host->GetPanel();
        if (!Test->TestNotNull(TEXT("panel created"),Panel)) return true;
        FString ScreenshotPath;
        if (FParse::Value(FCommandLine::Get(),TEXT("DTDeviceDemoScreenshot="),ScreenshotPath))
        {
            const auto Window=FSlateApplication::Get().FindWidgetWindow(Panel->TakeWidget());
            TArray<FColor> Pixels; FIntVector Size; TArray64<uint8> Png;
            if (Test->TestTrue(TEXT("offscreen demo screenshot"),Window.IsValid() && FSlateApplication::Get().TakeScreenshot(Window.ToSharedRef(),Pixels,Size)))
            {
                FImageUtils::PNGCompressImageArray(Size.X,Size.Y,Pixels,Png);
                Test->TestTrue(TEXT("demo screenshot saved"),FFileHelper::SaveArrayToFile(Png,*ScreenshotPath));
            }
        }
        Panel->SelectSensor(Lidar); Panel->RefreshStatus();
        Test->TestEqual(TEXT("selected sensor"),Panel->GetSelectedSensor(),static_cast<AVirtualSensorActorBase*>(Lidar));
        Test->TestTrue(TEXT("status refreshed"),Panel->GetStatusText().Contains(Lidar->GetSensorId()));
        const int32 Payload=Scan->GetLastServerPayloadPointCount(); const int32 Samples=Scan->HorizontalSamples;
        const FString PayloadBefore=Scan->GetLastJsonPayload();
        Test->TestTrue(TEXT("preview adjustment accepted"),Panel->AdjustPreview(1));
        Test->TestEqual(TEXT("preview preserves measured resolution"),Scan->HorizontalSamples,Samples);
        Test->TestEqual(TEXT("preview preserves server payload"),Scan->GetLastServerPayloadPointCount(),Payload);
        Test->TestEqual(TEXT("preview preserves serialized server payload"),Scan->GetLastJsonPayload(),PayloadBefore);
        Panel->SelectSensor(Camera); const int64 Before=Camera->GetSensorRuntimeStatus().FrameId;
        Panel->CaptureOnce();
        Test->TestTrue(TEXT("one camera capture requested"),Camera->GetSensorRuntimeStatus().FrameId>Before);
        Host->HideMonitor(); Test->TestFalse(TEXT("host removes panel"),Panel->IsInViewport());
        return true;
    }
private:
    FAutomationTestBase* Test; double Started;
};
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTDeviceDemoSmoke,"MA0T10.DevicePlugin.DemoMonitorRhi",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FDTDeviceDemoSmoke::RunTest(const FString&)
{
    AutomationOpenMap(TEXT("/Game/Maps/DTDeviceDemo"));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FCheckDTDeviceDemo(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}
#endif
