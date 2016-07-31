#include "RealTimeEWTListener.h"

#include <algorithm>
#include <iostream>
#include <cstring>
#include <Evntcons.h>

RealTimeEWTListener::RealTimeEWTListener() :
    _sessionHandle(0),
    _traceHandle(0)
{
    //Open a console!
    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);
    freopen("CONIN$", "w", stdin);
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

    //Allocate the buffer
    auto buffer = new char[buffer_size];

    //Zero initialize entire buffer (as required)
    std::fill_n(buffer, buffer_size, 0);
    
    PEVENT_TRACE_PROPERTIES properties = reinterpret_cast<PEVENT_TRACE_PROPERTIES>(buffer);

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
    strcpy(buffer + struct_size, KERNEL_LOGGER_NAME);

    //Stop the trace first
    int error = ControlTrace(0, KERNEL_LOGGER_NAME, properties, EVENT_TRACE_CONTROL_STOP);
    if (error != ERROR_SUCCESS) {
        std::cerr << "Failed to control close trace with code: ";
        PrettyPrintTraceErrorCode(error);
    }
    std::fill_n(buffer, buffer_size, 0);

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

    delete[] buffer;

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
    _thread = std::thread(&RealTimeEWTListener::ProcessTraceThread, this);
    _thread.detach();

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

    //Allocate the buffer
    auto buffer = new char[buffer_size];

    //Zero initialize entire buffer (as required)
    std::fill_n(buffer, buffer_size, 0);

    PEVENT_TRACE_PROPERTIES properties = reinterpret_cast<PEVENT_TRACE_PROPERTIES>(buffer);

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
    strcpy(buffer + struct_size, KERNEL_LOGGER_NAME);

    //Stop the trace first
    int error = ControlTrace(0, KERNEL_LOGGER_NAME, properties, EVENT_TRACE_CONTROL_STOP);
    if (error != ERROR_SUCCESS) {
        std::cerr << "Failed to control close trace with code: ";
        PrettyPrintTraceErrorCode(error);
    }
    else {
        _sessionHandle = 0;
    }

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

void WINAPI EventRecordCallback(PEVENT_RECORD EventRecord) {
    std::cout << "Received event" << std::endl;
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
