!include "LogicLib.nsh"
!include "FileFunc.nsh"
!insertmacro DirState
Name "vchanger"
OutFile "win32_vchanger-0.8.4.exe"
InstallDir "$PROGRAMFILES\vchanger\"
RequestExecutionLevel admin
InstallDirRegKey HKLM "Software\vchanger" "Install_Dir"

PageEx license
  LicenseData "license.txt"
PageExEnd
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "vchanger (required)"
  SectionIn RO
  SetShellVarContext all
  ${DirState} "$APPDATA\Bacula" $R0
  ${If} $R0 == -1
    MessageBox MB_OK 'Could not find Bacula working directory.$\rBacula must be installed before vchanger'
    Quit
  ${EndIf}
  SetOutPath $INSTDIR
  File vchanger.exe
  File license.txt
  File ReleaseNotes.txt
  File vchangerHowto.html
  File example.conf
  IfFileExists $APPDATA\Bacula\vchanger.conf +2 0
  CopyFiles $INSTDIR\example.conf $APPDATA\Bacula\vchanger.conf
  WriteRegStr HKLM SOFTWARE\vchanger "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "DisplayName" "vchanger"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

; Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\vchanger"
  CreateShortCut "$SMPROGRAMS\vchanger\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\vchanger\vchanger Howto.lnk" "$INSTDIR\vchangerHowto.html" "" "$INSTDIR\vchangerHowto.html" 0
SectionEnd

; Uninstaller

Section "Uninstall"
  SetShellVarContext all
  ; Remove registry keys
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger"
  DeleteRegKey HKLM SOFTWARE\vchanger

  ; Remove files and uninstaller
  Delete $INSTDIR\uninstall.exe
  Delete $INSTDIR\vchanger.exe
  Delete $INSTDIR\license.txt
  Delete $INSTDIR\vchangerHowto.html
  Delete $INSTDIR\example.conf

  ; Remove shortcuts, if any
  Delete "$SMPROGRAMS\vchanger\*.*"

  ; Remove directories used
  RMDir "$SMPROGRAMS\vchanger"
  RMDir "$INSTDIR"
SectionEnd
