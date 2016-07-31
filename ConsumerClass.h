#pragma once
#include "RealTimeEWTListener.h"

public ref class ConsumerClass
{
    RealTimeEWTListener* _handle;

public:
    ConsumerClass(RealTimeEWTListener* handle);

    static void StartProcessThread(RealTimeEWTListener* handle);

    void ProcessTrace();
};

