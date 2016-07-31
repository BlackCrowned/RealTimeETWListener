#include "ConsumerClass.h"

using namespace System;
using namespace System::Threading;

ConsumerClass::ConsumerClass(RealTimeEWTListener* handle) :
    _handle(handle)
{

}

void ConsumerClass::StartProcessThread(RealTimeEWTListener* handle)
{
    auto tmp = gcnew ConsumerClass(handle);
    Thread^ processThread = gcnew Thread(gcnew ThreadStart(tmp, &ConsumerClass::ProcessTrace));

    processThread->Start();

}


void ConsumerClass::ProcessTrace() {
    _handle->ProcessTraceThread();
}