Var V_AdminSettings_HowToStart
Var V_AdminSettings_StartCheck

Function fnc_AdminSettings_Init
    StrCpy $V_AdminSettings_HowToStart $(S_AdminSettings_StartLogonForAll)
    StrCpy $V_AdminSettings_StartCheck ${BST_CHECKED}
FunctionEnd

Function fnc_AdminSettings_OnCreate
    ${NSD_OnBack} fnc_AdminSettings_Remember
    ${NSD_SetState} $hCtl_AdminSettings_StartCheck $V_AdminSettings_StartCheck
    
    ${NSD_CB_AddString} $hCtl_AdminSettings_HowToStart "$(S_AdminSettings_StartLogonForAll)"
    ${NSD_CB_AddString} $hCtl_AdminSettings_HowToStart "$(S_AdminSettings_StartLogonForUser)"
    ${NSD_CB_AddString} $hCtl_AdminSettings_HowToStart "$(S_AdminSettings_StartManually)"
    ${NSD_CB_SelectString} $hCtl_AdminSettings_HowToStart "$V_AdminSettings_HowToStart"
FunctionEnd

Function fnc_AdminSettings_Remember
    ${NSD_GetText} $hCtl_AdminSettings_HowToStart $V_AdminSettings_HowToStart
    ${NSD_GetState} $hCtl_AdminSettings_StartCheck $V_AdminSettings_StartCheck
FunctionEnd

Function fnc_AdminSettings_Leave
    Call fnc_AdminSettings_Remember    
FunctionEnd
