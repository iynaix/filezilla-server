#!/bin/bash
	
what="$1"
from="$2"
shift 2

DUMP_KEY="SOFTWARE\Microsoft\Windows\Windows Error Reporting\LocalDumps"

: > "${what}_install.nsh"
: > "${what}_uninstall.nsh"

while read dep; do
    echo 'File "files\'$dep'"' >> "${what}_install.nsh"
    
    echo 'ClearErrors'                    >> "${what}_uninstall.nsh";
    echo 'Delete "$INSTDIR\'$dep'"'       >> "${what}_uninstall.nsh";
    echo '${If} ${Errors}'                >> "${what}_uninstall.nsh"; 
    echo '    StrCpy $Delete.errors true' >> "${what}_uninstall.nsh";
    echo '${EndIf}'                       >> "${what}_uninstall.nsh"; 

    if [[ "$dep" == *.exe ]]; then
	    echo 'WriteRegDWORD HKLM "'${DUMP_KEY}'\'$dep'" "DumpType" "1"' >> "${what}_install.nsh"
	    echo 'DeleteRegValue HKLM "'${DUMP_KEY}'\'$dep'" "DumpType"' >> "${what}_uninstall.nsh"
	    echo 'DeleteRegKey /ifempty HKLM "'${DUMP_KEY}'\'$dep'"' >> "${what}_uninstall.nsh"
	fi
done < "$from"

