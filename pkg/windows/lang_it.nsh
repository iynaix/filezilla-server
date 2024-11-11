!insertmacro MUI_LANGUAGE Italian

LangString S_IT_Full       ${LANG_ITALIAN} "Tutto"
LangString S_IT_ServerOnly ${LANG_ITALIAN} "Solo server"
LangString S_IT_AdminOnly  ${LANG_ITALIAN} "Solo amministrazione"

LangString S_SEC_Admin     ${LANG_ITALIAN} "Amministrazione"
LangString S_SEC_StartMenu ${LANG_ITALIAN} "Collegamenti nel menu"
LangString S_SEC_DeskIcons ${LANG_ITALIAN} "Icone sul desktop"

LangString S_LINK_Uninstall   ${LANG_ITALIAN} "Disinstalla ${PACKAGE_NAME}"
LangString S_LINK_Admin       ${LANG_ITALIAN} "Amministra ${PACKAGE_NAME}"
LangString S_LINK_StartServer ${LANG_ITALIAN} "Avvia ${PACKAGE_NAME}"
LangString S_LINK_StopServer  ${LANG_ITALIAN} "Ferma ${PACKAGE_NAME}"

LangString S_SEC_DESC_Server    ${LANG_ITALIAN} "Installa ${PACKAGE_NAME}."
LangString S_SEC_DESC_Admin     ${LANG_ITALIAN} "Installa l'interfaccia di amministrazione."
LangString S_SEC_DESC_StartMenu ${LANG_ITALIAN} "Create desktop icons for quick access to the Administration Interface "
LangString S_SEC_DESC_DeskIcons ${LANG_ITALIAN} "Crea le icone sul desktop."

LangString S_ServerSettings_Admin_ChoosePort            ${LANG_ITALIAN} "Scegli la &porta d'ascolto dell'interfaccia di amministrazione (1025-65535):"
LangString S_ServerSettings_Admin_ChoosePassword        ${LANG_ITALIAN} "Scegli la pa&ssword di amministrazione:"
LangString S_ServerSettings_Admin_RetypePassword        ${LANG_ITALIAN} "Ripeti la password scelta:"
LangString S_ServerSettings_Admin_PortNumberOutOfRange  ${LANG_ITALIAN} "Il numero della porta d'ascolto deve essere compreso tra 1025 e 65535"
LangString S_ServerSettings_Admin_PasswordsMustMatch    ${LANG_ITALIAN} "Le password inserite non corrispondono"
LangString S_ServerSettings_Admin_PasswordsIsEmpty      ${LANG_ITALIAN} "Non è stata inserita alcuna password di amministrazione: è una cosa pericolosa.$\n$\nSenza una password di amministrazione, qualunque utente o programma esistente nel computer potrebbe cambiare la configurazione di ${PACKAGE_NAME} e potenzialmente guadagnare i privilegi di Administrator della macchina su cui è installato ${PACKAGE_NAME}.$\n$\nVuoi ancora procedere senza una password di amministrazione?"
LangString S_ServerSettings_Admin_PasswordsIsWeak       ${LANG_ITALIAN} "La password scelta non soddisfa i criteri di sicurezza raccomandati.$\n$\nPer una sicurezza ottimale, la tua password dovrebbe includere quanto segue:$\n$\n  1. Una lunghezza minima di 12 caratteri.$\n  2. Almeno un numero (0-9).$\n  3. Almeno un carattere speciale (ad es., !, @, #, $$).$\n  4. Almeno una lettera maiuscola (A-Z).$\n  5. Almeno una lettera minuscola (a-z).$\n$\nConsidera l'utilizzo di una serie di parole o una frase, in quanto ciò può rendere la password sia sicura che facile da ricordare.$\n$\nVuoi procedere con la scelta della password attuale?"
LangString S_ServerSettings_Admin_ListenOnAllInterfaces ${LANG_ITALIAN} "Consenti connessioni di amministrazione su tutti gli adattatori di rete (password necessaria)."

LangString S_ServiceSettings_HowToInstallAndStart ${LANG_ITALIAN} "Scegli &come installare e avviare ${PACKAGE_NAME}:"
LangString S_ServiceSettings_AsServiceAutoStart   ${LANG_ITALIAN} "Installa come servizio, avvia con Windows (default)"
LangString S_ServiceSettings_AsServiceManualStart ${LANG_ITALIAN} "Installa come servizio, avvia manualmente"
LangString S_ServiceSettings_AsProgramManualStart ${LANG_ITALIAN} "Non installare come servizio, avvia manualmente (non raccomandato)"
LangString S_ServiceSettings_KeepOldConfig        ${LANG_ITALIAN} "Mantieni le impostazioni del servizio esistenti"
LangString S_ServiceSettings_StartCheck           ${LANG_ITALIAN} "Avvia il server alla fine dell'installazione"

LangString S_ServiceSettings_RunAsSYSTEM           ${LANG_ITALIAN} "Esegui il servizio con l'account utente &SYSTEM di Windows."
LangString S_ServiceSettings_RunAsOtherUser        ${LANG_ITALIAN} "Esegui il servizio con un altro account utente di Windows:"
LangString S_ServiceSettings_ChooseServiceUser     ${LANG_ITALIAN} "Nome utente dell'account:"
LangString S_ServiceSettings_ChooseServicePassword ${LANG_ITALIAN} "Password dell'account:"

LangString S_ServiceSettings_ServiceUserIsEmpty ${LANG_ITALIAN} "È necessario inserire un nome utente per un account utente Windows oppure scegliere l'account utente SYSTEM."
LangString S_ServiceSettings_ServiceUserInvalid ${LANG_ITALIAN} "L'utente specificato non esiste, oppure non possiede i prilegi di logon come servizio, oppure la password non è corretta."

LangString S_AdminSettings_ChooseHowToStart     ${LANG_ITALIAN} "Scegli &come avviare l'interfaccia di amministrazione:"
LangString S_AdminSettings_StartLogonForAll     ${LANG_ITALIAN} "Avvia automaticamente al log on di qualunque utente (default)"
LangString S_AdminSettings_StartLogonForUser    ${LANG_ITALIAN} "Avvia automaticamente al log on del solo utente corrente"
LangString S_AdminSettings_StartManually        ${LANG_ITALIAN} "Avvia manualmente"
LangString S_AdminSettings_StartCheck           ${LANG_ITALIAN} "&Avvia l'interfaccia di amministrazione alla fine dell'installazione"

LangString S_PRE_Uninstalling ${LANG_ITALIAN} "Rimuovo la precedente installazione di ${PACKAGE_NAME}"
LangString S_PRE_CloseGuiToContinue ${LANG_ITALIAN} "L'interfaccia di amministrazione di ${PACKAGE_NAME} sembra essere attiva. Assicurati che sia chiusa e riprova."

LangString S_Post_Server_AdminTlsFingerprints ${LANG_ITALIAN} "Prendi nota delle fingeprint TLS dell'interfaccia di amministrazione di ${PACKAGE_NAME}:"
LangString S_Post_Server_ServiceNotExisting   ${LANG_ITALIAN} "Hai scelto di mantenere la precedente configurazione del servizio di ${PACKAGE_NAME}, ma il servizio non esiste. Per piacere, riavvia l'installazione."

LangString S_FILE_CannotDelete ${LANG_ITALIAN} "Errore nel cancellare i file. Assicurati che l'interfaccia di amministrazione di ${PACKAGE_NAME} sia chiusa e riprova."
LangString S_FILE_CannotWrite  ${LANG_ITALIAN} "Errore nel creare i file. Assicurati che l'interfaccia di amministrazione di ${PACKAGE_NAME} sia chiusa e riprova."

LangString S_CONFIG_CannotReadResult ${LANG_ITALIAN} "Errore nella lettura del risultato del controllo della versione dei file di configurazione. L'installazione non proseguirà."
LangString S_CONFIG_UnknownResult ${LANG_ITALIAN} "Il risultato del controllo della versione dei file di configurazione è sconosciuto. L'installazione non proseguirà."
LangString S_CONFIG_VersionError ${LANG_ITALIAN} "Uno o più file di configurazione già esistenti sono stati generati da una versione di ${PACKAGE_NAME} che non è garantito sia compatibile.$\n$\n${PACKAGE_NAME} può comunque caricarli, ma parte delle informazioni contenute potrebbero andare perdute.$\n$\nVuoi effettuare una copia di sicurezza dei file di configurazione esistenti?"
LangString S_CONFIG_BackupOk ${LANG_ITALIAN} "Una copia di sicurezza dei file di configurazione è stata correttamente effettuata.$\n$\nL'installazione può ora continuare.$\n$\nDi seguito la lista dei file:"
LangString S_CONFIG_BackupKo ${LANG_ITALIAN} "Non è stato possibile effettuare una copia di sicurezza dei file di configurazione, controlla che ci sia sufficiente spazio sul disco e che i permessi delle cartelle siano giusti.$\n$\nL'installazione verrà interrotta."
LangString S_CONFIG_CannotReadOwnershipResult ${LANG_ITALIAN} "Errore nella lettura del risultato del controllo di proprietà della cartella di configurazione. L'installazione non proseguirà."
LangString S_CONFIG_WrongOwnership ${LANG_ITALIAN} "Ci sono problemi con la cartella di configurazione, impossibile procedere. Risolvi i seguenti problemi e riavvia l'installazione."

LangString S_INIT_UnsupportedOS ${LANG_ITALIAN} "Sistema operativo non supportato.$\n${PACKAGE_NAME} richiede almeno Windows 7."
LangString S_INIT_Unsupported32 ${LANG_ITALIAN} "Sistema operativo non supportato.$\n${PACKAGE_NAME} è a 64bit e non funziona sul tuo sistema operativo, che è a 32bit.$\nVuoi davvero continuare con l'installazione?"

LangString S_INSTDIR_WARNING ${LANG_ITALIAN} `Hai selezionato una cartella di installazione che non è una sottocartella di "$PROGRAMFILES64."$\n$\nSe questa è una scelta intenzionale, dovrai verificare e regolare manualmente i permessi per questa cartella e per le sue cartelle superiori, assicurandoti che solo l'account del servizio server e gli Amministratori abbiano accesso in scrittura.$\n$\nIn caso contrario, il server potrebbe essere vulnerabile ad attacchi di escalation dei privilegi.$\n$\nSei sicuro di voler procedere?`

