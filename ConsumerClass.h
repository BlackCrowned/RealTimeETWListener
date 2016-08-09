#pragma once
#include "RealTimeEWTListener.h"

using namespace System;

[SerializableAttribute]
public enum class DiskAction {
    Read = 1,
    Write = 2,
    Other = 3,
};

[SerializableAttribute]
public value struct CallbackData {
    Int32 Index;
    DiskAction Action;
    DateTime Time;
    UInt32 DiskNumber;
    UInt32 IrpFlags;
    UInt32 TransferSize;
    UInt32 Reserved;
    UInt64 ByteOffset;
    //IntPtr FileObject;
    //IntPtr Irp;
    UInt64 HighResResponseTime;
    UInt32 IssuingThreadId;
    Int32 IssuingProcessId;
    String^ IssuingProcessName;
};

public ref class ConsumerClass
{
    RealTimeEWTListener* _handle;

public:
    static UInt64 PerformanceCounterFrequency;

    delegate void EventReceivedHandler(CallbackData);
    
    static event EventReceivedHandler^ EventReceived;

    ConsumerClass(RealTimeEWTListener* handle);

    static ConsumerClass();

    static void StartProcessThread(RealTimeEWTListener* handle);
    void ProcessTrace();

    static void FireEvent(CallbackData data);
    
    static void ShowConsole();
};

