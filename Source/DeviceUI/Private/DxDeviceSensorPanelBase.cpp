#include "DeviceUI/DxDeviceSensorPanelBase.h"
#include "DeviceUI/SensorOwnedPresentationStyle.h"
#include "DeviceUI/SensorToolWorkspaceStyle.h"
#include "DeviceUI/VirtualSensorUiStyle.h"
#include "Styling/CoreStyle.h"
#include "Components/TextBlock.h"
#include "Components/EditableTextBox.h"
#include "Blueprint/WidgetTree.h"
bool UDxDeviceSensorPanelBase::HandleNavigateTo_Implementation(FName SubTarget)
{
	FString SensorId = SubTarget.ToString();
	if (SensorId.RemoveFromStart(TEXT("Sensor:")) && !SensorId.IsEmpty())
	{
		const auto* Host = GetDevicePanelHost();
		return Host && Host->NavigateToSensor(FName(*SensorId));
	}
	return Super::HandleNavigateTo_Implementation(SubTarget);
}

void UDxDeviceSensorPanelBase::BindDevicePanelHost(UObject* Owner, TSharedPtr<IDevicePanelHostBridge> Bridge)
{
	DeviceHostOwner = Owner;
	DeviceHostBridge = IsValid(Owner) ? MoveTemp(Bridge) : nullptr;
}
void UDxDeviceSensorPanelBase::NativeDestruct()
{
	DeviceHostBridge.Reset();
	DeviceHostOwner.Reset();
	Super::NativeDestruct();
}

int32 UDxDeviceSensorPanelBase::DeviceScaledFontSize(int32 BaseSize, float Scale)
{
	return FMath::Max(8, FMath::RoundToInt(BaseSize * (FMath::IsFinite(Scale) ? FMath::Clamp(Scale, 0.85f, 1.5f) : 1.0f)));
}

FVector2D UDxDeviceSensorPanelBase::DeviceClampPosition(FVector2D Position, FVector2D PanelSize, FVector2D ViewportSize, float Margin)
{
	const float MaxX = FMath::Max(Margin, ViewportSize.X - PanelSize.X - Margin);
	const float MaxY = FMath::Max(Margin, ViewportSize.Y - FMath::Min(PanelSize.Y, 64.0f) - Margin);
	return FVector2D(FMath::Clamp(Position.X, Margin, MaxX), FMath::Clamp(Position.Y, Margin, MaxY));
}

FVector2D UDxDeviceSensorPanelBase::DeviceDraggedPosition(FVector2D CurrentPosition, FVector2D CursorDelta, float DpiScale)
{
	return CurrentPosition + CursorDelta / FMath::Max(0.01f, DpiScale);
}

FVector2D UDxDeviceSensorPanelBase::DeviceResizedSize(
    FVector2D CurrentSize,
    FVector2D CursorDelta,
    float DpiScale,
    FVector2D MinimumSize,
    FVector2D MaximumSize)
{
	const FVector2D Candidate = CurrentSize + CursorDelta / FMath::Max(0.01f, DpiScale);
	return FVector2D(
	    FMath::Clamp(Candidate.X, MinimumSize.X, FMath::Max(MinimumSize.X, MaximumSize.X)),
	    FMath::Clamp(Candidate.Y, MinimumSize.Y, FMath::Max(MinimumSize.Y, MaximumSize.Y)));
}

void UDxDeviceSensorPanelBase::DeviceRegisterInputControl(UEditableTextBox* Input)
{
	if (!Input || Input->GetTypedOuter<UUserWidget>() != this || RegisteredSensorUmg.Contains(Input))
		return;
	RegisteredSensorUmg.Add(Input);
	const FEditableTextBoxStyle Base = Input->WidgetStyle;
	DeviceAddFontRegistration([Weak = TWeakObjectPtr<UEditableTextBox>(Input), Base](float Scale)
	                          { if (Weak.IsValid()) { auto Style=Base; Style.TextStyle.Font.Size=FMath::Max(8, FMath::RoundToInt(Base.TextStyle.Font.Size * Scale)); Weak->WidgetStyle=Style; Weak->SynchronizeProperties(); } }, [Weak = TWeakObjectPtr<UEditableTextBox>(Input)]()
	                          { return Weak.IsValid(); });
	if (HasDeviceAppearanceOwner() || IsDeviceHostOwned())
		SensorFontSetters.Last()(SensorToolFontScale);
}

void UDxDeviceSensorPanelBase::DeviceRegisterTextControl(UTextBlock* Text)
{
	if (!Text || Text->GetTypedOuter<UUserWidget>() != this || RegisteredSensorUmg.Contains(Text))
		return;
	RegisteredSensorUmg.Add(Text);
	const FSlateFontInfo Base = Text->GetFont();
	DeviceAddFontRegistration([Weak = TWeakObjectPtr<UTextBlock>(Text), Base](float Scale)
	                          { if (Weak.IsValid()) { auto Font=Base; Font.Size=FMath::Max(8, FMath::RoundToInt(Base.Size * Scale)); Weak->SetFont(Font); } }, [Weak = TWeakObjectPtr<UTextBlock>(Text)]()
	                          { return Weak.IsValid(); });
	if (HasDeviceAppearanceOwner() || IsDeviceHostOwned())
		SensorFontSetters.Last()(SensorToolFontScale);
}

void UDxDeviceSensorPanelBase::RegisterSensorNativeFont(TSharedRef<SEditableTextBox> Widget, const SEditableTextBox::FArguments& Args)
{
	FSlateFontInfo Base = Args._Font.Get(Args._Style->TextStyle.Font);
	if (IsDeviceHostOwned())
		Widget->SetStyle(&GetToolInputStyle());
	DeviceAddFontRegistration([Weak = TWeakPtr<SEditableTextBox>(Widget), Owner = TWeakObjectPtr<UDxDeviceSensorPanelBase>(this), Base](float Scale)
	                          {
        if(auto W=Weak.Pin())
        {
            if(!Owner.IsValid()||!Owner->SupportsOwnedDocking()){auto Font=Base;Font.Size=FMath::Max(8,FMath::RoundToInt(Base.Size*Scale));W->SetFont(Font);return;}
            W->SetFont(TAttribute<FSlateFontInfo>::CreateLambda([Owner,Base,Scale](){auto Font=Base;const bool Owned=Owner.IsValid()&&Owner->SupportsOwnedDocking();const int32 Size=Owned?FSensorOwnedPresentationStyle::BodySize:Base.Size;Font.Size=FMath::Max(8,FMath::RoundToInt(Size*(Owned?Owner->GetSensorToolVisualScale():Scale)));return Font;}));
        } }, [Weak = TWeakPtr<SEditableTextBox>(Widget)]()
	                          { return Weak.IsValid(); });
	if (HasDeviceAppearanceOwner() || IsDeviceHostOwned())
		SensorFontSetters.Last()(GetSensorToolVisualScale());
}

void UDxDeviceSensorPanelBase::RegisterSensorNativeFont(TSharedRef<SButton> Widget, const SButton::FArguments& Args)
{
	if (IsDeviceHostOwned())
	{
		if (Args._ButtonStyle == &FVirtualSensorUiStyle::ButtonStyle() || Args._ButtonStyle == &FSensorToolWorkspaceStyle::Button() || Args._ButtonStyle == &FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button"))
			Widget->SetButtonStyle(&GetToolButtonStyle());
		if (!Args._ForegroundColor.IsBound())
			Widget->SetForegroundColor(GetToolTextColor());
	}
	// Only SButton's own default caption, never user-supplied children.
	if (Args._Content.Widget == SNullWidget::NullWidget && Widget->GetContent()->GetType() == FName(TEXT("STextBlock")))
		RegisterSensorNativeFont(StaticCastSharedRef<STextBlock>(Widget->GetContent()), STextBlock::FArguments());
}

void UDxDeviceSensorPanelBase::RegisterSensorNativeFont(TSharedRef<STextBlock> Widget, const STextBlock::FArguments& Args)
{
	FSlateFontInfo Base = Widget->GetFont();
	// Respect the local control's wrapping contract. Forcing every caption to wrap
	// makes short buttons/checkboxes split into syllables at larger font scales.
	const auto Color = Widget->GetColorAndOpacity().GetSpecifiedColor();
	const bool Secondary = Color.Equals(FVirtualSensorUiStyle::SecondaryText) || Color.Equals(GetToolMutedColor());
	DeviceAddFontRegistration([Weak = TWeakPtr<STextBlock>(Widget), Owner = TWeakObjectPtr<UDxDeviceSensorPanelBase>(this), Base, Secondary](float Scale)
	                          {
        if(auto W=Weak.Pin())
        {
            if(!Owner.IsValid()||!Owner->SupportsOwnedDocking()){auto Font=Base;Font.Size=FMath::Max(8,FMath::RoundToInt(Base.Size*Scale));W->SetFont(Font);return;}
            W->SetFont(TAttribute<FSlateFontInfo>::CreateLambda([Owner,Base,Secondary,Scale](){auto Font=Base;const bool Owned=Owner.IsValid()&&Owner->SupportsOwnedDocking();const int32 Size=Owned?(Secondary?FSensorOwnedPresentationStyle::LabelSize:FSensorOwnedPresentationStyle::BodySize):Base.Size;Font.Size=FMath::Max(8,FMath::RoundToInt(Size*(Owned?Owner->GetSensorToolVisualScale():Scale)));return Font;}));
        } }, [Weak = TWeakPtr<STextBlock>(Widget)]()
	                          { return Weak.IsValid(); });
	if (HasDeviceAppearanceOwner() || IsDeviceHostOwned())
		SensorFontSetters.Last()(GetSensorToolVisualScale());
}

void UDxDeviceSensorPanelBase::DeviceApplyFontScale(float Scale)
{
	if ((!HasDeviceAppearanceOwner() && !IsDeviceHostOwned()) || !FMath::IsFinite(Scale))
		return;
	const float PreviousScale = SensorToolFontScale;
	SensorToolFontScale = FMath::Clamp(Scale, 0.85f, 1.5f);
	DeviceFontLayoutChanged();
	DevicePruneFontRegistrations();
	for (auto& Setter : SensorFontSetters)
		Setter(GetSensorToolVisualScale());
	InvalidateLayoutAndVolatility();
	if (bSensorPanelConstructed && !FMath::IsNearlyEqual(PreviousScale, SensorToolFontScale))
		DeviceFontScaleChanged(SensorToolFontScale);
}

void UDxDeviceSensorPanelBase::DeviceAddFontRegistration(TFunction<void(float)> Setter, TFunction<bool()> IsAlive)
{
	DevicePruneFontRegistrations();
	SensorFontSetters.Add(MoveTemp(Setter));
	SensorFontLifetimes.Add(MoveTemp(IsAlive));
}

void UDxDeviceSensorPanelBase::DevicePruneFontRegistrations()
{
	for (int32 I = SensorFontLifetimes.Num() - 1; I >= 0; --I)
		if (!SensorFontLifetimes[I]())
		{
			SensorFontLifetimes.RemoveAt(I);
			SensorFontSetters.RemoveAt(I);
		}
	for (auto It = RegisteredSensorUmg.CreateIterator(); It; ++It)
		if (!It->IsValid())
			It.RemoveCurrent();
}

const FButtonStyle& UDxDeviceSensorPanelBase::GetToolButtonStyle(bool) const
{
	return FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
}
const FEditableTextBoxStyle& UDxDeviceSensorPanelBase::GetToolInputStyle() const
{
	return FCoreStyle::Get().GetWidgetStyle<FEditableTextBoxStyle>("NormalEditableTextBox");
}
