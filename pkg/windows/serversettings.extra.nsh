Var V_ServerSettings_Admin_PortNumber
Var V_ServerSettings_Admin_Password
Var V_ServerSettings_Admin_Password2
Var V_ServerSettings_Admin_ListenAll

Function fnc_ServerSettings_Init
	StrCpy $V_ServerSettings_Admin_PortNumber "14148"
FunctionEnd

Function fnc_ServerSettings_OnCreate
	${NSD_OnBack} fnc_ServerSettings_Remember

	${NSD_SetText} $hCtl_ServerSettings_Admin_Password1 $V_ServerSettings_Admin_Password
	${NSD_SetText} $hCtl_ServerSettings_Admin_Password2 $V_ServerSettings_Admin_Password2
	${NSD_SetText} $hCtl_ServerSettings_Admin_PortNumber $V_ServerSettings_Admin_PortNumber
    ${NSD_SetState} $hCtl_ServerSettings_Admin_ListenAll $V_ServerSettings_Admin_ListenAll

    Push $hCtl_ServerSettings_Admin_Password1
    Call fnc_ServerSettings_Admin_PasswordChanged
FunctionEnd

Function fnc_ServerSettings_Remember
	${NSD_GetText} $hCtl_ServerSettings_Admin_Password1 $V_ServerSettings_Admin_Password
	${NSD_GetText} $hCtl_ServerSettings_Admin_Password2 $V_ServerSettings_Admin_Password2
	${NSD_GetText} $hCtl_ServerSettings_Admin_PortNumber $V_ServerSettings_Admin_PortNumber
	${NSD_GetState} $hCtl_ServerSettings_Admin_ListenAll $V_ServerSettings_Admin_ListenAll
FunctionEnd

!define /IfNDef PWD_CRIT_LENGTH  1
!define /IfNDef PWD_CRIT_NUMBER  2
!define /IfNDef PWD_CRIT_SPECIAL 4
!define /IfNDef PWD_CRIT_UPPER   8
!define /IfNDef PWD_CRIT_LOWER   16
!define /IfNDef PWD_CRIT_ALL     31

Function fnc_ServerSettings_Leave
	Push $R0 ; The password
	Push $R1 ; The copy of the password
    Push $R2 ; The met security criteria
    Push $R3 ; Password length and position
    Push $R4 ; Current character in password

	${NSD_GetText} $hCtl_ServerSettings_Admin_PortNumber $R0
	${If} $R0 < 1025
	${OrIf} $R0 > 65535
		${MessageBox} MB_ICONEXCLAMATION $(S_ServerSettings_Admin_PortNumberOutOfRange) IDOK ""
		Abort
	${EndIf}

	${NSD_GetText} $hCtl_ServerSettings_Admin_Password1 $R0
	${NSD_GetText} $hCtl_ServerSettings_Admin_Password2 $R1

	${If} $R0 != $R1
		${MessageBox} MB_ICONEXCLAMATION "$(S_ServerSettings_Admin_PasswordsMustMatch)" IDOK ""
		Abort
	${EndIf}

	${If} $R0 == ""
		${MessageBox} MB_ICONEXCLAMATION|MB_YESNO|MB_DEFBUTTON2 "$(S_ServerSettings_Admin_PasswordsIsEmpty)" IDYES "IDYES accept_empty_password"
		Abort
	accept_empty_password:
    ${Else}
        StrCpy $R2 0 ; Reset the criteria met flag
        StrLen $R3 $R0 ; Password length in $R3

        ${If} $R3 >= 12
            IntOp $R2 $R2 | ${PWD_CRIT_LENGTH}
        ${EndIf}

        IntOp $R3 $R3 - 1 ;  $R3 is now the be the last position index
        
        ${While} $R3 >= 0
            StrCpy $R4 $R0 1 $R3 ; Get character in $R4 at index $R3
            IntOp $R3 $R3 - 1 ; Move backwards

            ${If} $R4 >= '0' 
            ${AndIf} $R4 <= '9'
                IntOp $R2 $R2 | ${PWD_CRIT_NUMBER}
            ${EndIf}

            ${If} $R4 >= 'A'
            ${AndIf} $R4 <= 'Z'
                IntOp $R2 $R2 | ${PWD_CRIT_UPPER}
            ${EndIf}

            ${If} $R4 >= 'a'
            ${AndIf} $R4 <= 'z'
                IntOp $R2 $R2 | ${PWD_CRIT_LOWER}
            ${EndIf}

            # Check for special characters by ranges
            ${If} $R4 >= '!' 
            ${AndIf} $R4 <= '/'
                IntOp $R2 $R2 | ${PWD_CRIT_SPECIAL}
            ${EndIf}

            ${If} $R4 >= ':' 
            ${AndIf} $R4 <= '@'
                IntOp $R2 $R2 | ${PWD_CRIT_SPECIAL}
            ${EndIf}

            ${If} $R4 >= '[' 
            ${AndIf} $R4 <= 96 ; backtick
                IntOp $R2 $R2 | ${PWD_CRIT_SPECIAL}
            ${EndIf}

            ${If} $R4 >= '{' 
            ${AndIf} $R4 <= '~'
                IntOp $R2 $R2 | ${PWD_CRIT_SPECIAL}
            ${EndIf}
        ${EndWhile}

        ${If} $R2 != ${PWD_CRIT_ALL}
		    ${MessageBox} MB_ICONEXCLAMATION|MB_YESNO|MB_DEFBUTTON2 "$(S_ServerSettings_Admin_PasswordsIsWeak)" IDYES "IDYES accept_weak_password"
		    Abort
	    accept_weak_password:
	    ${EndIf}
	${EndIf}

    Pop $R4
    Pop $R3
    Pop $R2
	Pop $R1
	Pop $R0

	Call fnc_ServerSettings_Remember
FunctionEnd

Function fnc_ServerSettings_Admin_PasswordChanged
    Pop $R0

	${NSD_GetText} $hCtl_ServerSettings_Admin_Password1 $0
    ${If} $0 != ""
        EnableWindow $hCtl_ServerSettings_Admin_ListenAll 1
    ${Else}
        EnableWindow $hCtl_ServerSettings_Admin_ListenAll 0
    	${NSD_SetState} $hCtl_ServerSettings_Admin_ListenAll 0
    ${EndIf}
FunctionEnd

