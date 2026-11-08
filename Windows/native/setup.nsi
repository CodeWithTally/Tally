Unicode True
!include "MUI2.nsh"
!include "x64.nsh"
!include "LogicLib.nsh"
!include "nsDialogs.nsh"
!include "WinMessages.nsh"
!include "WordFunc.nsh"
!insertmacro VersionCompare
!ifndef TALLY_VERSION
  !error "Missing -DTALLY_VERSION (run native/build-windows.py)"
!endif
!ifndef TALLY_FILE_VERSION
  !error "Missing -DTALLY_FILE_VERSION (run native/build-windows.py)"
!endif
!ifndef OUTPUT
  !error "Missing -DOUTPUT (run native/build-windows.py)"
!endif
!ifndef PAYLOAD
!error "Pass -DPAYLOAD=/absolute/path/to/payload"
!endif
Name "Tally ${TALLY_VERSION}"
OutFile "${OUTPUT}"
InstallDir "$LOCALAPPDATA\Programs\Tally"
InstallDirRegKey HKCU "Software\TallyLanguage" "InstallDir"
RequestExecutionLevel user
SetCompressor /SOLID lzma
VIProductVersion "${TALLY_FILE_VERSION}"
VIAddVersionKey "ProductName" "Tally"
VIAddVersionKey "FileDescription" "Tally programming language setup"
VIAddVersionKey "FileVersion" "${TALLY_VERSION}"
VIAddVersionKey "LegalCopyright" "Tally project"
; Asset directory is passed as an ABSOLUTE path by build-windows.py.
; __FILEDIR__ may be relative ("native"), so using it here can accidentally
; resolve to native\native\assets when makensis compiles native/setup.nsi.
!ifndef TALLY_ASSET_DIR
  !error "Missing -DTALLY_ASSET_DIR=absolute-path-to-native/assets (run python native/build-windows.py)"
!endif
!define MUI_ICON "${TALLY_ASSET_DIR}\tally.ico"
!define MUI_UNICON "${TALLY_ASSET_DIR}\tally.ico"
!define MUI_WELCOMEFINISHPAGE_BITMAP "${TALLY_ASSET_DIR}\tally-wizard.bmp"
!define MUI_HEADERIMAGE
!define MUI_HEADERIMAGE_RIGHT
!define MUI_HEADERIMAGE_BITMAP "${TALLY_ASSET_DIR}\tally-header.bmp"
!define MUI_BGCOLOR "F7F3FF"
!define MUI_TEXTCOLOR "17141C"
!define MUI_BRANDINGTEXT "TALLY  /  ${TALLY_VERSION}    -    MADE TO MAKE"
!define MUI_ABORTWARNING
!define MUI_WELCOMEPAGE_TITLE "Your ideas. Now running."
!define MUI_WELCOMEPAGE_TEXT "A little language for big ideas.$\r$\n$\r$\nWrite code that moves, draws, and makes things happen. Tally installs in seconds and runs on its own.$\r$\n$\r$\nNative Windows program. No Node.js, Python, or browser required to run it."
!define MUI_FINISHPAGE_TITLE "You're ready. Go make something."
; A dedicated maintenance screen appears only when the current user already
; has Tally installed. It offers direct Repair/Update and Uninstall buttons.
Var ExistingDir
Var InstalledVersion
Var MaintenanceState
Var MaintenanceChoice
Var MaintenanceDialog
Var MaintenancePrimaryButton
Var MaintenanceUninstallButton

Page custom MaintenancePageCreate MaintenancePageLeave

!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipInstallIntroOnMaintenance
!insertmacro MUI_PAGE_WELCOME
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipInstallIntroOnMaintenance
!insertmacro MUI_PAGE_COMPONENTS
!define MUI_PAGE_CUSTOMFUNCTION_PRE SkipInstallIntroOnMaintenance
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!define MUI_FINISHPAGE_TEXT "Tally is now on your computer.$\r$\n$\r$\nOpen Tally Runner or Tally Examples from Start, or double-click any .tally program.$\r$\n$\r$\nMake something strange. Make something great."
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"
Function SkipInstallIntroOnMaintenance
  ${If} $MaintenanceChoice == "install"
    ; Repair or update launches directly into the install progress page.
    Abort
  ${EndIf}
FunctionEnd

Function MaintenancePageCreate
  ${If} $MaintenanceState == "none"
    Abort ; Not installed: display the ordinary installer wizard.
  ${EndIf}

  nsDialogs::Create 1018
  Pop $MaintenanceDialog
  ${If} $MaintenanceDialog == error
    Abort
  ${EndIf}

  ${If} $MaintenanceState == "update"
    !insertmacro MUI_HEADER_TEXT "UPDATE TALLY" "A newer version is ready."
    ${NSD_CreateLabel} 10u 7u 98% 30u "Tally $InstalledVersion is installed.$\r$\nThis setup will update it to ${TALLY_VERSION}."
  ${ElseIf} $MaintenanceState == "downgrade"
    !insertmacro MUI_HEADER_TEXT "NEWER VERSION INSTALLED" "This installer is older than your current Tally."
    ${NSD_CreateLabel} 10u 7u 98% 34u "You already have Tally $InstalledVersion.$\r$\nThis setup is for ${TALLY_VERSION}. To avoid a downgrade, installation is disabled."
  ${Else}
    !insertmacro MUI_HEADER_TEXT "TALLY IS ALREADY INSTALLED" "Choose what you want to do."
    ${NSD_CreateLabel} 10u 7u 98% 34u "Tally $InstalledVersion is already on this computer.$\r$\nRepair will reinstall program files without deleting your .tally projects."
  ${EndIf}
  Pop $0

  ${NSD_CreateLabel} 10u 48u 98% 12u "Installed in: $INSTDIR"
  Pop $0

  ${If} $MaintenanceState != "downgrade"
    ${If} $MaintenanceState == "update"
      ${NSD_CreateButton} 10u 74u 128u 30u "UPDATE TO ${TALLY_VERSION}"
    ${Else}
      ${NSD_CreateButton} 10u 74u 128u 30u "REPAIR TALLY"
    ${EndIf}
    Pop $MaintenancePrimaryButton
    ${NSD_OnClick} $MaintenancePrimaryButton MaintenanceInstallClicked
  ${Else}
    ${NSD_CreateLabel} 10u 73u 98% 30u "Use a newer installer to update, or uninstall the current version below."
    Pop $0
  ${EndIf}

  ${NSD_CreateButton} 151u 74u 128u 30u "UNINSTALL TALLY"
  Pop $MaintenanceUninstallButton
  ${NSD_OnClick} $MaintenanceUninstallButton MaintenanceUninstallClicked

  ; Only the large primary buttons perform actions; Cancel exits setup.
  GetDlgItem $0 $HWNDPARENT 1 ; Next
  ShowWindow $0 0
  GetDlgItem $0 $HWNDPARENT 3 ; Back
  ShowWindow $0 0
  nsDialogs::Show
FunctionEnd

Function MaintenancePageLeave
  ; Make the normal wizard buttons available to the following progress page.
  GetDlgItem $0 $HWNDPARENT 1
  ShowWindow $0 1
  GetDlgItem $0 $HWNDPARENT 3
  ShowWindow $0 1
  ${If} $MaintenanceChoice != "install"
    Abort
  ${EndIf}
FunctionEnd

Function MaintenanceInstallClicked
  StrCpy $MaintenanceChoice "install"
  ; Trigger the normal wizard Next command so NSIS advances to InstFiles.
  SendMessage $HWNDPARENT ${WM_COMMAND} 1 0
FunctionEnd

Function MaintenanceUninstallClicked
  IfFileExists "$INSTDIR\Uninstall.exe" 0 uninstall_missing
    ; Launch the existing real uninstaller, then close this setup program.
    ClearErrors
    Exec '$\"$INSTDIR\Uninstall.exe$\"'
    ${If} ${Errors}
      MessageBox MB_ICONSTOP "Could not launch Tally's uninstaller from $INSTDIR."
    ${Else}
      Quit
    ${EndIf}
    Return
  uninstall_missing:
    MessageBox MB_ICONSTOP "Cannot find Uninstall.exe in $INSTDIR."
FunctionEnd

Section "Tally runner (required)" CORE
 SectionIn RO
 ; Silent installs also refuse to downgrade an existing installation.
 ${If} $MaintenanceState == "downgrade"
   DetailPrint "Installed Tally $InstalledVersion is newer than ${TALLY_VERSION}. Refusing downgrade."
   SetErrorLevel 2
   Abort
 ${EndIf}
 SetOutPath "$INSTDIR"
 ; Remove only obsolete application-owned runtime files on upgrade.
 remove_legacy_node:
 Delete "$INSTDIR\runtime\node.exe"
 IfFileExists "$INSTDIR\runtime\node.exe" 0 legacy_node_removed
 MessageBox MB_RETRYCANCEL|MB_ICONEXCLAMATION "Close any running Tally programs so setup can remove the old Node.js runtime, then click Retry." IDRETRY remove_legacy_node
 Abort
 legacy_node_removed:
 Delete "$INSTDIR\runtime\NODE-LICENSE.txt"
 RMDir "$INSTDIR\runtime"
 Delete "$INSTDIR\cli.cjs"
 Delete "$INSTDIR\native-run.cjs"
 Delete "$INSTDIR\tally-engine.exe"
 Delete "$INSTDIR\dist\tally.js"
 Delete "$INSTDIR\dist\Tally-Offline.html"
 RMDir "$INSTDIR\dist"
 Delete "$SMPROGRAMS\Tally\Tally Editor.lnk"
 File "${PAYLOAD}\tally.exe"
 File "${PAYLOAD}\tally-draw.exe"
 File "${PAYLOAD}\tally-userpath.exe"
 File "${PAYLOAD}\THIRD-PARTY.txt"
 File "${PAYLOAD}\WINDOWS-START-HERE.txt"
 SetOutPath "$INSTDIR\examples"
 ; Preserve any student-edited example files during repair/update.
 SetOverwrite off
 File "${PAYLOAD}\examples\*.tally"
 SetOverwrite on
 WriteRegStr HKCU "Software\TallyLanguage" "InstallDir" "$INSTDIR"
 WriteRegStr HKCU "Software\TallyLanguage" "Version" "${TALLY_VERSION}"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\App Paths\tally.exe" "" "$INSTDIR\tally.exe"
 WriteUninstaller "$INSTDIR\Uninstall.exe"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "DisplayName" "Tally ${TALLY_VERSION}"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "DisplayVersion" "${TALLY_VERSION}"
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "UninstallString" '$\"$INSTDIR\Uninstall.exe$\"'
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "QuietUninstallString" '$\"$INSTDIR\Uninstall.exe$\" /S'
 WriteRegStr HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "InstallLocation" "$INSTDIR"
 WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "NoModify" 1
 WriteRegDWORD HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "NoRepair" 1
 CreateDirectory "$SMPROGRAMS\Tally"
 CreateShortCut "$SMPROGRAMS\Tally\Tally Runner.lnk" "$INSTDIR\tally.exe"
 CreateShortCut "$SMPROGRAMS\Tally\Tally Examples.lnk" "$INSTDIR\examples"
 CreateShortCut "$SMPROGRAMS\Tally\Start here.lnk" "$INSTDIR\WINDOWS-START-HERE.txt"
 CreateShortCut "$SMPROGRAMS\Tally\Uninstall Tally.lnk" "$INSTDIR\Uninstall.exe"
SectionEnd
Section "Open .tally files with Tally" ASSOC
 ReadRegStr $0 HKCU "Software\Classes\.tally" ""
 ${If} $0 != "TallyLanguage.Program"
  WriteRegStr HKCU "Software\TallyLanguage" "PreviousAssociation" "$0"
 ${EndIf}
 WriteRegStr HKCU "Software\Classes\.tally" "" "TallyLanguage.Program"
 WriteRegStr HKCU "Software\Classes\.tally\OpenWithProgids" "TallyLanguage.Program" ""
 WriteRegStr HKCU "Software\Classes\TallyLanguage.Program" "" "Tally program"
 WriteRegStr HKCU "Software\Classes\TallyLanguage.Program\DefaultIcon" "" '$\"$INSTDIR\tally.exe$\",0'
 WriteRegStr HKCU "Software\Classes\Applications\tally.exe" "FriendlyAppName" "Tally"
 WriteRegStr HKCU "Software\Classes\TallyLanguage.Program\shell\open\command" "" '$\"$INSTDIR\tally.exe$\" --pause $\"%1$\"'
 WriteRegStr HKCU "Software\Classes\TallyLanguage.Program\shell\edit" "" "Edit in Notepad"
 WriteRegStr HKCU "Software\Classes\TallyLanguage.Program\shell\edit\command" "" '$\"$WINDIR\system32\notepad.exe$\" $\"%1$\"'
 WriteRegStr HKCU "Software\Classes\Applications\tally.exe\shell\open\command" "" '$\"$INSTDIR\tally.exe$\" --pause $\"%1$\"'
 WriteRegStr HKCU "Software\Classes\Applications\tally.exe\SupportedTypes" ".tally" ""
 System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd
Section "Add tally to your user PATH" PATH
 ExecWait '$\"$INSTDIR\tally-userpath.exe$\" add $\"$INSTDIR$\"' $0
 ${If} $0 == 0
  WriteRegDWORD HKCU "Software\TallyLanguage" "PathAdded" 1
 ${ElseIf} $0 != 10
  DetailPrint "Could not update PATH. File associations and the Start menu still work."
 ${EndIf}
SectionEnd
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
 !insertmacro MUI_DESCRIPTION_TEXT ${CORE} "Standalone native interpreter, drawing window, and examples."
 !insertmacro MUI_DESCRIPTION_TEXT ${ASSOC} "Double-click .tally programs to run; right-click to edit in Notepad."
 !insertmacro MUI_DESCRIPTION_TEXT ${PATH} "Use tally from a new terminal. Updates only your user's PATH."
!insertmacro MUI_FUNCTION_DESCRIPTION_END
Section "Uninstall"
 SetShellVarContext current
 ReadRegDWORD $0 HKCU "Software\TallyLanguage" "PathAdded"
 ${If} $0 == 1
  ExecWait '$\"$INSTDIR\tally-userpath.exe$\" remove $\"$INSTDIR$\"'
 ${EndIf}
 ReadRegStr $0 HKCU "Software\Classes\.tally" ""
 ${If} $0 == "TallyLanguage.Program"
  ReadRegStr $1 HKCU "Software\TallyLanguage" "PreviousAssociation"
  ${If} $1 == ""
   DeleteRegValue HKCU "Software\Classes\.tally" ""
  ${Else}
   WriteRegStr HKCU "Software\Classes\.tally" "" "$1"
  ${EndIf}
 ${EndIf}
 DeleteRegValue HKCU "Software\Classes\.tally\OpenWithProgids" "TallyLanguage.Program"
 DeleteRegKey /ifempty HKCU "Software\Classes\.tally\OpenWithProgids"
 DeleteRegKey /ifempty HKCU "Software\Classes\.tally"
 DeleteRegKey HKCU "Software\Classes\TallyLanguage.Program"
 DeleteRegKey HKCU "Software\Classes\Applications\tally.exe"
 DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\App Paths\tally.exe"
 DeleteRegKey HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage"
 DeleteRegKey HKCU "Software\TallyLanguage"
 Delete "$INSTDIR\tally.exe"
 Delete "$INSTDIR\tally-draw.exe"
 Delete "$INSTDIR\tally-userpath.exe"
 Delete "$INSTDIR\cli.cjs"
 Delete "$INSTDIR\native-run.cjs"
 Delete "$INSTDIR\tally-engine.exe"
 Delete "$INSTDIR\THIRD-PARTY.txt"
 Delete "$INSTDIR\README.md"
 Delete "$INSTDIR\WINDOWS-START-HERE.txt"
 Delete "$INSTDIR\runtime\node.exe"
 Delete "$INSTDIR\runtime\NODE-LICENSE.txt"
 Delete "$INSTDIR\dist\tally.js"
 Delete "$INSTDIR\dist\Tally-Offline.html"
 Delete "$INSTDIR\Uninstall.exe"
 RMDir "$INSTDIR\runtime"
 RMDir "$INSTDIR\dist"
 ; Preserve the examples folder and student edits.
 RMDir "$INSTDIR"
 Delete "$SMPROGRAMS\Tally\Tally Runner.lnk"
 Delete "$SMPROGRAMS\Tally\Tally Editor.lnk"
 Delete "$SMPROGRAMS\Tally\Tally Examples.lnk"
 Delete "$SMPROGRAMS\Tally\Start here.lnk"
 Delete "$SMPROGRAMS\Tally\Uninstall Tally.lnk"
 RMDir "$SMPROGRAMS\Tally"
 System::Call 'shell32::SHChangeNotify(i 0x08000000, i 0, p 0, p 0)'
SectionEnd

; The section symbols ASSOC and PATH must exist before this callback is compiled.
Function .onInit
 ${IfNot} ${RunningX64}
  MessageBox MB_ICONSTOP "Tally requires 64-bit Windows 10 or Windows 11."
  Abort
 ${EndIf}
 SetShellVarContext current
 StrCpy $MaintenanceState "none"
 StrCpy $MaintenanceChoice ""
 StrCpy $InstalledVersion ""
 ; Only recognize an existing per-user Tally installation with its uninstaller.
 ReadRegStr $ExistingDir HKCU "Software\TallyLanguage" "InstallDir"
 ${If} $ExistingDir == ""
   ReadRegStr $ExistingDir HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "InstallLocation"
 ${EndIf}
 ${If} $ExistingDir != ""
   ${If} ${FileExists} "$ExistingDir\Uninstall.exe"
     StrCpy $INSTDIR $ExistingDir
     ReadRegStr $InstalledVersion HKCU "Software\Microsoft\Windows\CurrentVersion\Uninstall\TallyLanguage" "DisplayVersion"
     ${If} $InstalledVersion == ""
       ReadRegStr $InstalledVersion HKCU "Software\TallyLanguage" "Version"
     ${EndIf}
     ${If} $InstalledVersion == ""
       StrCpy $InstalledVersion "unknown"
       StrCpy $MaintenanceState "repair"
     ${Else}
       ; Result 0=same, 1=installer newer, 2=installed version newer.
       ${VersionCompare} "${TALLY_VERSION}" "$InstalledVersion" $0
       ${If} $0 == 1
         StrCpy $MaintenanceState "update"
       ${ElseIf} $0 == 2
         StrCpy $MaintenanceState "downgrade"
       ${Else}
         StrCpy $MaintenanceState "repair"
       ${EndIf}
     ${EndIf}
     ; Keep the previous optional settings during repair/update.
     ReadRegStr $0 HKCU "Software\Classes\.tally" ""
     ${If} $0 != "TallyLanguage.Program"
       ; Preserve non-selection bits such as read-only/visibility.
       SectionGetFlags ${ASSOC} $1
       IntOp $1 $1 & 0xFFFFFFFE
       SectionSetFlags ${ASSOC} $1
     ${EndIf}
     ReadRegDWORD $0 HKCU "Software\TallyLanguage" "PathAdded"
     ${If} $0 != 1
       SectionGetFlags ${PATH} $1
       IntOp $1 $1 & 0xFFFFFFFE
       SectionSetFlags ${PATH} $1
     ${EndIf}
   ${EndIf}
 ${EndIf}
FunctionEnd
