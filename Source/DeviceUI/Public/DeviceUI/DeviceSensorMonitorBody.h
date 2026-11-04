#pragma once
#include "DeviceUI/DevicePanelBodyCall.h"
#include "DeviceUI/DevicePanelWidgetDecl.h"
#include "Async/Async.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "DeviceUI/DeviceSensorPanelTypes.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "IImageWrapper.h"
#include "IImageWrapperModule.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "RHICommandList.h"
#include "RHIGPUReadback.h"
#include "RenderingThread.h"
#include "DeviceUI/SensorOwnedPresentationStyle.h"
#include "DeviceUI/SensorToolNumericInput.h"
#include "DeviceUI/DeviceSensorUiText.h"
#include "DeviceUI/SensorToolWidgetDecl.h"
#include "DeviceUI/SensorToolWorkspaceStyle.h"
#include "Styling/CoreStyle.h"
#include "TextureResource.h"
#include "DeviceUI/VirtualSensorUiStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SComboButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SWrapBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"
#include "Device/Camera/VirtualCameraCaptureComp.h"
#include "Device/Contracts/DeviceIdentityPolicy.h"
#include "Device/Contracts/VirtualSensorDefaults.h"
#include "Device/Core/DxDeviceSensorChannelSubsystem.h"
#include "Engine/GameInstance.h"
#include "Device/Core/VirtualSensorSchedulerSubsystem.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"
#include "Device/Sensor/DeviceSensorCoordinator.h"
#include "Device/Sensor/RealSensorSourceComp.h"
#include "Device/Sensor/VirtualLidarHeight.h"
#include "Device/Sensor/VirtualLidarScanComp.h"
#include "Device/Sensor/VirtualLidarSensorActor.h"
#include "Device/Sensor/VirtualLidarSensorTypes.h"
#include "Device/Sensor/VirtualLidarVisualizationComp.h"
#include "Device/Sensor/VirtualSensorEditableTypes.h"
#include "Device/Sensor/VirtualSensorTransportComp.h"
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
/** Generic View contract: host policy, public BP state and optional extensions stay in the facade.
 * No concrete project widget, Workspace, scenario or job type is included here.
 * Slate callbacks retain only a weak UObject view; bodies create no tick, subscription or readback. */
struct FDeviceSensorMonitorBody
{
	static FString TransportModeToText(EVirtualSensorTransportMode Mode)
	{
		switch (Mode)
		{
			case EVirtualSensorTransportMode::None:
				return TEXT("None");
			case EVirtualSensorTransportMode::LogOnly:
				return TEXT("LogOnly");
			case EVirtualSensorTransportMode::SaveToFile:
				return TEXT("SaveToFile");
			case EVirtualSensorTransportMode::HttpPost:
				return TEXT("HttpPost");
			default:
				return TEXT("Unknown");
		}
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetFrameSummaryText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;
		FString Warning;
		if (auto* Game = View.GetGameInstance())
			if (auto* Registry = Game->template GetSubsystem<UDxDeviceSensorChannelSubsystem>())
			{
				Warning = Registry->GetTopicConfigurationWarning();
				if (!Warning.IsEmpty())
					Warning = TEXT("\n") + Warning;
			}

		if (DeviceSelf->bShowingLidar && DeviceSelf->LidarComp)
		{
			const FVirtualSensorRuntimeStatus& Status = DeviceSelf->LidarComp->GetRuntimeStatus();
			return FString::Printf(TEXT("Frame: %lld | Scan: %.3fs"), Status.FrameId, DeviceSelf->LidarComp->ScanInterval) + Warning;
		}
		if (!DeviceSelf->bShowingLidar && DeviceSelf->CameraComp)
		{
			const FVirtualSensorRuntimeStatus& Status = DeviceSelf->CameraComp->GetRuntimeStatus();
			return FString::Printf(TEXT("Frame: %lld | Interval: %.3fs"), Status.FrameId, DeviceSelf->CameraComp->CaptureInterval) + Warning;
		}
		return TEXT("Frame: unavailable") + Warning;
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetMeasurementSummaryText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		if (DeviceSelf->bShowingLidar && DeviceSelf->LidarComp)
		{
			const FVirtualSensorRuntimeStatus& Status = DeviceSelf->LidarComp->GetRuntimeStatus();
			return FString::Printf(TEXT("Rays: %d | Points/Hits: %d/%d"),
			                       DeviceSelf->LidarComp->HorizontalSamples * DeviceSelf->LidarComp->VerticalChannels,
			                       Status.TotalPointCount,
			                       Status.HitPointCount);
		}
		if (!DeviceSelf->bShowingLidar && DeviceSelf->CameraComp)
		{
			return FString::Printf(TEXT("Resolution: %dx%d"), DeviceSelf->CameraComp->CaptureResolution.X, DeviceSelf->CameraComp->CaptureResolution.Y);
		}
		return TEXT("Measurements: unavailable");
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetPreviewPolicySummaryText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		const UVirtualLidarScanComp* TargetLidar = DeviceSelf->GetTargetLidarForPreview();
		if (!TargetLidar)
		{
			return TEXT("Preview: unavailable");
		}

		const FVirtualSensorRuntimeStatus& Status = TargetLidar->GetRuntimeStatus();
		return FString::Printf(TEXT("Preview: %s Points=%d Stride=%d Max=%d HitOnly=%s"),
		                       TargetLidar->IsPointCloudPreviewEnabled() ? TEXT("On") : TEXT("Off"),
		                       Status.PreviewPointCount,
		                       TargetLidar->PreviewPointStride,
		                       TargetLidar->MaxPreviewPoints,
		                       TargetLidar->bPointCloudPreviewHitOnly ? TEXT("true") : TEXT("false"));
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetTransportWarningText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		if (const auto* Rate = DeviceSelf->GetSelectedRateDiagnostic())
		{
			if (Rate->State == EVirtualSensorRateState::Starved)
				return TEXT("상태/경고: 측정 정체 · 마지막 측정 이후 새 프레임이 없습니다.");
			if (Rate->State == EVirtualSensorRateState::Failed)
				return TEXT("상태/경고: 측정 오류 · 상세 진단을 확인하세요.");
		}
		if (DeviceSelf->bShowingLidar && DeviceSelf->LidarComp)
		{
			const FString& Warning = DeviceSelf->LidarComp->GetRuntimeStatus().PerformanceWarning;
			return FString::Printf(TEXT("상태/경고: %s"), Warning.IsEmpty() ? TEXT("없음") : *Warning);
		}
		if (!DeviceSelf->bShowingLidar && DeviceSelf->CameraComp)
		{
			const FString& Message = DeviceSelf->CameraComp->GetRuntimeStatus().LastMessage;
			return FString::Printf(TEXT("상태/경고: %s"), Message.IsEmpty() ? TEXT("없음") : *Message);
		}
		return TEXT("상태/경고: 센서 연결 없음");
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetTransportStatusSummaryText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		const UVirtualSensorTransportComp* TransportComp = DeviceSelf->SensorManager
		                                                       ? DeviceSelf->SensorManager->SharedTransportComp.Get()
		                                                       : (DeviceSelf->bShowingLidar && DeviceSelf->LidarComp
		                                                              ? DeviceSelf->LidarComp->TransportComp.Get()
		                                                              : (!DeviceSelf->bShowingLidar && DeviceSelf->CameraComp ? DeviceSelf->CameraComp->TransportComp.Get() : nullptr));
		FString SchedulerText;
		if (DeviceSelf->GetWorld())
			if (const UVirtualSensorSchedulerSubsystem* Scheduler = DeviceSelf->GetWorld()->GetSubsystem<UVirtualSensorSchedulerSubsystem>())
				SchedulerText = TEXT("\n성능: ") + Scheduler->GetTelemetrySummaryText();
		if (!TransportComp)
			return TEXT("Transport: unavailable") + SchedulerText;

		const FVirtualSensorTransportResult& Result = TransportComp->LastResult;
		return FString::Printf(TEXT("Transport: Mode=%s InFlight=%d/%d BackpressureRejected=%d Retries=%d Failed=%d RetryExhausted=%d LastRetries=%d LastSubmitted=%s LastAccepted=%s LastCode=%d%s"),
		                       *TransportModeToText(TransportComp->TransportMode),
		                       TransportComp->InFlightHttpRequestCount,
		                       FMath::Max(1, TransportComp->MaxInFlightHttpRequests),
		                       TransportComp->BackpressureRejectedRequestCount,
		                       TransportComp->TotalHttpRetryAttemptCount,
		                       TransportComp->FailedHttpRequestCount,
		                       TransportComp->RetryExhaustedRequestCount,
		                       Result.RetryAttemptCount,
		                       Result.bSubmitted ? TEXT("true") : TEXT("false"),
		                       Result.bAccepted ? TEXT("true") : TEXT("false"),
		                       Result.HttpStatusCode,
		                       *SchedulerText);
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetViewModeSummaryText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		if (DeviceSelf->bShowingLidar)
		{
			return FString::Printf(TEXT("LiDAR View: %s"), *DeviceSelf->GetLidarViewModeDisplayText());
		}
		return TEXT("Camera View: Render Target");
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetAcceptanceGateSummaryText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		const UVirtualLidarScanComp* TargetLidar = DeviceSelf->GetTargetLidarForPreview();
		const bool bTrueLazValidated = TargetLidar && TargetLidar->WasLastLazTrueCompressionValidated();
		const bool bHasServerPayload = DeviceSelf->bShowingLidar
		                                   ? (TargetLidar && !TargetLidar->GetLastJsonPayload().IsEmpty())
		                                   : (DeviceSelf->CameraComp && !DeviceSelf->CameraComp->GetLastJsonPayload().IsEmpty());

		return FString::Printf(TEXT("Acceptance Gates: WBP=ManualPIEPending Server=%s LAZ=%s RealSensor=DeploymentEvidencePending"),
		                       bHasServerPayload ? TEXT("PayloadCached") : TEXT("PayloadPending"),
		                       bTrueLazValidated ? TEXT("TrueValidated") : TEXT("TrueCompressionPending"));
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetRealSensorDeploymentSummaryText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		if (DeviceSelf->RealSensorSourceComp)
		{
			return DeviceSelf->RealSensorSourceComp->GetDeploymentReadinessSummaryText();
		}
		return TEXT("Real Sensor Deployment: source not bound; live adapter evidence pending");
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString GetPointCloudRendererStatusText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		const UVirtualLidarVisualizationComp* Visualization = DeviceSelf->GetLidarVisualizationComponent();
		if (!Visualization)
			return TEXT("3D 포인트 렌더러: 사용할 수 없음");
		const FVirtualLidarRendererTelemetry& Render = Visualization->GetRendererTelemetry();
		const TCHAR* StateText = TEXT("꺼짐");
		switch (Render.State)
		{
			case ELidarPointCloudRendererState::Starting:
				StateText = TEXT("시작 중");
				break;
			case ELidarPointCloudRendererState::NiagaraActive:
				StateText = TEXT("Niagara 활성");
				break;
			case ELidarPointCloudRendererState::CpuFallback:
				StateText = TEXT("CPU fallback");
				break;
			case ELidarPointCloudRendererState::Error:
				StateText = TEXT("오류");
				break;
			case ELidarPointCloudRendererState::Disabled:
			default:
				break;
		}
		return FString::Printf(
		    TEXT("3D 포인트 렌더러: %s | 측정=%d 검출=%d 업로드=%d 표시=%d | %s"),
		    StateText,
		    Render.MeasuredPointCount,
		    Render.HitPointCount,
		    Render.UploadedPointCount,
		    Render.VisiblePointCount,
		    Render.Message.IsEmpty() ? TEXT("상태 메시지 없음") : *Render.Message);
	}

#undef LOCTEXT_NAMESPACE
#define LOCTEXT_NAMESPACE "VirtualSensorMonitorPanelWidget"
	template <class TView>
	static FString BuildSemanticLegendText(TView& View)
	{
		TWeakObjectPtr<TView> DeviceOwner(&View);
		auto* DeviceSelf = &View;

		if (!DeviceSelf->LidarComp)
			return TEXT("의미 분류 규칙 없음");
		FString Text = TEXT("분류: ");
		const int32 Count = FMath::Min(6, DeviceSelf->LidarComp->SemanticClassRules.Num());
		for (int32 Index = 0; Index < Count; ++Index)
		{
			if (Index > 0)
				Text += TEXT(" · ");
			const FVirtualLidarSemanticClassRule& Rule = DeviceSelf->LidarComp->SemanticClassRules[Index];
			Text += Rule.Label.IsNone() ? TEXT("이름 없음") : Rule.Label.ToString();
		}
		if (DeviceSelf->LidarComp->SemanticClassRules.Num() > Count)
		{
			Text += FString::Printf(TEXT(" · 외 %d개"), DeviceSelf->LidarComp->SemanticClassRules.Num() - Count);
		}
		if (Count == 0)
			Text += FString::Printf(TEXT("미분류=%s"), *DeviceSelf->LidarComp->DefaultSemanticLabel.ToString());
		return Text;
	}
	static bool SupportsRangeOverlays(ELidarMonitorProjectionMode P)
	{
		return P == ELidarMonitorProjectionMode::RangeImage || P == ELidarMonitorProjectionMode::Split;
	}
	static bool SupportsAdaptiveDistance(ELidarColorMode C)
	{
		return C == ELidarColorMode::DistanceTurbo || C == ELidarColorMode::DistanceViridis || C == ELidarColorMode::DistanceGray;
	}
};
#undef LOCTEXT_NAMESPACE
