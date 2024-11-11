!ifndef FZ_MACROS_NSH
!define FZ_MACROS_NSH

!include "FileFunc.nsh"
!include "LogicLib.nsh"
!include "TextFunc.nsh"
!include "process_running.nsh"

!macro Label.PushContext name
	!insertmacro _PushScope Label.${name} _${name}_Label_${LOGICLIB_COUNTER}
	!insertmacro _IncreaseCounter
!macroend
!define Label.PushContext `!insertmacro Label.PushContext`

!macro Label.PopContext name
	!insertmacro _PopScope Label.${name}
!macroend
!define Label.PopContext `!insertmacro Label.PopContext`

!macro MakeVar __MakeVar.name initial_value
	!ifndef __Var.${__MakeVar.name}.defined
		!define __Var.${__MakeVar.name}.defined ${__MakeVar.name}
		Var /Global ${__MakeVar.name}
	!endif
	StrCpy $${__MakeVar.name} `${initial_value}`
!macroend
!define MakeVar `!insertmacro MakeVar`

!macro Store __Store.parent __Store.name
       !insertmacro MakeVar ${__Store.parent}.${__Store.name} `${${__Store.name}}`
!macroend
!define Store `!insertmacro Store`

!macro MakeStaticVar __MakeStaticVar.name initial_value
	!ifndef __StaticVar.${__MakeStaticVar.name}.defined
		!define __StaticVar.${__MakeStaticVar.name}.defined ${__MakeStaticVar.name}
		Var /Global ${__MakeStaticVar.name}
    	StrCpy $${__MakeStaticVar.name} `${initial_value}`
	!endif
!macroend
!define MakeStaticVar `!insertmacro MakeStaticVar`

!macro StaticStore __StaticStore.parent __StaticStore.name
	!insertmacro MakeStaticVar ${__StaticStore.parent}.${__StaticStore.name} `${${__StaticStore.name}}`
!macroend
!define StaticStore `!insertmacro StaticStore`

; Assorted utility macros
!macro WriteMenuReg ID Type Name Value
	!if "${MUI_STARTMENUPAGE_${ID}_REGISTRY_ROOT}" == ""
		!error "You must define MUI_STARTMENUPAGE_REGISTRY_ROOT"
	!endif
	!if "${MUI_STARTMENUPAGE_${ID}_REGISTRY_KEY}" == ""
		!error "You must define MUI_STARTMENUPAGE_REGISTRY_KEY"
	!endif
	!if "${MUI_STARTMENUPAGE_${ID}_REGISTRY_VALUENAME}" == ""
		!error "You must define MUI_STARTMENUPAGE_REGISTRY_VALUENAME"
	!endif

	WriteReg${Type} "${MUI_STARTMENUPAGE_${ID}_REGISTRY_ROOT}" "${MUI_STARTMENUPAGE_${ID}_REGISTRY_KEY}\${MUI_STARTMENUPAGE_${ID}_REGISTRY_VALUENAME}" "${Name}" "${Value}"
!macroend
!define WriteMenuReg `!insertmacro WriteMenuReg`

Var ReadMenuReg.Tmp
!macro ReadMenuReg ID Type Name Var DefValue
	!if "${MUI_STARTMENUPAGE_${ID}_REGISTRY_ROOT}" == ""
		!error "You must define MUI_STARTMENUPAGE_REGISTRY_ROOT"
	!endif
	!if "${MUI_STARTMENUPAGE_${ID}_REGISTRY_KEY}" == ""
		!error "You must define MUI_STARTMENUPAGE_REGISTRY_KEY"
	!endif
	!if "${MUI_STARTMENUPAGE_${ID}_REGISTRY_VALUENAME}" == ""
		!error "You must define MUI_STARTMENUPAGE_REGISTRY_VALUENAME"
	!endif

	ReadReg${Type} $ReadMenuReg.Tmp "${MUI_STARTMENUPAGE_${ID}_REGISTRY_ROOT}" "${MUI_STARTMENUPAGE_${ID}_REGISTRY_KEY}\${MUI_STARTMENUPAGE_${ID}_REGISTRY_VALUENAME}" "${Name}"
	${If} "$ReadMenuReg.Tmp" == ""
		StrCpy $ReadMenuReg.Tmp ${DefValue}
	${Endif}
	StrCpy ${Var} $ReadMenuReg.Tmp
!macroend
!define ReadMenuReg `!insertmacro ReadMenuReg`

!define REG_CURRENT_VERSION "Software\Microsoft\Windows\CurrentVersion"
!define REG_UNINSTALL "${REG_CURRENT_VERSION}\Uninstall"

!macro WriteUninstallReg Type Name Value
	!if "${PRODUCT_NAME}" == ""
		!error "You must define PRODUCT_NAME first"
	!endif
	WriteReg${Type} SHCTX "${REG_UNINSTALL}\${PRODUCT_NAME}" "${Name}" `${Value}`
!macroend
!define WriteUninstallReg `!insertmacro WriteUninstallReg`

!macro ReadUninstallReg Type Name Var
	!if "${PRODUCT_NAME}" == ""
		!error "You must define PRODUCT_NAME first"
	!endif
	ReadReg${Type} ${Var} SHCTX "${REG_UNINSTALL}\${PRODUCT_NAME}" "${Name}"
!macroend
!define ReadUninstallReg `!insertmacro ReadUninstallReg`

!macro DeleteUninstallReg
	!if "${PRODUCT_NAME}" == ""
		!error "You must define PRODUCT_NAME first"
	!endif
	DeleteRegKey SHCTX "${REG_UNINSTALL}\${PRODUCT_NAME}"
!macroend
!define DeleteUninstallReg `!insertmacro DeleteUninstallReg`

!macro WriteRunReg Type Name Value
	!if "${PRODUCT_NAME}" == ""
		!error "You must define PRODUCT_NAME first"
	!endif
	WriteReg${Type} SHCTX "${REG_CURRENT_VERSION}\Run" "${Name}" `${Value}`
!macroend
!define WriteRunReg `!insertmacro WriteRunReg`

!macro DeleteRunReg
	!if "${PRODUCT_NAME}" == ""
		!error "You must define PRODUCT_NAME first"
	!endif
	DeleteRegValue SHCTX "${REG_CURRENT_VERSION}\Run" "${PRODUCT_NAME}"
!macroend
!define DeleteRunReg `!insertmacro DeleteRunReg`

Var GetInstalledSize.Tmp
!macro GetInstalledSize Result
	Push $0
	Push $1
	StrCpy $GetInstalledSize.Tmp 0

	${ForEach} $1 0 256 + 1
		${if} ${SectionIsSelected} $1
			SectionGetSize $1 $0
			IntOp $GetInstalledSize.Tmp $GetInstalledSize.Tmp + $0
		${Endif}
	${Next}

	Pop $1
	Pop $0

	IntFmt $GetInstalledSize.Tmp "0x%08X" $GetInstalledSize.Tmp
	StrCpy ${Result} $GetInstalledSize.Tmp
!macroend
!define GetInstalledSize `!insertmacro GetInstalledSize`

Var V_AsUser_Open_What
Var V_AsUser_Open_Args
!macro AsUser_Open what args
	; We need to copy the arguments into variables, otherwise on windows they won't get properly expanded if they contain variables themselves
	; We are forced to use registers, because those are the only variables UAC knows about and can pass along.
	; Also - and this is damn important - variables need too be passed to UAC_AsUser_ExecShell *QUOTED*
	StrCpy $V_AsUser_Open_What `${what}`
	StrCpy $V_AsUser_Open_Args `${args}`

	SetOutPath `$INSTDIR`
	ShellExecAsUser::ShellExecAsUser `open` `$V_AsUser_Open_What` `$V_AsUser_Open_Args`
!macroend
!define AsUser_Open `!insertmacro AsUser_Open`

; Begins with, case insensitively
!macro _^== _a _b _t _f
	!insertmacro _LOGICLIB_TEMP

	Push $0

	StrLen $0 `${_b}`
	StrCpy $_LOGICLIB_TEMP `${_a}` $0

	Pop $0

	StrCmp $_LOGICLIB_TEMP `${_b}` `${_t}` `${_f}`
!macroend

; Begins with, case sensitively
!macro _S^== _a _b _t _f
	!insertmacro _LOGICLIB_TEMP

	Push $0

	StrLen $0 `${_b}`
	StrCpy $_LOGICLIB_TEMP `${_a}` $0

	Pop $0

	StrCmpS $_LOGICLIB_TEMP `${_b}` `${_t}` `${_f}`
!macroend

; Does not begin with, case insensitively
!macro _^!= _a _b _t _f
	!insertmacro _^== `${_a}` `${_b}` `${_f}` `${_t}`
!macroend

; Does not begin with, case sensitively
!macro _S^!= _a _b _t _f
	!insertmacro _S^== `${_a}` `${_b}` `${_f}` `${_t}`
!macroend

!macro _FindSubstr S result str substr
	!insertmacro _LOGICLIB_TEMP

	${MakeVar} _FindSubstr.len 0
	${MakeVar} _FindSubstr.position -1

	StrLen $_FindSubstr.len `${substr}`

	!define _FindSubstr_start _FindSubstr_label__${LOGICLIB_COUNTER}
	!insertmacro _IncreaseCounter

	!define _FindSubstr_end _FindSubstr_label_${LOGICLIB_COUNTER}
	!insertmacro _IncreaseCounter

${_FindSubstr_start}:
	IntOp $_FindSubstr.position $_FindSubstr.position + 1
	StrCpy $_LOGICLIB_TEMP `${str}` $_FindSubstr.len $_FindSubstr.position

	StrCmp${S} $_LOGICLIB_TEMP `${substr}` `${_FindSubstr_end}` 0
	StrCmp $_LOGICLIB_TEMP '' 0 `${_FindSubstr_start}`
	StrCpy $_FindSubstr.position -1

${_FindSubstr_end}:
	StrCpy ${result} $_FindSubstr.position

	!undef _FindSubstr_start
	!undef _FindSubstr_end
!macroend
!define FindSubstr `!insertmacro _FindSubstr ''`
!define FindSubstrS `!insertmacro _FindSubstr S`

!macro _LeftAndRightOfSubstr S left right str substr
	${MakeVar} _LeftAndRightOfSubstr.position -1

	StrCpy ${left} ""
	StrCpy ${right} ""

	!insertmacro _FindSubstr `${S}` $_LeftAndRightOfSubstr.position `${str}` `${substr}`

	!define _LeftAndRightOfSubstr_end _LeftAndRightOfSubstr_label__${LOGICLIB_COUNTER}
	!insertmacro _IncreaseCounter

	StrCmp $_LeftAndRightOfSubstr.position -1 ${_LeftAndRightOfSubstr_end}

	# Found
	!if `${left}` != ``
		StrCpy ${left} ${str} $_LeftAndRightOfSubstr.position 0
	!endif

	!if `${right}` != ``
		${MakeVar} _LeftAndRightOfSubstr.sublen 0
		StrLen $_LeftAndRightOfSubstr.sublen `${substr}`

		${MakeVar} _LeftAndRightOfSubstr.len 0
		StrLen $_LeftAndRightOfSubstr.len `${str}`

		IntOp $_LeftAndRightOfSubstr.position $_LeftAndRightOfSubstr.position + $_LeftAndRightOfSubstr.sublen
		StrCpy ${right} ${str} $_LeftAndRightOfSubstr.len $_LeftAndRightOfSubstr.position
	!endif

${_LeftAndRightOfSubstr_end}:
	!undef _LeftAndRightOfSubstr_end
!macroend
!define LeftAndRightOfSubstr `!insertmacro _LeftAndRightOfSubstr ''`
!define LeftAndRightOfSubstrS `!insertmacro _LeftAndRightOfSubstr S`

; a Contains substring b, case insensitively
!macro _~= _a _b _t _f
	${MakeVar} _Contains.position -1

	${FindSubstr} $_Contains.position `${_a}` `${_b}`
	StrCmp $_Contains.position -1 `${_f}` `${_t}`
!macroend

; a Contains substring b, case sensitively
!macro _S~= _a _b _t _f
	${MakeVar} _Contains.position -1

	${FindSubstrS} $_Contains.position `${_a}` `${_b}`
	StrCmp $_Contains.position -1 `${_f}` `${_v}`
!macroend

; a does not contain b, case insensitively
!macro _~!= _a _b _t _f
	!insertmacro _~= `${_a}` `${_b}` `${_f}` `${_t}`
!macroend

; a does not contain b, case sensitively
!macro _S~!= _a _b _t _f
	!insertmacro _S~= `${_a}` `${_b}` `${_f}` `${_t}`
!macroend

!macro _AsBool _a _b _t _f
	${Store} _AsBool _b

	!define _AsBool_end _AsBool_label__${LOGICLIB_COUNTER}
	!insertmacro _IncreaseCounter

	StrCmp `${_b}` "true" +1 ${_AsBool_end}
	StrCpy $_AsBool._b "1"

${_AsBool_end}:
	StrCmp $_AsBool._b "1" `${_t}` `${_f}`
	!undef _AsBool_end
!macroend
!define AsBool `"" AsBool`
!define AsBoolImpl `!insertmacro _AsBool ""`

!macro _Quiet _a _b _t _f
	${AsBoolImpl} `$ParseOptions.IsQuiet` `${_t}` `${_f}`
!macroend
!define Quiet `"" Quiet ""`

!macro _KeepService _a _b _t _f
	${AsBoolImpl} `$ParseOptions.KeepService` `${_t}` `${_f}`
!macroend
!define KeepService `"" KeepService ""`

!macro _ProcessRunning _a name _t _f
	!insertmacro _LOGICLIB_TEMP

	Push "${name}"
	call IsProcessRunning
	Pop $_LOGICLIB_TEMP

	StrCmp $_LOGICLIB_TEMP `` `${_f}` `${_t}`
!macroend
!define ProcessRunning `"" ProcessRunning`

;;;; LOGGING ;;;;

Var Log.File.handle
!macro Log.File file
	FileClose $Log.File.Handle
	FileOpen $Log.File.Handle "${file}" w
	ClearErrors
!macroend
!define Log.File `!insertmacro Log.File`

!macro Log string
	${Store} Log string

	${Label.PushContext} Log

	StrCmp "$Log.File.handle" "" ${_Label.Log}detail_print +1
		FileWrite $Log.File.handle "$Log.string$\r$\n"
		ClearErrors

${_Label.Log}detail_print:	
	DetailPrint $Log.string

	${Label.PopContext} Log
!macroend
!define Log `!insertmacro Log`

;;;;;;;;;;;;;;;;;

!define /ifndef	SERVICE_ERROR            0
!define /ifndef	SERVICE_STOPPED          1
!define /ifndef SERVICE_START_PENDING    2
!define /ifndef SERVICE_STOP_PENDING     3
!define /ifndef SERVICE_RUNNING          4
!define /ifndef SERVICE_CONTINUE_PENDING 5
!define /ifndef SERVICE_PAUSE_PENDING    6
!define /ifndef SERVICE_PAUSED           7

; State in Pop 0
; Win32 Exit Code in Pop 1
; Service Specific Exit Code in Pop 2
!macro QueryServiceStatus name
	${Label.PushContext} QueryServiceStatus

	!define /ifndef SERVICE_QUERY_STATUS 4

	Push $1
	Push $0
	Push $2
	Push $3

	System::Call 'ADVAPI32::OpenSCManager(p 0, p 0, i 1) p.r1'

	StrCmp "$1" "0" ${_Label.QueryServiceStatus}error +1 ; ${If} $1 P<> 0
		System::Call 'ADVAPI32::OpenService(p r1, t "${name}", i ${SERVICE_QUERY_STATUS}) p . r2 ?e'
		System::Call 'ADVAPI32::CloseServiceHandle(p r1)'

		StrCmp "$2" "0" ${_Label.QueryServiceStatus}error +1 ; ${If} $2 P<> 0
			System::Call 'ADVAPI32::QueryServiceStatus(p r2, @ r3) i .r0'
			System::Call 'ADVAPI32::CloseServiceHandle(p r2)'

			StrCmp "$0" "0" ${_Label.QueryServiceStatus}error +1 ; ${If} $0 <> 0
				System::Call '*$3(i . r3, i . r0, i, i . r1, i . r2)'
				;${Log} "Type=$3 CurrentState=$0 Win32ExitCode=$1 ServiceSpecificExitCode=$2"
			;${EndIf}
		;${EndIf}
	;${EndIf}

	Goto ${_Label.QueryServiceStatus}end

${_Label.QueryServiceStatus}error:
	StrCpy $0 ${SERVICE_ERROR}
	Pop $1
	StrCpy $2 0

${_Label.QueryServiceStatus}end:
	Pop $3
	Exch $2
	Exch 2
	Exch $1
	Exch 1
	Exch $0

	${Label.PopContext} QueryServiceStatus
!macroend
!define QueryServiceStatus `!insertmacro QueryServiceStatus`

!macro QueryServiceUser out_user service_name
	!define /ifndef SERVICE_QUERY_CONFIG 1

	StrCpy ${out_user} ""

	Push $0
	Push $1
	Push $2
	Push $3

	System::Call 'ADVAPI32::OpenSCManager(p 0, p 0, i 1) p.r1'
	${If} $1 P<> 0
		System::Call 'ADVAPI32::OpenService(p r1, t "${service_name}", i ${SERVICE_QUERY_CONFIG}) p . r2'
		System::Call 'ADVAPI32::CloseServiceHandle(p r1)'
		${If} $2 P<> 0
			System::Call 'ADVAPI32::QueryServiceConfigW(p r2, @ r3, i ${NSIS_MAX_STRLEN}, *i .n) i .r0 ?e'
			Pop $1 #LastError
			System::Call 'ADVAPI32::CloseServiceHandle(p r2)'
			${If} $0 <> 0
				System::Call '*$3(i, i, i, w, w, i, w, w . r0, w)'
				StrCpy ${out_user} $0
			${Else}
				${Log} "FAILED QueryServiceConfigW: LastError: [$1]"
			${EndIf}
		${EndIf}
	${Else}
		${Log} "FAILED OpenSCManager"
	${EndIf}

	Pop $3
	Pop $2
	Pop $1
	Pop $0
!macroend
!define QueryServiceUser `!insertmacro QueryServiceUser`

Var WaitForServiceState.Tmp
!macro WaitForServiceStateEx name state win32Exit serviceExit
	Push $R0 ; Exec result
	Push $R1 ; Counter

	StrCpy $R1 10 ; Maximum number of tries

	${Do}
		IntOp $R1 $R1 - 1

		${QueryServiceStatus} "${name}"
		Pop $R0
		Pop "${win32Exit}"
		Pop "${serviceExit}"

		${If} $R0 == ${SERVICE_ERROR}
			StrCpy "${win32Exit}" "SERVICE_ERROR"
			${Break}
		${EndIf}

		${If} $R0 == ${SERVICE_${state}}
		${OrIf} $R1 == 0
			${Break}
		${Else}
			Sleep 500
		${Endif}
	${Loop}

	Pop $R1
	Pop $R0
!macroend
!define WaitForServiceStateEx `!insertmacro WaitForServiceStateEx`

!macro WaitForServiceState name state
	${WaitForServiceStateEx} "${name}" "${state}" "$WaitForServiceState.Tmp" "$WaitForServiceState.Tmp"
!macroend
!define WaitForServiceState `!insertmacro WaitForServiceState`

Var ReportOnServiceError.win32Exit
Var ReportOnServiceError.serviceExit
!macro ReportOnServiceError name
	ClearErrors
	${WaitForServiceStateEx} "${name}" STOPPED "$ReportOnServiceError.win32Exit" "$ReportOnServiceError.serviceExit"
	${If} "$ReportOnServiceError.win32Exit" != "0"
		${MessageBox} MB_ICONSTOP "There was an error while executing service ${name}.$\n$\nWin32 code: $ReportOnServiceError.win32Exit$\nService code: $ReportOnServiceError.serviceExit" IDOK ""
		SetErrors
	${Endif}
!macroend
!define ReportOnServiceError `!insertmacro ReportOnServiceError`

!macro AbortOnServiceError name
	${ReportOnServiceError} "${name}"
	${If} ${Errors}
		Abort
	${EndIf}
!macroend
!define AbortOnServiceError `!insertmacro AbortOnServiceError`

!define /ifndef SC_MANAGER_ALL_ACCESS 0xF003F
!define /ifndef SC_MANAGER_CREATE_SERVICE 0x0002
!define /ifndef SERVICE_CHANGE_CONFIG 2
!define /ifndef SERVICE_WIN32_OWN_PROCESS 16
!define /ifndef SERVICE_AUTO_START 2
!define /ifndef SERVICE_DEMAND_START 3
!define /ifndef SERVICE_NO_CHANGE 4294967295
!define /ifndef SERVICE_ERROR_NORMAL 0x00000001

!define /ifndef Service.start_auto ${SERVICE_AUTO_START}
!define /ifndef Service.start_demand ${SERVICE_DEMAND_START}
!define /IfNDef Service.Debug true

; Result code in Pop 0
; Result string in Pop 1
!macro DoService_config name exe args startmode user password
	${Store} DoService_config exe
	${Store} DoService_config args

	${MakeVar} DoService_config.result_code 0
	${MakeVar} DoService_config.result_string "Service ${name} successfully reconfigured."
	${MakeVar} DoService_config.binpath '"$DoService_config.exe" $DoService_config.args'

	Push $0
	Push $1
	Push $2
	Push $3
	Push $4
	Push $5
	Push $6

	System::Call 'ADVAPI32::OpenSCManagerW(p 0, p 0, i ${SC_MANAGER_ALL_ACCESS}) p.r1 ?e'
	Pop $0 #LastError
	${If} $1 P<> 0
		System::Call 'ADVAPI32::OpenServiceW(p r1, t "${name}", i ${SERVICE_CHANGE_CONFIG}) p . r2 ?e'
		Pop $0 #LastError
		System::Call 'ADVAPI32::CloseServiceHandleW(p r1)'
		${If} $2 P<> 0
			StrCpy $4 "$DoService_config.binpath"
			StrCpy $5 "${user}"
			StrCpy $6 "${password}"

			System::Call 'ADVAPI32::ChangeServiceConfigW(p r2, i ${SERVICE_WIN32_OWN_PROCESS}, i ${Service.start_${startmode}}, i ${SERVICE_NO_CHANGE}, t r4, p n, p n, p n, t r5, t r6, p n) i .r1 ?e'
			Pop $0 #LastError
			System::Call 'ADVAPI32::CloseServiceHandleW(p r2)'

			${If} $1 == 0
				StrCpy $DoService_config.result_code $0	
			${EndIf}
		${Else}
			StrCpy $DoService_config.result_code $0
		${EndIf}
	${Else}
		StrCpy $DoService_config.result_code $0	
	${EndIf}

	${If} $DoService_config.result_code == 0
		${Log} $DoService_config.result_string
	${Else}
		StrCpy $DoService_config.result_string "Service ${name} couldn't be reconfigured. Error $0"
	${EndIf}

	Pop $6
	Pop $5
	Pop $4
	Pop $3
	Pop $2
	Pop $1
	Pop $0

	Push $DoService_config.result_string
	Push $DoService_config.result_code
!macroend
!define DoService_config `!insertmacro DoService_config`


; Result code in Pop 0
; Result string in Pop 1
!macro DoService_create name exe args startmode user password
	${Store} DoService_create exe
	${Store} DoService_create args

	${MakeVar} DoService_create.result_code 0
	${MakeVar} DoService_create.result_string "Service ${name} successfully created."
	${MakeVar} DoService_create.binpath '"$DoService_create.exe" $DoService_create.args'

	Push $0
	Push $1
	Push $2
	Push $3
	Push $4
	Push $5
	Push $6

	System::Call 'ADVAPI32::OpenSCManagerW(p 0, p 0, i ${SC_MANAGER_ALL_ACCESS}) p.r1 ?e'
	Pop $0 #LastError
	${If} $1 P<> 0
		StrCpy $4 "$DoService_create.binpath"
		StrCpy $5 "${user}"
		StrCpy $6 "${password}"

		System::Call 'ADVAPI32::CreateServiceW(p r1, t "${name}", t "${name}", i ${SC_MANAGER_CREATE_SERVICE}, i ${SERVICE_WIN32_OWN_PROCESS}, i ${Service.start_${startmode}}, i ${SERVICE_ERROR_NORMAL}, t r4, n, n, n, t r5, t r6) p . r2 ?e'
		Pop $0 #LastError
		System::Call 'ADVAPI32::CloseServiceHandleW(p r1)'
		${If} $2 P<> 0
			System::Call 'ADVAPI32::CloseServiceHandleW(p r2)'
		${Else}
			StrCpy $DoService_create.result_code $0
		${EndIf}
	${Else}
		StrCpy $DoService_create.result_code $0	
	${EndIf}

	${If} $DoService_create.result_code == 0
		${Log} $DoService_create.result_string
	${Else}
		StrCpy $DoService_create.result_string "Service ${name} couldn't be created. Error $0"
	${EndIf}

	Pop $6
	Pop $5
	Pop $4
	Pop $3
	Pop $2
	Pop $1
	Pop $0

	Push $DoService_create.result_string
	Push $DoService_create.result_code
!macroend
!define DoService_create `!insertmacro DoService_create`

!macro DoService op name exe args startmode user password
	${Store} DoService op
	${Store} DoService name
	${Store} DoService exe
	${Store} DoService args
	${Store} DoService user
	${Store} DoService password

	${If} $DoService.user == ""
		StrCpy $DoService.user "LocalSystem"
	${ElseIf} $DoService.user != "LocalSystem"
		${If} $DoService.user ~!= "@"
		${AndIf} $DoService.user ~!= "\"
			StrCpy $DoService.user ".\$DoService.user"
		${EndIf}
	${EndIf}

	${Log} "$DoService.op service $DoService.name: $DoService.exe $DoService.args"

	Push $R0
	Push $R1
	
	${If} $DoService.op == "create"
		${DoService_create} "$DoService.name" "$DoService.exe" "$DoService.args" ${startmode} "$DoService.user" "$DoService.password"
	${ElseIf} $DoService.op == "config"
		${DoService_config} "$DoService.name" "$DoService.exe" "$DoService.args" ${startmode} "$DoService.user" "$DoService.password"
	${Else}
		Push "DoService: Wrong operation"
		Push 1
	${EndIf}

	Pop $R0
	Pop $R1

	${If} ${AsBool} ${Service.Debug}
		${If} $R0 <> 0
			${Log} $R1
		${EndIf}
	${EndIf}

	Pop $R1
	Pop $R0
!macroend
!define DoService `!insertmacro DoService`

!macro CreateServiceEx name exe args startmode user password
	${DoService} create "${name}" "${exe}" "${args}" "${startmode}" "${user}" "${password}"
!macroend
!define CreateServiceEx `!insertmacro CreateServiceEx`

!macro CreateService name exe args startmode
	${DoService} create "${name}" "${exe}" "${args}" "${startmode}" "LocalSystem" ""
!macroend
!define CreateService `!insertmacro CreateService`

!macro StartService name args
	${Store} StartService name
	${Store} StartService args

	Push $R0
	Push $R1

	nsExec::ExecToStack /OEM '$SYSDIR\sc start "$StartService.name" $StartService.args'
	Pop $R0
	Pop $R1

	${If} $R0 <> 0
		${If} ${AsBool} ${Service.Debug}
			${Log} `StartService: "$R1"`
		${EndIf}
	${EndIf}

	Pop $R1
	Pop $R0
!macroend
!define StartService `!insertmacro StartService`

!macro StopService name
	${Store} StopService name

	Push $R0
	Push $R1

	StrCpy $R0 0

	${If} ${ServiceIsRunning} "$StopService.name"
		nsExec::ExecToStack /OEM '$SYSDIR\sc stop "$StopService.name"'
		Pop $R0
		Pop $R1

		${If} $R0 <> 0
		${AndIf} ${AsBool} ${Service.Debug}
			${Log} `StopService: "$R1"`
		${EndIf}
	${EndIf}

	${If} $R0 == 0
		${WaitForServiceState} "$StopService.name" STOPPED
	${EndIf}

	Pop $R1
	Pop $R0
!macroend
!define StopService `!insertmacro StopService`

!macro DeleteService name
	${Store} DeleteService name

	Push $R0
	Push $R1

	nsExec::ExecToStack /OEM '$SYSDIR\sc delete "$DeleteService.name"'
	Pop $R0
	Pop $R1

	${If} ${AsBool} ${Service.Debug}
		${If} $R0 <> 0
			${Log} `DeleteService: "$R1"`
		${EndIf}
	${EndIf}

	Pop $R1
	Pop $R0
!macroend
!define DeleteService `!insertmacro DeleteService`

!macro _ServiceExists a name _t _f
	${Store} _ServiceExists name
	${MakeVar} _ServiceExists.result 0

	Push $R0
	Push $R1

	${QueryServiceStatus} "$_ServiceExists.name"
	Pop $R0
	Pop $_ServiceExists.result
	Pop $R1

	Pop $R1
	Pop $R0

	IntCmp $_ServiceExists.result 1060 `${_f}` `${_t}` `${_t}`
!macroend
!define ServiceExists `"" ServiceExists`

!macro _ServiceIsRunning a name _t _f
	${Store} _ServiceIsRunning name
	${MakeVar} _ServiceIsRunning.result 0

	Push $R0
	Push $R1

	${QueryServiceStatus} "$_ServiceIsRunning.name"
	Pop $_ServiceIsRunning.result
	Pop $R0
	Pop $R1

	Pop $R1
	Pop $R0

	IntCmp $_ServiceIsRunning.result ${SERVICE_RUNNING} `${_t}` `${_f}` `${_f}`
!macroend
!define ServiceIsRunning `"" ServiceIsRunning`

!macro GrantAccess entry user properties
	${MakeVar} GrantAccess.user_for_grant ""
	${MakeVar} GrantAccess.domain ""

	${If} "${user}" != ""
		${LeftAndRightOfSubstr} $GrantAccess.domain $GrantAccess.user_for_grant `${user}` "\"
		${If} $GrantAccess.domain != "."
			StrCpy $GrantAccess.user_for_grant `${user}`
		${EndIf}

		AccessControl::GrantOnFile "$PLUGINSDIR" "$GrantAccess.user_for_grant" `${properties}`
	${EndIf}
!macroend
!define GrantAccess `!insertmacro GrantAccess`

Var ReadAnsiFile.Handle
Var ReadAnsiFile.Line
Var ReadAnsiFile.Num
!macro ReadAnsiFile file callback
	ClearErrors
	StrCpy $ReadAnsiFile.Num 0

	FileOpen $ReadAnsiFile.Handle "${file}" r
	${IfNot} ${Errors}
		${Do}
			FileRead $ReadAnsiFile.Handle $ReadAnsiFile.Line

			${If} "$ReadAnsiFile.Line" != ""
				${callback} $ReadAnsiFile.Num $ReadAnsiFile.Line
				IntOp $ReadAnsiFile.Num $ReadAnsiFile.Num + 1
			${EndIf}
		${LoopUntil} ${Errors}
		ClearErrors
	${EndIf}
	FileClose $ReadAnsiFile.Handle
!macroend
!define ReadAnsiFile `!insertmacro ReadAnsiFile`

!macro ReadAnsiFile.AppendToVar var num line
	${If} "${num}" == "0"
		StrCpy "${var}" ""
	${EndIf}
	StrCpy "${var}" "${var}${line}"
!macroend
!define ReadAnsiFile.AppendToVar `!insertmacro ReadAnsiFile.AppendToVar`

!macro ReadAnsiFile.AppendToVarAndLog var num line
	${Log} "${line}"
	${ReadAnsiFile.AppendToVar} "${var}" "${num}" "${line}"
!macroend
!define ReadAnsiFile.AppendToVarAndLog `!insertmacro ReadAnsiFile.AppendToVarAndLog`

!macro CheckConfigVersion.callback first_line rest num line
	${If} "${num}" == "0"
		StrCpy "${first_line}" "${line}"
		${TrimNewLines} "${first_line}" "${first_line}"
		${Log} 'CheckConfigVersion: got [${first_line}]'
		StrCpy "${rest}" ""
	${Else}
		${If} "${num}" == "1"
			${Log} "Follows list of backed up files:"
		${EndIf}
		${Log} '${line}'
		StrCpy "${rest}" "${rest}${line}"
	${EndIf}
!macroend
!define CheckConfigVersion.callback `!insertmacro CheckConfigVersion.callback`

!macro CheckConfigDirOwnership.callback first_line rest num line
	${If} "${num}" == "0"
    	StrCpy "${first_line}" "${line}"
		${TrimNewLines} "${first_line}" "${first_line}"

        ${If} "${first_line}" != "ok"
    		${Log} 'CheckConfigDirOwnership: issue found'
	    	StrCpy "${first_line}" "ownership"
	    	StrCpy "${rest}" "${line}"
        ${Else}
    		${Log} 'CheckConfigDirOwnership: ok'
            StrCpy "${first_line}" ""
	    	StrCpy "${rest}" ""
        ${EndIf}
    	${Log} "Details follow:"
    ${EndIf}

    ${If} "${num}" != "0"
    ${OrIf} "${first_line}" != ""
        ${Log} '${line}'
    ${EndIf}
!macroend
!define CheckConfigDirOwnership.callback `!insertmacro CheckConfigDirOwnership.callback`

!macro CheckConfigVersionAndOwnership mode first_line rest
	${Store} CheckConfigVersionAndOwnership mode

	StrCpy "${first_line}" ""
	StrCpy "${rest}" ""

	Delete "$PLUGINSDIR\config-version-check-result.txt"
	ClearErrors

	${If} "$CheckConfigVersionAndOwnership.mode" == "ignore"
		StrCpy $CheckConfigVersionAndOwnership.mode "ignore --write-config"
	${EndIf}

	${MakeVar} CheckConfigVersionAndOwnership.user ""
	${QueryServiceUser} $CheckConfigVersionAndOwnership.user "filezilla-server"
	${GrantAccess} "$PLUGINSDIR" $CheckConfigVersionAndOwnership.user "AddFile"

	${StartService} filezilla-server '--config-version-check $CheckConfigVersionAndOwnership.mode --config-version-check-result-file "$PLUGINSDIR\config-version-check-result.txt" --config-dir-ownership-check-result-file "$PLUGINSDIR\config-dir-ownership-check-result.txt"'
	${WaitForServiceState} filezilla-server STOPPED

	${ReadAnsiFile} "$PLUGINSDIR\config-dir-ownership-check-result.txt" '${CheckConfigDirOwnership.callback} "${first_line}" "${rest}"'
	${If} ${Errors}
		${MessageBox} MB_ICONSTOP|MB_OK "$(S_CONFIG_CannotReadOwnershipResult)" IDOK ""
		Abort
	${EndIf}

	${If} "${first_line}" != "ownership"
	    ${ReadAnsiFile} "$PLUGINSDIR\config-version-check-result.txt" '${CheckConfigVersion.callback} "${first_line}" "${rest}"'
	    ${If} ${Errors}
	    ${OrIf} "${first_line}" == ""
		    ${MessageBox} MB_ICONSTOP|MB_OK "$(S_CONFIG_CannotReadResult)" IDOK ""
		    Abort
	    ${EndIf}

	    ${If} "${first_line}" != "ok"
	    ${AndIf} "${first_line}" != "error"
	    ${AndIf} "${first_line}" != "backup"
		    ${MessageBox} MB_ICONSTOP|MB_OK "$(S_CONFIG_UnknownResult)" IDOK ""
		    Abort
	    ${EndIf}
    ${EndIf}
!macroend
!define CheckConfigVersionAndOwnership `!insertmacro CheckConfigVersionAndOwnership`

!macro GenRandomString result
	GetTempFileName "${result}"
	Delete "${result}"
!macroend
!define GenRandomString `!insertmacro GenRandomString`

Var PopStrings.Tmp
!macro PopStrings resultStr resultNum untilCanary
	ClearErrors

	StrCpy "${resultNum}" 0
	StrCpy "${resultStr}" ""

	${Do}
		Pop "$PopStrings.Tmp"

		${If} ${Errors}
		${OrIf} "$PopStrings.Tmp" == "${untilCanary}"
			${Break}
		${EndIf}

		${If} "${resultStr}" != ""
			StrCpy "${resultStr}" "${resultStr}$\n"
		${EndIf}
		StrCpy "${resultStr}" "${resultStr}$PopStrings.Tmp"

		IntOp "${resultNum}" "${resultNum}" + 1
	${Loop}
!macroend
!define PopStrings `!insertmacro PopStrings`

!macro SplitUserAndDomain res_user res_domain user
	${LeftAndRightOfSubstr} ${res_domain} ${res_user} ${user} "\"

	${If} "${res_user}" == ""
		StrCpy ${res_user} ${user}
		${If} "${res_user}" ~!= "@"
			StrCpy ${res_domain} "."
		${EndIf}
	${EndIf}
!macroend
!define SplitUserAndDomain `!insertmacro SplitUserAndDomain`

!define /IfNDef LOGON32_LOGON_NETWORK 3
!define /IfNDef LOGON32_LOGON_SERVICE 5
!macro CheckIfUserCanLogin name password type
	${Store} CheckIfUserCanLogin name
	${Store} CheckIfUserCanLogin password

	Push $0
	Push $1
	Push $2
	Push $3
	Push $4

	${SplitUserAndDomain} $0 $1 $CheckIfUserCanLogin.name

	StrCpy $2 $CheckIfUserCanLogin.password
	StrCpy $3 "${${type}}"

	ClearErrors
	System::Call "advapi32::LogonUserW(w r0, w r1, w r2, i r3, i 0, *p .r4) i .r0 ?e"
	Pop $2
	${If} $0 == 0
		SetErrors
		${Log} "LogonUserW failed with error: [$2]"
	${Else}
		System::Call "kernel32.dll::CloseHandle(p r4) i"
	${Endif}

	Pop $4
	Pop $3
	Pop $2
	Pop $1
	Pop $0
!macroend
!define CheckIfUserCanLogin `!insertmacro CheckIfUserCanLogin`

;-------
; From https://nsis.sourceforge.io/Dump_log_to_file
;-------
!define /IfNDef LVM_GETITEMCOUNT 0x1004
!define /IfNDef LVM_GETITEMTEXTA 0x102D
!define /IfNDef LVM_GETITEMTEXTW 0x1073
!if "${NSIS_CHAR_SIZE}" > 1
!define /IfNDef LVM_GETITEMTEXT ${LVM_GETITEMTEXTW}
!else
!define /IfNDef LVM_GETITEMTEXT ${LVM_GETITEMTEXTA}
!endif

Function DumpLog
  Exch $5
  Push $0
  Push $1
  Push $2
  Push $3
  Push $4
  Push $6
  FindWindow $0 "#32770" "" $HWNDPARENT
  GetDlgItem $0 $0 1016
  StrCmp $0 0 exit
  FileOpen $5 $5 "w"
  StrCmp $5 "" exit
	SendMessage $0 ${LVM_GETITEMCOUNT} 0 0 $6
	System::Call '*(&t${NSIS_MAX_STRLEN})p.r3'
	StrCpy $2 0
	System::Call "*(i, i, i, i, i, p, i, i, i) i  (0, 0, 0, 0, 0, r3, ${NSIS_MAX_STRLEN}) .r1"
	loop: StrCmp $2 $6 done
	  System::Call "User32::SendMessage(p, i, p, p) i ($0, ${LVM_GETITEMTEXT}, $2, r1)"
	  System::Call "*$3(&t${NSIS_MAX_STRLEN} .r4)"
	  !ifdef DumpLog_As_UTF16LE
	  FileWriteUTF16LE ${DumpLog_As_UTF16LE} $5 "$4$\r$\n"
	  !else
	  FileWrite $5 "$4$\r$\n" ; Unicode will be translated to ANSI!
	  !endif
	  IntOp $2 $2 + 1
	  Goto loop
	done:
	  FileClose $5
	  System::Free $1
	  System::Free $3
  exit:
	Pop $6
	Pop $4
	Pop $3
	Pop $2
	Pop $1
	Pop $0
	Pop $5
FunctionEnd
!macro DumpLog file
	Push "${file}"
	Call DumpLog
!macroend
!define DumpLog `!insertmacro DumpLog`

; From https://nsis.sourceforge.io/IShellLink_Set_RunAs_flag
!ifndef IPersistFile
	!define IPersistFile {0000010b-0000-0000-c000-000000000046}
!endif
!ifndef CLSID_ShellLink
	!define CLSID_ShellLink {00021401-0000-0000-C000-000000000046}
	!define IID_IShellLinkA {000214EE-0000-0000-C000-000000000046}
	!define IID_IShellLinkW {000214F9-0000-0000-C000-000000000046}
	!define IShellLinkDataList {45e2b4ae-b1c3-11d0-b92f-00a0c90312e1}
	!ifdef NSIS_UNICODE
		!define IID_IShellLink ${IID_IShellLinkW}
	!else
		!define IID_IShellLink ${IID_IShellLinkA}
	!endif
!endif

Function ShellLinkSetRunAs
	System::Store S
	pop $9
	System::Call "ole32::CoCreateInstance(g'${CLSID_ShellLink}',i0,i1,g'${IID_IShellLink}',*i.r1)i.r0"
	${If} $0 = 0
		System::Call "$1->0(g'${IPersistFile}',*i.r2)i.r0" ;QI
		${If} $0 = 0
			System::Call "$2->5(w '$9',i 0)i.r0" ;Load
			${If} $0 = 0
				System::Call "$1->0(g'${IShellLinkDataList}',*i.r3)i.r0" ;QI
				${If} $0 = 0
					System::Call "$3->6(*i.r4)i.r0" ;GetFlags
					${If} $0 = 0
						System::Call "$3->7(i $4|0x2000)i.r0" ;SetFlags ;SLDF_RUNAS_USER
						${If} $0 = 0
							System::Call "$2->6(w '$9',i1)i.r0" ;Save
						${EndIf}
					${EndIf}
					System::Call "$3->2()" ;Release
				${EndIf}
			System::Call "$2->2()" ;Release
			${EndIf}
		${EndIf}
		System::Call "$1->2()" ;Release
	${EndIf}
	push $0
	System::Store L
FunctionEnd

Var ShellLinkSetRunAs.result
!macro ShellLinkSetRunAs path resvar
	Push `${path}`
	Call ShellLinkSetRunAs
	Pop $ShellLinkSetRunAs.result
	!if "${resvar}" != ""
		StrCpy "${resvar}" $ShellLinkSetRunAs.result
	!endif
!macroend
!define ShellLinkSetRunAs `!insertmacro ShellLinkSetRunAs`

!macro MessageBox options text sdreturn checks
	${If} ${Silent}
		${Log} `Message: "${text}"`
	${EndIf}

	${If} ${Quiet}
		MessageBox ${options} `${text}` ${checks}
	${Else}
		MessageBox ${options} `${text}` /SD ${sdreturn} ${checks}
	${Endif}
!MacroEnd
!define MessageBox `!insertmacro MessageBox`

!define STRFUNC_STALL_install ``
!define STRFUNC_STALL_uninstall `Un`
!define MyStrStr `${${STRFUNC_STALL_${stall}}StrStr}`

!macro GetToggleOption str opt var
	${GetOptions} `${str}` `${opt}` `${var}`
	${If} ${Errors}
		StrCpy ${var} false
	${ElseIf} `${var}` == ""
		StrCpy ${var} true
	${Else}
		${If} `${var}` != false
		${AndIf} `${var}` != true
		${AndIf} `${var}` != 0
		${AndIf} `${var}` != 1
			${MessageBox} MB_ICONSTOP "5 Wrong value for option ${opt}. It's '${var}' but must be one of: true, false, 0, 1. Aborting." IDOK ""
			Abort
		${EndIf}
	${EndIf}
!macroend
!define GetToggleOption `!insertmacro GetToggleOption`

; Call this at the top of .onInit and un.onInit.
Var ParseOptions.IsQuiet
Var ParseOptions.KeepService
Var ParseOptions.AdminTlsFingerprintsFile
!macro ParseOptions stall
	Push $R0
	Push $R1

	${GetParameters} $R0

	${GetToggleOption} $R0 "/quiet" $ParseOptions.IsQuiet
	${If} ${Quiet}
		SetSilent silent
	${Endif}

	${GetToggleOption} $R0 "/keepservice" $ParseOptions.KeepService

	${GetOptions} $R0 "/AdminTlsFingerprintsFile" $R1
	${If} ${Errors}
		StrCpy $ParseOptions.AdminTlsFingerprintsFile "$PLUGINSDIR\admin-tls-fingerprints.txt"
	${Else}
		StrCpy $ParseOptions.AdminTlsFingerprintsFile "$R1"
	${EndIf}

	;;;;;

	${MyStrStr} $0 "SYSDIR" " "
	${MyStrStr} $1 "SYSDIR" "$\t"
	${If} "$0" != ""
	${OrIf} "$1" != ""
		${MessageBox} MB_ICONSTOP|MB_OK "$$SYSDIR contains spaces, this is not supported.$\n$\nAborting installation." IDOK ""
		Abort
	${EndIf}

	Pop $R1
	Pop $R0
!macroend
!define ParseOptions `!insertmacro ParseOptions`

!endif

