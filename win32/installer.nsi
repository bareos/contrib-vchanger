#................-
#. Date of creation: 2014-10-01
#. Name: installer.nsi
#................-
#..- Package parameters ...
SetCompress force
SetCompressor lzma
!include LogicLib.nsh
!include MUI2.nsh
!include WinMessages.nsh
!include x64.nsh
!insertmacro MUI_LANGUAGE English
#... Installation Windows Parameters ..
Name "vchanger 1.0.0"
VIProductVersion "1.0.0.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "vchanger Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Copyright (c) Josh Fisher 2008-2014"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "vchanger Windows Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "1.0.0"
ShowInstDetails nevershow
SilentInstall normal
RequestExecutionLevel admin
AutoCloseWindow True
InstallDir "$PROGRAMFILES\vchanger"
OutFile "vchanger-1.0.0.exe"

PageEx license
  LicenseData "license.txt"
PageExEnd
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "vchanger 32-bit" SEC0001
  SetOutPath $instdir
  SectionIn RO
  SetShellVarContext all
  ClearErrors
  CreateDirectory "$APPDATA\vchanger"
  IfErrors 0 +3
    MessageBox MB_OK 'Could not create vchanger work directory'
    Quit
  SetOutPath "$INSTDIR"
  File license.txt
  File ReleaseNotes.txt
  File vchangerHowto.html
  File vchanger-example.conf
  File vchanger.exe
  SetRegView 32
  IfFileExists "$APPDATA\vchanger\vchanger.conf" +2 0
    CopyFiles "$INSTDIR\vchanger-example.conf" "$APPDATA\vchanger\vchanger.conf"
  WriteRegStr HKLM "Software\vchanger" "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "DisplayName" "vchanger"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd

Section "vchanger 64-bit" SEC0002
  SetOutPath $instdir
  SectionIn RO
  SetShellVarContext all
  ClearErrors
  CreateDirectory "$APPDATA\vchanger"
  IfErrors 0 +3
    MessageBox MB_OK 'Could not create vchanger work directory'
    Quit
  SetOutPath "$INSTDIR"
  File license.txt
  File ReleaseNotes.txt
  File vchangerHowto.html
  File vchanger-example.conf
  File "/oname=vchanger.exe" vchanger64.exe
  SetRegView 64
  IfFileExists "$APPDATA\vchanger\vchanger.conf" +2 0
    CopyFiles "$INSTDIR\vchanger-example.conf" "$APPDATA\vchanger\vchanger.conf"
  WriteRegStr HKLM "Software\vchanger" "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "DisplayName" "vchanger"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoRepair" 1
  WriteUninstaller "uninstall.exe"
SectionEnd


# Optional section (can be disabled by the user)
Section "Start Menu Shortcuts"
  SetShellVarContext all
  CreateDirectory "$SMPROGRAMS\vchanger"
  CreateShortCut "$SMPROGRAMS\vchanger\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\vchanger\vchanger Howto.lnk" "$INSTDIR\vchangerHowto.html" "" "$INSTDIR\vchangerHowto.html" 0
  ${If} ${RunningX64}
    CreateShortcut "$SMPROGRAMS\vchanger\vchanger config.lnk" "$PROGRAMFILES64\Windows NT\Accessories\wordpad.exe" "$APPDATA\vchanger\vchanger.conf"
  ${else}
    CreateShortcut "$SMPROGRAMS\vchanger\vchanger config.lnk" "$PROGRAMFILES32\Windows NT\Accessories\wordpad.exe" "$APPDATA\vchanger\vchanger.conf"
  ${endif}
SectionEnd

# Uninstaller Section
Section "Uninstall"
  # Remove registry keys
  ${If} ${RunningX64}
    SetRegView 64
  ${else}
    SetRegView 32
  ${endif}
  SetShellVarContext all
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger"
  DeleteRegKey HKLM "Software\vchanger"

  # Remove files and uninstaller
  Delete $INSTDIR\uninstall.exe
  Delete $INSTDIR\vchanger.exe
  Delete $INSTDIR\license.txt
  Delete $INSTDIR\ReleaseNotes.txt
  Delete $INSTDIR\vchangerHowto.html
  Delete $INSTDIR\vchanger-example.conf

  # Remove shortcuts, if any
  Delete "$SMPROGRAMS\vchanger\*.*"

  # Remove directories used
  RMDir "$SMPROGRAMS\vchanger"
  RMDir "$INSTDIR"
SectionEnd

Function .onInit
  #Determine the bitness of the OS and enable the correct section
  ${If} ${RunningX64}
    SectionSetFlags SEC0000  ${SECTION_OFF}
    SectionSetFlags SEC0001  ${SF_SELECTED}
  ${Else}
    SectionSetFlags SEC0001  ${SECTION_OFF}
    SectionSetFlags SEC0000  ${SF_SELECTED}
  ${EndIf}
FunctionEnd

