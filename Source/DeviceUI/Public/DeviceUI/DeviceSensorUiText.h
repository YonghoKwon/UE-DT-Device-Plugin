#pragma once
#include "CoreMinimal.h"
namespace SensorToolUiText
{
#define LOCTEXT_NAMESPACE "SensorToolUiText"
inline FText Advanced() { return LOCTEXT("Advanced", "고급"); }
inline FText FileSaveWaiting() { return LOCTEXT("FileSaveWaiting", "파일을 저장하면 처리 결과가 표시됩니다."); }
inline FText OptionState(const FText& Label,bool Enabled)
    { return FText::Format(Enabled?LOCTEXT("OptionOn","{0} · 켜짐"):LOCTEXT("OptionOff","{0} · 꺼짐"),Label); }
inline FText Preview() { return LOCTEXT("Preview", "미리보기"); }
#undef LOCTEXT_NAMESPACE
}
