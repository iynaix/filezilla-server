!insertmacro MUI_LANGUAGE "English"

LangString S_IT_Full       ${LANG_ENGLISH} "Full"
LangString S_IT_ServerOnly ${LANG_ENGLISH} "Server only"
LangString S_IT_AdminOnly  ${LANG_ENGLISH} "Administration only"

LangString S_SEC_Admin     ${LANG_ENGLISH} "Administration interface"
LangString S_SEC_StartMenu ${LANG_ENGLISH} "Start menu shortcuts"
LangString S_SEC_DeskIcons ${LANG_ENGLISH} "Desktop icons"

LangString S_LINK_Uninstall   ${LANG_ENGLISH} "Uninstall ${PACKAGE_NAME}"
LangString S_LINK_Admin       ${LANG_ENGLISH} "Administer ${PACKAGE_NAME}"
LangString S_LINK_StartServer ${LANG_ENGLISH} "Start ${PACKAGE_NAME}"
LangString S_LINK_StopServer  ${LANG_ENGLISH} "Stop ${PACKAGE_NAME}"

LangString S_SEC_DESC_Server    ${LANG_ENGLISH} "Copy ${PACKAGE_NAME} to the application folder."
LangString S_SEC_DESC_Admin     ${LANG_ENGLISH} "Copy the administration interface to the application folder."
LangString S_SEC_DESC_StartMenu ${LANG_ENGLISH} "Create shortcuts to ${PACKAGE_NAME} and Administration Interface in the Start Menu"
LangString S_SEC_DESC_DeskIcons ${LANG_ENGLISH} "Create desktop icons for quick access to the Administration Interface "

LangString S_ServerSettings_Admin_ChoosePort            ${LANG_ENGLISH} "Choose the listening &port for the administration interface (1025-65535):"
LangString S_ServerSettings_Admin_ChoosePassword        ${LANG_ENGLISH} "Choose the administration pass&word:"
LangString S_ServerSettings_Admin_RetypePassword        ${LANG_ENGLISH} "&Retype the chosen password:"
LangString S_ServerSettings_Admin_PortNumberOutOfRange  ${LANG_ENGLISH} "The listening port must range from 1025 to 65535"
LangString S_ServerSettings_Admin_PasswordsMustMatch    ${LANG_ENGLISH} "Input passwords don't match"
LangString S_ServerSettings_Admin_PasswordsIsEmpty      ${LANG_ENGLISH} "No administration password has been provided, this is dangerous.$\n$\nWithout an administration password, any user and any program on your computer will be able to make changes to ${PACKAGE_NAME} and potentially gain Administrator privileges on the machine ${PACKAGE_NAME} is running on.$\n$\nDo you still want to proceed without an administration password?"
LangString S_ServerSettings_Admin_PasswordsIsWeak       ${LANG_ENGLISH} "The chosen password does not meet the recommended security criteria.$\n$\nFor optimal security, your password should include the following:$\n$\n  1. A minimum length of 12 characters.$\n  2. At least one numeral (0-9).$\n  3. At least one special character (e.g., !, @, #, $$). $\n  4. At least one uppercase letter (A-Z).$\n  5. At least one lowercase letter (a-z). $\n$\nConsider using a sequence of words or a sentence, as it can be both secure and memorable.$\n$\nDo you wish to proceed with the current password choice?"
LangString S_ServerSettings_Admin_ListenOnAllInterfaces ${LANG_ENGLISH} "Allow administration connections on all network adapters (requires password)."

LangString S_ServiceSettings_HowToInstallAndStart ${LANG_ENGLISH} "Choose &how ${PACKAGE_NAME} should be installed and started:"
LangString S_ServiceSettings_AsServiceAutoStart   ${LANG_ENGLISH} "Install as service, started with Windows (default)"
LangString S_ServiceSettings_AsServiceManualStart ${LANG_ENGLISH} "Install as service, start manually"
LangString S_ServiceSettings_AsProgramManualStart ${LANG_ENGLISH} "Do not install as service, start manually (not recommended)"
LangString S_ServiceSettings_KeepOldConfig        ${LANG_ENGLISH} "Keep existing service settings"
LangString S_ServiceSettings_StartCheck           ${LANG_ENGLISH} "Start server after se&tup completes"

LangString S_ServiceSettings_RunAsSYSTEM       ${LANG_ENGLISH} "Run service under the &SYSTEM Windows user account"
LangString S_ServiceSettings_RunAsOtherUser    ${LANG_ENGLISH} "Run service under a &different Windows user account:"
LangString S_ServiceSettings_ChooseServiceUser ${LANG_ENGLISH} "Account user&name:"
LangString S_ServiceSettings_ChooseServicePassword ${LANG_ENGLISH} "Account pass&word:"

LangString S_ServiceSettings_ServiceUserIsEmpty ${LANG_ENGLISH} "You must input a user name for a Windows user accout, or choose the SYSTEM user account."
LangString S_ServiceSettings_ServiceUserInvalid ${LANG_ENGLISH} "Provided user does not exist, or does not have the rights to logon as service, or the password is incorrect."

LangString S_AdminSettings_ChooseHowToStart  ${LANG_ENGLISH} "Choose &how the administration interface should be started:"
LangString S_AdminSettings_StartLogonForAll  ${LANG_ENGLISH} "Start if user logs on, apply to all users (default)"
LangString S_AdminSettings_StartLogonForUser ${LANG_ENGLISH} "Start if user logs on, apply only to current user"
LangString S_AdminSettings_StartManually     ${LANG_ENGLISH} "Start manually"
LangString S_AdminSettings_StartCheck        ${LANG_ENGLISH} "Start &administration interface after setup completes"

LangString S_PRE_Uninstalling ${LANG_ENGLISH} "Removing previous ${PACKAGE_NAME} installation"
LangString S_PRE_CloseGuiToContinue ${LANG_ENGLISH} "The ${PACKAGE_NAME} Administration Interface seems to be running. Make sure to close it and try again."

LangString S_Post_Server_AdminTlsFingerprints ${LANG_ENGLISH} "Take note of the ${PACKAGE_NAME} Administration Interface TLS fingerprints:"
LangString S_Post_Server_ServiceNotExisting   ${LANG_ENGLISH} "You choose to keep the previous ${PACKAGE_NAME} service configuration, but the service does not exist. Try running the installer again."

LangString S_FILE_CannotDelete ${LANG_ENGLISH} "Cannot delete files. Please make sure the ${PACKAGE_NAME} Administration Interface is closed and try again."
LangString S_FILE_CannotWrite  ${LANG_ENGLISH} "Cannot create files. Please make sure the ${PACKAGE_NAME} Administration Interface is closed and try again."

LangString S_CONFIG_CannotReadResult ${LANG_ENGLISH} "Cannot read result of the configuration version check, something is wrong. Aborting installation."
LangString S_CONFIG_UnknownResult ${LANG_ENGLISH} "Cannot understand the result of the configuration version check, something is wrong. Aborting installation."
LangString S_CONFIG_VersionError ${LANG_ENGLISH} "One or more existing configuration files belong to a version of ${PACKAGE_NAME} which is not guaranteed to be compatible.$\n$\n${PACKAGE_NAME} can still load them up, but some information might be lost.$\n$\nDo you want to make a backup of the files?"
LangString S_CONFIG_BackupOk ${LANG_ENGLISH} "Configuration files were correctly backed up.$\n$\nInstallation can now proceed as normal.$\n$\nThese are the backed up files:"
LangString S_CONFIG_BackupKo ${LANG_ENGLISH} "Unable to make a backup of the configuration files, please check folder permissions and there's available disk space.$\n$\nAborting installation."
LangString S_CONFIG_CannotReadOwnershipResult ${LANG_ENGLISH} "Cannot read result of the configuration dir ownership check, something is wrong. Aborting installation."
LangString S_CONFIG_WrongOwnership ${LANG_ENGLISH} "There are issues with the configuration folder, cannot proceed. Fix the following issues and restart the installation."

LangString S_INIT_UnsupportedOS ${LANG_ENGLISH} "Unsupported operating system.$\n${PACKAGE_NAME} requires at least Windows 7."
LangString S_INIT_Unsupported32 ${LANG_ENGLISH} "Unsupported operating system.$\n${PACKAGE_NAME} is 64bit and does not run on your operating system which is only 32bit.$\nDo you really want to continue with the installation?"

LangString S_INSTDIR_WARNING ${LANG_ENGLISH} `You've selected an installation folder that is not a subfolder of "$PROGRAMFILES64."$\n$\nIf this is intentional, you will need to manually verify and adjust the permissions for this folder and its parent folders to ensure only the server service account and Administrators have write access.$\n$\nNot doing so could leave the server vulnerable to privilege escalation attacks.$\n$\nAre you sure you want to proceed?`
