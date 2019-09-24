#include "RealTimeEWTListener.h"

#include <algorithm>
#include <iostream>
#include <vector>
#include <cstring>
#include <Evntcons.h>

#include "ConsumerClass.h"

#define _CRT_SECURE_NO_WARNINGS

RealTimeEWTListener::RealTimeEWTListener() :
    _sessionHandle(0),
    _traceHandle(0)
{

}

RealTimeEWTListener::~RealTimeEWTListener() {
    if (_traceHandle != 0) {
        CloseTrace();
    }
    if (_sessionHandle != 0) {
        StopTrace();
    }
}

void RealTimeEWTListener::PrettyPrintTraceErrorCode(int error) {
    switch (error) {
        case ERROR_BAD_LENGTH: std::cerr << "ERROR_BAD_LENGTH";
            break;
        case ERROR_INVALID_PARAMETER: std::cerr << "ERROR_INVALID_PARAMETER";
            break;
        case ERROR_ALREADY_EXISTS: std::cerr << "ERROR_ALREADY_EXISTS";
            break;
        case ERROR_BAD_PATHNAME: std::cerr << "ERROR_BAD_PATHNAME";
            break;
        case ERROR_DISK_FULL: std::cerr << "ERROR_DISK_FULL";
            break;
        case ERROR_ACCESS_DENIED: std::cerr << "ERROR_ACCESS_DENIED";
            break;
        case ERROR_WMI_INSTANCE_NOT_FOUND: std::cerr << "ERROR_WMI_INSTANCE_NOT_FOUND";
            break;
        case ERROR_MORE_DATA: std::cerr << "ERROR_MORE_DATA";
            break;
        case ERROR_NO_SYSTEM_RESOURCES: std::cerr << "ERROR_NO_SYSTEM_RESOURCES";
            break;
        default: std::cerr << "Unknown";
    }
    std::cerr << " (" << error << ")" << std::endl;
}

ULONG RealTimeEWTListener::StartTrace() {
    size_t struct_size = sizeof(EVENT_TRACE_PROPERTIES);
    size_t session_name_size = sizeof(KERNEL_LOGGER_NAME);
    size_t buffer_size = struct_size + session_name_size;

    //Allocate the buffer and zero initialize (as required)
    auto buffer = std::vector<char>(buffer_size, static_cast<char>(0));

    PEVENT_TRACE_PROPERTIES properties = reinterpret_cast<PEVENT_TRACE_PROPERTIES>(buffer.data());

    properties->Wnode.BufferSize = buffer_size;
    properties->Wnode.Guid = SystemTraceControlGuid;  //Will be set automatically
    properties->Wnode.ClientContext = 2;    //Use system time (https://msdn.microsoft.com/en-us/library/windows/desktop/aa364160%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396)
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    //properties->BufferSize = 512;   //in KB
    //properties->MinimumBuffers = 2;
    //properties->MaximumBuffers = 22;    //as suggested
    //properties->MaximumFileSize = 0;    //in MB
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;    //Use Real time mode (https://msdn.microsoft.com/en-us/library/windows/desktop/aa364080%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396)
    properties->FlushTimer = 1; //in seconds
    properties->EnableFlags = EVENT_TRACE_FLAG_DISK_IO;
    //properties->AgeLimit = 0; //Not used
    properties->LogFileNameOffset = 0;  //Do not use a log file // Use real time
    properties->LoggerNameOffset = struct_size;

    //Copy name to the end
    strcpy(buffer.data() + struct_size, KERNEL_LOGGER_NAME);

    //Stop the trace first
    int error = ControlTrace(0, KERNEL_LOGGER_NAME, properties, EVENT_TRACE_CONTROL_STOP);
    if (error != ERROR_SUCCESS) {
        std::cerr << "Failed to control close trace with code: ";
        PrettyPrintTraceErrorCode(error);
    }
    std::fill_n(buffer.begin(), buffer_size, static_cast<char>(0));

    properties->Wnode.BufferSize = buffer_size;
    properties->Wnode.Guid = SystemTraceControlGuid;  //Will be set automatically
    properties->Wnode.ClientContext = 2;    //Use system time (https://msdn.microsoft.com/en-us/library/windows/desktop/aa364160%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396)
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    //properties->BufferSize = 512;   //in KB
    //properties->MinimumBuffers = 2;
    //properties->MaximumBuffers = 22;    //as suggested
    //properties->MaximumFileSize = 0;    //in MB
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;    //Use Real time mode (https://msdn.microsoft.com/en-us/library/windows/desktop/aa364080%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396)
    properties->FlushTimer = 1; //in seconds
    properties->EnableFlags = EVENT_TRACE_FLAG_DISK_IO;
    //properties->AgeLimit = 0; //Not used
    properties->LogFileNameOffset = 0;  //Do not use a log file // Use real time
    properties->LoggerNameOffset = struct_size;

    //StartTrace
    error = ::StartTrace(&_sessionHandle, KERNEL_LOGGER_NAME, properties);
    if (error != ERROR_SUCCESS) {
        std::cerr << "Failed to start trace with code: ";
        PrettyPrintTraceErrorCode(error);
    }

    return error;
}

ULONG RealTimeEWTListener::OpenTrace() {
    int error = 0;
    EVENT_TRACE_LOGFILE logfile;
    std::fill_n(reinterpret_cast<char*>(&logfile), sizeof(logfile), 0);

    logfile.LogFileName = nullptr;
    logfile.LoggerName = KERNEL_LOGGER_NAME;
    logfile.ProcessTraceMode = (PROCESS_TRACE_MODE_REAL_TIME | PROCESS_TRACE_MODE_EVENT_RECORD | PROCESS_TRACE_MODE_RAW_TIMESTAMP);
    logfile.EventRecordCallback = EventRecordCallback;


    _traceHandle = ::OpenTrace(&logfile);

    if (_traceHandle == INVALID_PROCESSTRACE_HANDLE) {
        error = GetLastError();
        std::cerr << "Failed to open trace with code: " << error << std::endl;
    }

    return error;
}

ULONG RealTimeEWTListener::StartConsumption() {
    ConsumerClass::StartProcessThread(this);

    return 0;
}

ULONG RealTimeEWTListener::CloseTrace() {
    int error;

    error = ::CloseTrace(_traceHandle);
    if (error != ERROR_SUCCESS && error != ERROR_CTX_CLOSE_PENDING) {
        std::cerr << "Failed to close trace with code: ";
        PrettyPrintTraceErrorCode(error);
    }
    else {
        _traceHandle = 0;
    }

    return error;
}

ULONG RealTimeEWTListener::StopTrace() {
    size_t struct_size = sizeof(EVENT_TRACE_PROPERTIES);
    size_t session_name_size = sizeof(KERNEL_LOGGER_NAME);
    size_t buffer_size = struct_size + session_name_size;

    //Allocate the buffer and zero initialize (as required)
    auto buffer = std::vector<char>(buffer_size, static_cast<char>(0));

    PEVENT_TRACE_PROPERTIES properties = reinterpret_cast<PEVENT_TRACE_PROPERTIES>(buffer.data());

    properties->Wnode.BufferSize = buffer_size;
    properties->Wnode.Guid = SystemTraceControlGuid;  //Will be set automatically
    properties->Wnode.ClientContext = 2;    //Use system time (https://msdn.microsoft.com/en-us/library/windows/desktop/aa364160%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396)
    properties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;
    //properties->BufferSize = 512;   //in KB
    //properties->MinimumBuffers = 2;
    //properties->MaximumBuffers = 22;    //as suggested
    //properties->MaximumFileSize = 0;    //in MB
    properties->LogFileMode = EVENT_TRACE_REAL_TIME_MODE;    //Use Real time mode (https://msdn.microsoft.com/en-us/library/windows/desktop/aa364080%28v=vs.85%29.aspx?f=255&MSPPError=-2147217396)
    properties->FlushTimer = 1; //in seconds
    properties->EnableFlags = EVENT_TRACE_FLAG_DISK_IO;
    //properties->AgeLimit = 0; //Not used
    properties->LogFileNameOffset = 0;  //Do not use a log file // Use real time
    properties->LoggerNameOffset = struct_size;

    //Copy name to the end
    strcpy(buffer.data() + struct_size, KERNEL_LOGGER_NAME);

    //Stop the trace first
    int error = ControlTrace(0, KERNEL_LOGGER_NAME, properties, EVENT_TRACE_CONTROL_STOP);
    if (error != ERROR_SUCCESS) {
        std::cerr << "Failed to control close trace with code: ";
        PrettyPrintTraceErrorCode(error);
    }
    else {
        _sessionHandle = 0;
    }

#ifdef DEBUG
	    wprintf(L"\nTrace closed\n");
		wprintf(L"Events lost: %d\n", properties->EventsLost);
		wprintf(L"Realtime buffers lost: %d\n", properties->RealTimeBuffersLost);
#endif // DEBUG


    return error;
}

ULONG RealTimeEWTListener::ProcessTraceThread() {
    return ProcessTrace(&_traceHandle, 1, nullptr, nullptr);
}

void RTEWTL_API genRTL(void** handle) {
    if (handle != nullptr) {
        *handle = new RealTimeEWTListener();
    }
}

void RTEWTL_API deleteRTL(void** handle) {
    if (handle != nullptr && *handle != nullptr) {
        delete static_cast<RealTimeEWTListener*>(*handle);
        *handle = nullptr;
    }
}

ULONG RTEWTL_API rtlStartTrace(void* handle) {
    ULONG error;
    if (handle != nullptr) {
        error = static_cast<RealTimeEWTListener*>(handle)->StartTrace();
    }
    else {
        error =  -1;
    }

    return error;
}

ULONG RTEWTL_API rtlOpenTrace(void* handle) {
    ULONG error;
    if (handle != nullptr) {
        error = static_cast<RealTimeEWTListener*>(handle)->OpenTrace();
    }
    else {
        error = -1;
    }
    return error;
}

void WINAPI EventRecordCallback(PEVENT_RECORD pEvent) {
    /*
     * CODE FROM: https://msdn.microsoft.com/en-us/library/windows/desktop/ee441329(v=vs.85).aspx
     */
    
    //Skip event trace Event
    if (IsEqualGUID(pEvent->EventHeader.ProviderId, EventTraceGuid) &&
        pEvent->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_INFO)
    {
        return;
    }

    int status;
    PTRACE_EVENT_INFO pInfo = NULL;
    DWORD BufferSize = 0;
    LPWSTR pwsEventGuid = NULL;

    status = TdhGetEventInformation(pEvent, 0, NULL, pInfo, &BufferSize);

    if (ERROR_INSUFFICIENT_BUFFER == status)
    {
        pInfo = (TRACE_EVENT_INFO*)malloc(BufferSize);
        if (pInfo == NULL)
        {
            std::cout << "Failed to allocate memory for event info (size=" << BufferSize << ")." << std::endl;
            status = ERROR_OUTOFMEMORY;
            return;
        }

        // Retrieve the event metadata.
        status = TdhGetEventInformation(pEvent, 0, NULL, pInfo, &BufferSize);
    } else if (BufferSize == 0) {
        return;
    }

    if (pInfo->DecodingSource == DecodingSourceWbem) {
        HRESULT hr = StringFromCLSID(pInfo->EventGuid, &pwsEventGuid);

        if (FAILED(hr))
        {
            wprintf(L"StringFromCLSID failed with 0x%x\n", hr);
            status = hr;
        }

#ifdef DEBUG
        wprintf(L"\nEvent GUID: %s\n", pwsEventGuid);
        wprintf(L"Event version: %d\n", pEvent->EventHeader.EventDescriptor.Version);
        wprintf(L"Event type: %d\n", pEvent->EventHeader.EventDescriptor.Opcode);
#endif
    }
    
    //Only process DiskIO events
    if (wcscmp(pwsEventGuid, L"{3D6FA8D4-FE05-11D0-9DDA-00C04FD7BA7C}") != 0) {
        CoTaskMemFree(pwsEventGuid);
        free(pInfo);
        return;
    }
    //Only process read and write events
    if (!(pEvent->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_IO_READ || pEvent->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_IO_WRITE)) {
        CoTaskMemFree(pwsEventGuid);
        free(pInfo);
        return;
    }
    
    FILETIME fileTime;

    fileTime.dwHighDateTime = pEvent->EventHeader.TimeStamp.HighPart;
    fileTime.dwLowDateTime = pEvent->EventHeader.TimeStamp.LowPart;

    /*DWORD pointerSize;

    if (EVENT_HEADER_FLAG_32_BIT_HEADER == (pEvent->EventHeader.Flags & EVENT_HEADER_FLAG_32_BIT_HEADER))
    {
        pointerSize = 4;
    }
    else
    {
        pointerSize = 8;
    }*/

    struct DiskIo_TypeGroup1
    {
        unsigned int DiskNumber;          //0
        unsigned int IrpFlags;            //4
        unsigned int TransferSize;        //8
        unsigned int Reserved;            //12
        unsigned long long ByteOffset;          //16
        unsigned long long FileObject;          //24
        unsigned long long Irp;                 //24 + pointerSize
        unsigned long long HighResResponseTime; //24 + 2 * pointerSize
        unsigned int IssuingThreadId;     //32 + 2 * pointerSize
    };

    DiskIo_TypeGroup1* eventData = static_cast<DiskIo_TypeGroup1*>(pEvent->UserData);

    auto callbackData = CallbackData();

    callbackData.Action = pEvent->EventHeader.EventDescriptor.Opcode == EVENT_TRACE_TYPE_IO_READ ? DiskAction::Read : DiskAction::Write;
    callbackData.Time = DateTime::FromFileTime(((UInt64)fileTime.dwHighDateTime << 32) + (UInt64)fileTime.dwLowDateTime);
    callbackData.DiskNumber = eventData->DiskNumber;
    callbackData.IrpFlags = eventData->IrpFlags;
    callbackData.TransferSize = eventData->TransferSize;
    callbackData.ByteOffset = eventData->ByteOffset;
    //callbackData.FileObject = UIntPtr(eventData->FileObject);
    //callbackData.Irp = UIntPtr(eventData->Irp);
    callbackData.HighResResponseTime = eventData->HighResResponseTime;
    callbackData.IssuingThreadId = eventData->IssuingThreadId;

    auto threadHandle = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, callbackData.IssuingThreadId);
    callbackData.IssuingProcessId = GetProcessIdOfThread(threadHandle);

    ConsumerClass::FireEvent(callbackData);

    CoTaskMemFree(pwsEventGuid);
    free(pInfo);
}

ULONG rtlStartConsumption(void* handle) {
    ULONG error;
    auto ewt_listener = static_cast<RealTimeEWTListener*>(handle);
    if (handle != nullptr) {
        error = ewt_listener->OpenTrace();
        if (!error)
            error = ewt_listener->StartConsumption();
    }
    else {
        error = -1;
    }

    return error;
}

ULONG rtlStopConsumption(void* handle) {
    ULONG error;
    auto ewt_listener = static_cast<RealTimeEWTListener*>(handle);
    if (handle != nullptr) {
        error = ewt_listener->CloseTrace();
    }
    else {
        error = -1;
    }

    return error;
}
