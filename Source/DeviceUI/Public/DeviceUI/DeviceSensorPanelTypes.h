#pragma once
#include "CoreMinimal.h"
#include "DeviceSensorPanelTypes.generated.h"

UENUM(BlueprintType)
enum class EVirtualSensorPanelPlacement : uint8
{
    RightCenter,
    LeftCenter,
    BottomCenter
};
USTRUCT(BlueprintType)
struct DEVICEUI_API FVirtualSensorSettingHelpDescriptor
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FName Key = NAME_None;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText Label;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText Description;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText Unit;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText PerformanceImpact;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText RecommendedValue;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DigitalTwin|SensorControl|Help")
    FText Caution;
};

UENUM(BlueprintType)
enum class EVirtualSensorCaptureExportTab : uint8
{
	LiveStream UMETA(DisplayName = "실시간 전송"),
	Capture UMETA(DisplayName = "캡처"),
	Export UMETA(DisplayName = "내보내기"),
	ConnectionLog UMETA(DisplayName = "연결 및 로그")
};
