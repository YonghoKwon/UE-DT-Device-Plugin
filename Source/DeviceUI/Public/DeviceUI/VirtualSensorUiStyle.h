#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateTypes.h"

struct DEVICEUI_API FVirtualSensorUiStyle
{
	static const FLinearColor PanelBackground;
	static const FLinearColor HeaderBackground;
	static const FLinearColor SectionBackground;
	static const FLinearColor ControlBackground;
	static const FLinearColor ControlHovered;
	static const FLinearColor ControlPressed;
	static const FLinearColor InputBackground;
	static const FLinearColor PrimaryText;
	static const FLinearColor SecondaryText;
	static const FLinearColor Accent;
	static const FLinearColor Success;
	static const FLinearColor Warning;
	static const FLinearColor Error;

	static const FButtonStyle& ButtonStyle();
	static const FEditableTextBoxStyle& EditableTextBoxStyle();
	static float ContrastRatio(const FLinearColor& Foreground, const FLinearColor& Background);
};
