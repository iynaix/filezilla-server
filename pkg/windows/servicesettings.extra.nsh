Var V_ServiceSettings_HowToInstallAndStart
Var V_ServiceSettings_StartCheck
Var V_ServiceSettings_RunAsSYSTEM
Var V_ServiceSettings_RunAsOtherUser
Var V_ServiceSettings_ServiceUserSaved
Var V_ServiceSettings_ServicePasswordSaved
Var V_ServiceSettings_ServiceUser
Var V_ServiceSettings_ServicePassword
Var V_ServiceSettings_CanKeepOldConfig
Var V_ServiceSettings_MustKeepOldConfig
Var V_ServiceSettings_OldServiceVersionMajor
Var V_ServiceSettings_OldServiceVersionMinor
Var V_ServiceSettings_OldServiceVersionMicro
Var V_ServiceSettings_OldServiceInstdir

!macro check_version_greater_than_or_equal_to a b c res
	${If} $V_ServiceSettings_OldServiceVersionMajor > ${a}
		StrCpy ${res} true
	${ElseIf} $V_ServiceSettings_OldServiceVersionMajor == ${a}
		${If} $V_ServiceSettings_OldServiceVersionMinor > ${b}
			StrCpy ${res} true
		${ElseIf} $V_ServiceSettings_OldServiceVersionMinor == ${b}
			${If} $V_ServiceSettings_OldServiceVersionMicro >= ${c}
				StrCpy ${res} true
			${Else}
				StrCpy ${res} false
			${EndIf}
		${Else}
			StrCpy ${res} false
		${EndIf}
	${Else}
		StrCpy ${res} false
	${EndIf}
!macroend
!define check_version_greater_than_or_equal_to `!insertmacro check_version_greater_than_or_equal_to`

Function fnc_ServiceSettings_Init
	StrCpy $V_ServiceSettings_CanKeepOldConfig false

	${If} ${Silent}
		${If} ${ServiceExists} filezilla-server
			${check_version_greater_than_or_equal_to} 1 6 0 $V_ServiceSettings_CanKeepOldConfig
		${EndIf}
	${EndIf}

	${If} ${AsBool} $V_ServiceSettings_CanKeepOldConfig
	${AndIf} "$V_ServiceSettings_OldServiceInstdir" != "$INSTDIR"
		;Previous installation directory is different than current one: cannot keep old config
		StrCpy $V_ServiceSettings_CanKeepOldConfig false
	${EndIf}

	StrCpy $V_ServiceSettings_MustKeepOldConfig $V_ServiceSettings_CanKeepOldConfig

	${If} ${AsBool} $V_ServiceSettings_CanKeepOldConfig
		StrCpy $V_ServiceSettings_HowToInstallAndStart "$(S_ServiceSettings_KeepOldConfig)"
	${Else}
		StrCpy $V_ServiceSettings_HowToInstallAndStart "$(S_ServiceSettings_AsServiceAutoStart)"
	${EndIf}

	StrCpy $V_ServiceSettings_StartCheck ${BST_CHECKED}
	StrCpy $V_ServiceSettings_RunAsSYSTEM  ${BST_CHECKED}
	StrCpy $V_ServiceSettings_RunAsOtherUser  ${BST_UNCHECKED}
	StrCpy $V_ServiceSettings_ServiceUserSaved ""
	StrCpy $V_ServiceSettings_ServicePasswordSaved ""
FunctionEnd

Function fnc_ServiceSettings_OnCreate
	${NSD_OnBack} fnc_ServiceSettings_Remember

	${If} ${ServiceExists} filezilla-server
		${check_version_greater_than_or_equal_to} 1 6 0 $V_ServiceSettings_CanKeepOldConfig
	${EndIf}

	${If} ${AsBool} $V_ServiceSettings_CanKeepOldConfig
	${AndIf} "$V_ServiceSettings_OldServiceInstdir" != "$INSTDIR"
		;Previous installation directory is different than current one: cannot keep old config
		StrCpy $V_ServiceSettings_CanKeepOldConfig false
	${EndIf}

	${If} ${AsBool} $V_ServiceSettings_CanKeepOldConfig
		StrCpy $V_ServiceSettings_HowToInstallAndStart "$(S_ServiceSettings_KeepOldConfig)"
	${Else}
		StrCpy $V_ServiceSettings_HowToInstallAndStart "$(S_ServiceSettings_AsServiceAutoStart)"
	${EndIf}

	${NSD_SetState} $hCtl_ServiceSettings_StartCheck $V_ServiceSettings_StartCheck
	${NSD_CB_AddString} $hCtl_ServiceSettings_HowToInstallAndStart "$(S_ServiceSettings_AsServiceAutoStart)"
	${NSD_CB_AddString} $hCtl_ServiceSettings_HowToInstallAndStart "$(S_ServiceSettings_AsServiceManualStart)"
	;${NSD_CB_AddString} $hCtl_ServerSettings_HowToInstallAndStart "$(S_ServerSettings_AsProgramManualStart)"
	${If} ${AsBool} $V_ServiceSettings_CanKeepOldConfig
		${NSD_CB_AddString} $hCtl_ServiceSettings_HowToInstallAndStart "$(S_ServiceSettings_KeepOldConfig)"
	${EndIf}

	${NSD_CB_SelectString} $hCtl_ServiceSettings_HowToInstallAndStart "$V_ServiceSettings_HowToInstallAndStart"

	${NSD_SetState} $hCtl_ServiceSettings_RunAsSYSTEM $V_ServiceSettings_RunAsSYSTEM
	${NSD_SetState} $hCtl_ServiceSettings_RunAsOtherUser $V_ServiceSettings_RunAsOtherUser
	${NSD_SetText} $hCtl_ServiceSettings_ServiceUser $V_ServiceSettings_ServiceUserSaved
	${NSD_SetText} $hCtl_ServiceSettings_ServicePassword $V_ServiceSettings_ServicePasswordSaved

	Push $hCtl_ServiceSettings_HowToInstallAndStart
	Call fnc_ServiceSettings_HowToInstallAndStartChanged
FunctionEnd

Function fnc_ServiceSettings_Remember
	${NSD_GetText} $hCtl_ServiceSettings_HowToInstallAndStart $V_ServiceSettings_HowToInstallAndStart
	${NSD_GetState} $hCtl_ServiceSettings_StartCheck $V_ServiceSettings_StartCheck

	${NSD_GetState} $hCtl_ServiceSettings_RunAsSYSTEM $V_ServiceSettings_RunAsSYSTEM
	${NSD_GetState} $hCtl_ServiceSettings_RunAsOtherUser $V_ServiceSettings_RunAsOtherUser
	${NSD_GetText} $hCtl_ServiceSettings_ServiceUser $V_ServiceSettings_ServiceUserSaved
	${NSD_GetText} $hCtl_ServiceSettings_ServicePassword $V_ServiceSettings_ServicePasswordSaved

	${If} $V_ServiceSettings_HowToInstallAndStart == "$(S_ServiceSettings_KeepOldConfig)"
		StrCpy $V_ServiceSettings_MustKeepOldConfig true
		StrCpy $V_ServiceSettings_ServiceUser ""
		StrCpy $V_ServiceSettings_ServicePassword ""		
	${Else}
		StrCpy $V_ServiceSettings_MustKeepOldConfig false
		${If} ${AsBool} $V_ServiceSettings_RunAsSYSTEM
			StrCpy $V_ServiceSettings_ServiceUser ""
			StrCpy $V_ServiceSettings_ServicePassword ""
		${Else}
			StrCpy $V_ServiceSettings_ServiceUser $V_ServiceSettings_ServiceUserSaved
			StrCpy $V_ServiceSettings_ServicePassword $V_ServiceSettings_ServicePasswordSaved
		${EndIf}
	${EndIf}
FunctionEnd

Function fnc_ServiceSettings_HowToInstallAndStartChanged
	Pop $0

	${NSD_GetText} $hCtl_ServiceSettings_HowToInstallAndStart $0
	${If} $0 != $(S_ServiceSettings_KeepOldConfig)
		StrCpy $0 1
	${Else}
		StrCpy $0 0
	${EndIf}
	
	ShowWindow $hCtl_ServiceSettings_RunAsSYSTEM $0
	ShowWindow $hCtl_ServiceSettings_RunAsOtherUser $0
	ShowWindow $hCtl_ServiceSettings_ChooseServiceUserLabel $0
	ShowWindow $hCtl_ServiceSettings_ServiceUser $0
	ShowWindow $hCtl_ServiceSettings_ChooseServicePasswordLabel $0
	ShowWindow $hCtl_ServiceSettings_ServicePassword $0

	Push $hCtl_ServiceSettings_RunAsOtherUser
	Call fnc_ServiceSettings_RunAsChanged
FunctionEnd

Function fnc_ServiceSettings_RunAsChanged
	Pop $0
	${NSD_GetState} $hCtl_ServiceSettings_RunAsOtherUser $0
	EnableWindow $hCtl_ServiceSettings_ChooseServiceUserLabel $0
	EnableWindow $hCtl_ServiceSettings_ServiceUser $0
	EnableWindow $hCtl_ServiceSettings_ChooseServicePasswordLabel $0
	EnableWindow $hCtl_ServiceSettings_ServicePassword $0
FunctionEnd


Function fnc_ServiceSettings_Leave
	Push $0
	Push $1

	${NSD_GetState} $hCtl_ServiceSettings_RunAsOtherUser $0
	${If} $0 == ${BST_CHECKED}
		${NSD_GetText} $hCtl_ServiceSettings_ServiceUser $0
		${If} "$0" == ""
			${MessageBox} MB_ICONEXCLAMATION "$(S_ServiceSettings_ServiceUserIsEmpty)" IDOK ""
			Abort
		${EndIf}

		${NSD_GetText} $hCtl_ServiceSettings_ServiceUser $0
		${NSD_GetText} $hCtl_ServiceSettings_ServicePassword $1

		${CheckIfUserCanLogin} "$0" "$1" LOGON32_LOGON_SERVICE
		${If} ${Errors}
			${MessageBox} MB_ICONEXCLAMATION "$(S_ServiceSettings_ServiceUserInvalid)" IDOK ""
			Abort
		${EndIf}
	${EndIf}

	Pop $1
	Pop $0

	Call fnc_ServiceSettings_Remember
FunctionEnd
