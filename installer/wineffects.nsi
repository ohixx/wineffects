; WinEffects installer. Build (Linux or Windows):
;   makensis -DROOT=<repo> -DEXE=<path>/wineffects.exe -DVERSION=0.3.0 -DOUTFILE=WinEffects-Setup.exe installer/wineffects.nsi
Unicode true
!include "MUI2.nsh"
!include "LogicLib.nsh"

!ifndef EXE
  !error "pass -DEXE=<path to wineffects.exe>"
!endif
!ifndef ROOT
  !error "pass -DROOT=<repository root>"
!endif
!ifndef VERSION
  !define VERSION "0.0.0"
!endif
!ifndef OUTFILE
  !define OUTFILE "WinEffects-Setup.exe"
!endif

!define NAME "WinEffects"
!define UNINST_KEY "Software\Microsoft\Windows\CurrentVersion\Uninstall\${NAME}"
!define RUN_KEY "Software\Microsoft\Windows\CurrentVersion\Run"

Name "${NAME}"
OutFile "${OUTFILE}"
InstallDir "$LOCALAPPDATA\Programs\${NAME}"
InstallDirRegKey HKCU "${UNINST_KEY}" "InstallLocation"
RequestExecutionLevel user  ; no admin needed; only the optional driver step asks for it
SetCompressor /SOLID lzma
BrandingText "${NAME} ${VERSION}"

!define MUI_ABORTWARNING
!define MUI_FINISHPAGE_RUN "$INSTDIR\wineffects.exe"
!define MUI_FINISHPAGE_RUN_TEXT "Launch ${NAME}"

!insertmacro MUI_PAGE_LICENSE "${ROOT}/LICENSE"
!insertmacro MUI_PAGE_COMPONENTS
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH
!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES
!insertmacro MUI_LANGUAGE "English"

Section "WinEffects" SecMain
  SectionIn RO
  nsExec::Exec 'taskkill /F /IM wineffects.exe'
  SetOutPath "$INSTDIR"
  File "${EXE}"
  WriteUninstaller "$INSTDIR\uninstall.exe"

  CreateDirectory "$SMPROGRAMS\${NAME}"
  CreateShortcut "$SMPROGRAMS\${NAME}\${NAME}.lnk" "$INSTDIR\wineffects.exe"
  CreateShortcut "$SMPROGRAMS\${NAME}\Uninstall ${NAME}.lnk" "$INSTDIR\uninstall.exe"

  WriteRegStr HKCU "${UNINST_KEY}" "DisplayName" "${NAME}"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayVersion" "${VERSION}"
  WriteRegStr HKCU "${UNINST_KEY}" "Publisher" "ohixx"
  WriteRegStr HKCU "${UNINST_KEY}" "InstallLocation" "$INSTDIR"
  WriteRegStr HKCU "${UNINST_KEY}" "DisplayIcon" "$INSTDIR\wineffects.exe"
  WriteRegStr HKCU "${UNINST_KEY}" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoModify" 1
  WriteRegDWORD HKCU "${UNINST_KEY}" "NoRepair" 1
SectionEnd

Section "Desktop shortcut" SecDesktop
  CreateShortcut "$DESKTOP\${NAME}.lnk" "$INSTDIR\wineffects.exe"
SectionEnd

Section "Start with Windows" SecAutostart
  WriteRegStr HKCU "${RUN_KEY}" "${NAME}" '"$INSTDIR\wineffects.exe" --minimized'
SectionEnd

Section "Virtual microphone (VB-Cable)" SecCable
  ; Downloads the driver from vb-audio.com, then installs it (Windows asks for administrator rights).
  SetOutPath "$TEMP"
  File "/oname=$TEMP\wineffects-get-vbcable.ps1" "${ROOT}/installer/get-vbcable.ps1"
  DetailPrint "Downloading VB-Cable from vb-audio.com..."
  nsExec::ExecToLog 'powershell -NoProfile -ExecutionPolicy Bypass -File "$TEMP\wineffects-get-vbcable.ps1"'
  Pop $0
  Delete "$TEMP\wineffects-get-vbcable.ps1"
  ${If} $0 == 0
  ${AndIf} ${FileExists} "$TEMP\wineffects-cable\pack\VBCABLE_Setup_x64.exe"
    DetailPrint "Installing the virtual audio device..."
    ExecShellWait "runas" "$TEMP\wineffects-cable\pack\VBCABLE_Setup_x64.exe" "-i -h"
    RMDir /r "$TEMP\wineffects-cable"
    MessageBox MB_OK|MB_ICONINFORMATION "VB-Cable was installed.$\n$\nIf WinEffects does not list 'CABLE Input' yet, restart Windows once."
  ${Else}
    MessageBox MB_OK|MB_ICONEXCLAMATION "Could not download VB-Cable automatically. The download page will open now; install it manually."
    ExecShell "open" "https://vb-audio.com/Cable/"
  ${EndIf}
SectionEnd

LangString DESC_Main ${LANG_ENGLISH} "The application."
LangString DESC_Desktop ${LANG_ENGLISH} "Adds a shortcut to the desktop."
LangString DESC_Autostart ${LANG_ENGLISH} "Starts WinEffects minimized in the tray when you sign in."
LangString DESC_Cable ${LANG_ENGLISH} "Downloads and installs VB-Cable, the free virtual audio cable that other programs see as a microphone. Needs administrator rights."
!insertmacro MUI_FUNCTION_DESCRIPTION_BEGIN
  !insertmacro MUI_DESCRIPTION_TEXT ${SecMain} $(DESC_Main)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecDesktop} $(DESC_Desktop)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecAutostart} $(DESC_Autostart)
  !insertmacro MUI_DESCRIPTION_TEXT ${SecCable} $(DESC_Cable)
!insertmacro MUI_FUNCTION_DESCRIPTION_END

Section "Uninstall"
  nsExec::Exec 'taskkill /F /IM wineffects.exe'
  Delete "$INSTDIR\wineffects.exe"
  Delete "$INSTDIR\uninstall.exe"
  RMDir "$INSTDIR"
  Delete "$SMPROGRAMS\${NAME}\${NAME}.lnk"
  Delete "$SMPROGRAMS\${NAME}\Uninstall ${NAME}.lnk"
  RMDir "$SMPROGRAMS\${NAME}"
  Delete "$DESKTOP\${NAME}.lnk"
  DeleteRegValue HKCU "${RUN_KEY}" "${NAME}"
  DeleteRegKey HKCU "${UNINST_KEY}"
  MessageBox MB_YESNO|MB_ICONQUESTION "Also delete your WinEffects settings?" /SD IDNO IDNO keep
    RMDir /r "$APPDATA\${NAME}"
  keep:
SectionEnd
