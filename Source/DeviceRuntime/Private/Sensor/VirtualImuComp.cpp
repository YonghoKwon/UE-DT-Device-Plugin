#include "Device/Sensor/VirtualImuComp.h"
#include "Device/Sensor/VirtualSensorActorBase.h"
#include "Device/Sensor/VirtualSensorOutputComp.h"
#include "Device/Sensor/VirtualSensorRecorderComp.h"
#include "Device/Contracts/VirtualSensorMotionProvider.h"
#include "Device/Core/VirtualSensorStreamPublisherComp.h"
#include "Device/Core/VirtualSensorFileSaveSubsystem.h"
#include "Engine/World.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

UVirtualImuComp::UVirtualImuComp()
{PrimaryComponentTick.bCanEverTick=true;PrimaryComponentTick.TickGroup=TG_PostUpdateWork;}
UVirtualImuComp* UVirtualImuComp::Attach(AActor* A)
{
    if(!IsValid(A)||!A->GetRootComponent())return nullptr;
    if(auto* Existing=A->FindComponentByClass<UVirtualImuComp>())return Existing;
    auto* C=NewObject<UVirtualImuComp>(A);C->SetupAttachment(A->GetRootComponent());A->AddInstanceComponent(C);C->RegisterComponent();return C;
}
bool UVirtualImuComp::Configure(const FVirtualImuConfig& V,FString& Error)
{if(Detaching){Error=TEXT("IMU CSV 저장을 마친 뒤 해제합니다.");return false;}if(!V.Validate(Error))return false;Config=V;++Generation;Initialized=false;HasLatest=false;return true;}
void UVirtualImuComp::ResolveServices()
{
    if(auto* A=Cast<AVirtualSensorActorBase>(GetOwner()))if(A->OutputComp)
    {Recorder=A->OutputComp->RecorderComp;Publisher=A->OutputComp->StreamPublisherComp;}
}
void UVirtualImuComp::BeginPlay(){Super::BeginPlay();ResolveServices();}
FString UVirtualImuComp::GetImuId() const
{const auto* A=Cast<AVirtualSensorActorBase>(GetOwner());return (A?A->GetSensorId():GetOwner()?GetOwner()->GetName():TEXT("sensor"))+TEXT(".imu");}
void UVirtualImuComp::SetRecording(bool E)
{ResolveServices();Recording=E&&!Detaching&&IsValid(Recorder);}
void UVirtualImuComp::SetPublishing(bool E)
{ResolveServices();if(Publisher){if(E&&!Detaching)Publisher->StartStream(EVirtualSensorStreamKind::Imu,GetImuId());else Publisher->StopStream(EVirtualSensorStreamKind::Imu,GetImuId());}}
bool UVirtualImuComp::IsPublishing() const
{return IsValid(Publisher)&&Publisher->IsStreamEnabled(EVirtualSensorStreamKind::Imu,GetImuId());}
FString UVirtualImuComp::SaveCsv()
{ResolveServices();return Recorder&&GetWorld()?GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->RequestImuCsv(this,GetImuId(),Recorder->GetImuSamples(GetImuId())):FString();}
bool UVirtualImuComp::RequestDetach(FString& Message)
{
    if(Detaching){Message=DetachStatus;return false;}
    ResolveServices();
    SetRecording(false);SetPublishing(false);
    if(!IsValid(Recorder)||Recorder->GetImuSamples(GetImuId()).IsEmpty())
    {Message=TEXT("IMU 해제 완료 · 기록된 표본 없음");FinishDetach();return true;}
    DetachCsvRequest=SaveCsv();
    const auto Status=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(DetachCsvRequest);
    if(DetachCsvRequest.IsEmpty()||Status.IsTerminal())
    {DetachStatus=Message=TEXT("CSV 저장을 시작할 수 없습니다 · IMU는 유지됩니다. 저장 상태를 확인한 뒤 다시 해제하세요.");return false;}
    Detaching=true;DetachStatus=Message=TEXT("기록·전송 중지 · CSV 저장을 마친 뒤 IMU를 해제합니다.");return true;
}
void UVirtualImuComp::FinishDetach()
{
    if(IsValid(Recorder))Recorder->ClearImuSamples(GetImuId());
    DestroyComponent();
}
void UVirtualImuComp::EndPlay(const EEndPlayReason::Type R)
{SetPublishing(false);Recording=false;Super::EndPlay(R);}
TSharedRef<FJsonObject> UVirtualImuComp::BatchJson(const FString& Id,const FVirtualImuConfig& C,const TArray<FVirtualImuSample>& Samples)
{
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("schemaVersion"),TEXT("virtual-imu.v1"));R->SetStringField(TEXT("sensorId"),Id);
    R->SetStringField(TEXT("coordinates"),TEXT("sensor FLU; world FLU (UE Y reflected); specific force=R^-1(a-g); m/s^2; rad/s"));
    R->SetStringField(TEXT("orientationSource"),TEXT("simulator_truth"));R->SetNumberField(TEXT("rateHz"),C.RateHz);
    R->SetStringField(TEXT("clock"),TEXT("fixed simulationSeconds; timestampUtc interpolated between game-frame wall UTC observations; no hardware synchronization claim"));
    R->SetNumberField(TEXT("seed"),C.Seed);R->SetBoolField(TEXT("noiseEnabled"),C.Noise&&C.Profile!=EVirtualImuProfile::Ideal);
    TArray<TSharedPtr<FJsonValue>> Values;for(const auto& S:Samples)Values.Add(MakeShared<FJsonValueObject>(S.ToJson()));R->SetArrayField(TEXT("samples"),Values);return R;
}
TSharedRef<FJsonObject> UVirtualImuComp::StatusJson() const
{
    auto R=MakeShared<FJsonObject>();R->SetStringField(TEXT("sensor_id"),GetImuId());R->SetNumberField(TEXT("rate_hz"),Config.RateHz);
    R->SetBoolField(TEXT("recording"),Recording);R->SetBoolField(TEXT("publishing"),IsPublishing());R->SetBoolField(TEXT("has_sample"),HasLatest);
    R->SetNumberField(TEXT("samples"),SampleCount);R->SetNumberField(TEXT("discontinuities"),Model.Discontinuities);R->SetNumberField(TEXT("skipped_samples"),Model.SkippedSamples);
    if(HasLatest)R->SetObjectField(TEXT("latest"),Latest.ToJson());return R;
}
void UVirtualImuComp::TickComponent(float D,ELevelTick T,FActorComponentTickFunction* F)
{
    Super::TickComponent(D,T,F);if(!GetWorld()||!GetOwner())return;
    if(Detaching)
    {
        const auto Status=GetWorld()->GetSubsystem<UVirtualSensorFileSaveSubsystem>()->GetRequestStatus(DetachCsvRequest);
        if(!Status.IsTerminal())return;
        if(Status.State==EVirtualSensorFileSaveState::Succeeded){FinishDetach();return;}
        Detaching=false;DetachStatus=TEXT("CSV 저장 실패 · IMU는 유지됩니다. 저장 상태를 확인한 뒤 다시 해제하세요.");
        return;
    }
    const double Now=GetWorld()->GetTimeSeconds();if(!Initialized){Model.Reset(Config,Now,FDateTime::UtcNow());Initialized=true;}
    FVirtualImuObservation O;O.Seconds=Now;O.ObservationUtc=FDateTime::UtcNow();O.Pose=GetComponentTransform();
    // Resolve only the explicit attachment chain, including SetActorTransform motion providers.
    for(AActor* A=GetOwner();A;A=A->GetAttachParentActor())
    {
        TInlineComponentArray<UActorComponent*> Components(A);
        for(auto* C:Components)if(auto* P=Cast<IVirtualSensorMotionProvider>(C))
        {const auto M=P->CaptureSensorMotion();O.HasVelocity=true;O.VelocityCm=M.LinearVelocityCmPerSecond+FVector::CrossProduct(M.AngularVelocityRadiansPerSecond,O.Pose.GetLocation()-M.PivotWorldCm);O.AngularVelocity=M.AngularVelocityRadiansPerSecond;break;}
        if(O.HasVelocity)break;
    }
    auto& Samples=TickSamples;Samples.Reset();Samples.Reserve(8);Model.Observe(O,GetWorld()->GetGravityZ(),Samples);if(Samples.IsEmpty())return;
    for(auto& S:Samples)S.ConfigurationRevision=Generation;
    Latest=Samples.Last();HasLatest=true;SampleCount+=Samples.Num();
    if(Recording&&IsValid(Recorder))Recorder->RecordImuSamples(GetImuId(),Samples);
    if(IsPublishing())
    {FString Json;FJsonSerializer::Serialize(BatchJson(GetImuId(),Config,Samples),TJsonWriterFactory<>::Create(&Json));Publisher->SubmitImuBatch(GetImuId(),++BatchCount,Latest.TimestampUtc,Json);}
}
