#pragma once
#include "Commandlets/Commandlet.h"
#include "CreateDTDeviceDemoCommandlet.generated.h"
/** Creates only TestHost's owned demo package; never touches consumer maps. */
UCLASS()
class UCreateDTDeviceDemoCommandlet : public UCommandlet
{
    GENERATED_BODY()
public:
    UCreateDTDeviceDemoCommandlet();
    virtual int32 Main(const FString& Params) override;
};
