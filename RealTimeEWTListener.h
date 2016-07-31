#pragma once
#define INITGUID
#include <Windows.h>
#include <evntrace.h>
#include <string>


#ifdef RTEWTL_BUILD
#define RTEWTL_API __declspec(dllexport) __cdecl
#elif
#define RTEWTL_API __declspec(dllimport) __cdecl
#endif

class RealTimeEWTListener {

    TRACEHANDLE _sessionHandle;
    TRACEHANDLE _traceHandle;

public:
    RealTimeEWTListener();
    ~RealTimeEWTListener();
    ULONG StartTrace();
    ULONG OpenTrace();
    ULONG StartConsumption();
    ULONG CloseTrace();
    ULONG StopTrace();
    ULONG ProcessTraceThread();

private:
    void PrettyPrintTraceErrorCode(int error);
};

extern "C" {
void RTEWTL_API genRTL(void** handle /*out*/);
void RTEWTL_API deleteRTL(void** handle /*inout*/);

ULONG RTEWTL_API rtlStartTrace(void* handle /*in*/);
ULONG RTEWTL_API rtlOpenTrace(void* handle /*in*/);

ULONG RTEWTL_API rtlStartConsumption(void* handle /*in*/);
ULONG RTEWTL_API rtlStopConsumption(void* handle /*in*/);
}

VOID WINAPI EventRecordCallback(PEVENT_RECORD EventRecord);