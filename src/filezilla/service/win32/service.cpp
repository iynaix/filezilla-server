#include <windows.h>
#include <tchar.h>
#include <strsafe.h>

#include <functional>
#include <vector>

#include <libfilezilla/string.hpp>

#include "../../../filezilla/service.hpp"

namespace fz::service {

namespace {

std::function<int (int argc, char *argv[])> start_func;
std::function<void ()> stop_func;
std::function<void ()> reload_config_func;
std::vector<std::string> utf8_main_argv;
std::vector<char *> main_argv;

SERVICE_STATUS service_status;
SERVICE_STATUS_HANDLE service_status_handle;

VOID report_service_status(DWORD dwCurrentState, DWORD dwWin32ExitCode, DWORD dwWaitHint)
{
	static DWORD dwCheckPoint = 0;

	// Fill in the SERVICE_STATUS structure.
	service_status.dwCurrentState = dwCurrentState;
	service_status.dwWin32ExitCode = dwWin32ExitCode;
	service_status.dwWaitHint = dwWaitHint;

	if (dwCurrentState == SERVICE_START_PENDING)
		service_status.dwControlsAccepted = 0;
	else
		service_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PARAMCHANGE;

	service_status.dwCheckPoint = (dwCurrentState == SERVICE_RUNNING) || (dwCurrentState == SERVICE_STOPPED) ? 0 : ++dwCheckPoint;

	// Report the status of the service to the SCM.
	SetServiceStatus(service_status_handle, &service_status);
}

VOID WINAPI service_ctrl_handler(DWORD dwCtrl)
{
   // Handle the requested control code.
   switch(dwCtrl)
   {
	  case SERVICE_CONTROL_STOP:
		 report_service_status(SERVICE_STOP_PENDING, NO_ERROR, 0);
		 if (stop_func)
			 stop_func();
		 return;

	  case SERVICE_CONTROL_PARAMCHANGE:
		 report_service_status(SERVICE_RUNNING, NO_ERROR, 0);
		 if (reload_config_func)
			 reload_config_func();
		 return;

	  default:
		 break;
   }
}

VOID WINAPI service_main(DWORD argc, WCHAR **argv)
{
	service_status_handle = RegisterServiceCtrlHandlerW(argv[0], service_ctrl_handler);
	if(!service_status_handle) {
		return;
	}

	// These SERVICE_STATUS members remain as set here
	service_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
	service_status.dwServiceSpecificExitCode = 0;

	// Report initial status to the SCM
	report_service_status(SERVICE_START_PENDING, NO_ERROR, 3000);

	std::vector<std::string> utf8_string_argv;
	utf8_string_argv.reserve(argc);
	main_argv.resize(utf8_main_argv.size()+argc-1);
	for (std::size_t i = 1; i < argc; ++i) {
		auto &s = utf8_string_argv.emplace_back(fz::to_utf8(argv[i]));
		main_argv[utf8_main_argv.size()+i-1] = s.data();
	}

	report_service_status(SERVICE_RUNNING, NO_ERROR, 0);

	service_status.dwServiceSpecificExitCode = (DWORD)start_func((int)main_argv.size(), main_argv.data());

	report_service_status(SERVICE_STOPPED, service_status.dwServiceSpecificExitCode == EXIT_SUCCESS ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR, 0);
}

}

int make(int argc, wchar_t **argv, std::function<int (int argc, char *argv[])> start, std::function<void ()> stop, std::function<void ()> reload_config)
{
	start_func = std::move(start);
	stop_func = std::move(stop);
	reload_config_func = std::move(reload_config);

	utf8_main_argv.reserve((size_t)argc);
	for (int i = 0; i < argc; ++i) {
		auto &s = utf8_main_argv.emplace_back(fz::to_utf8(argv[i]));
		main_argv.push_back(s.data());
	}

	SERVICE_TABLE_ENTRYW DispatchTable[] =  {
		{ (wchar_t*)L"This is going to be ignored, because of OWN_PROCESS", service_main },
		{ nullptr, nullptr }
	};

	// This call returns when the service has stopped.
	// The process should simply terminate when the call returns.
	if (!StartServiceCtrlDispatcherW(DispatchTable)) {
		auto err = GetLastError();

		if (err == ERROR_FAILED_SERVICE_CONTROLLER_CONNECT) {
			// We're being called as a normal process.
			return start_func((int)main_argv.size(), main_argv.data());
		}

		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}

}
