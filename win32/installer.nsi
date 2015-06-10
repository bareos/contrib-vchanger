#................-
#. Date of creation: 2015-06-01
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
!define APPNAME "vchanger"
!define COMPANYNAME "Josh Fisher"
!define DESCRIPTION "A virtual disk autochanger for Bacula"
# These three must be integers
!define VERSIONMAJOR 1
!define VERSIONMINOR 0
!define VERSIONBUILD 1
# These will be displayed by the "Click here for support information" link in "Add/Remove Programs"
!define HELPURL "http://sourceforge.net/projects/vchanger/" # "Support Information" link
Name "vchanger 1.0.1"
VIProductVersion "1.0.1.0"
VIAddVersionKey /LANG=${LANG_ENGLISH} "ProductName" "vchanger Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "LegalCopyright" "Copyright (c) Josh Fisher 2008-2015"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileDescription" "vchanger Windows Installer"
VIAddVersionKey /LANG=${LANG_ENGLISH} "FileVersion" "1.0.1"
ShowInstDetails nevershow
SilentInstall normal
RequestExecutionLevel admin
AutoCloseWindow True
OutFile "vchanger-1.0.1.exe"

PageEx license
  LicenseData "license.txt"
PageExEnd
Page directory
Page instfiles

UninstPage uninstConfirm
UninstPage instfiles

Section "vchanger 32-bit" SEC0001
  SectionIn RO
  SetOutPath $INSTDIR
  ClearErrors
  CreateDirectory "$APPDATA\vchanger"
  IfErrors 0 +3
    MessageBox MB_OK 'Could not create vchanger work directory'
    Quit
  File license.txt
  File ReleaseNotes.txt
  File vchangerHowto.html
  File vchanger-example.conf
  File vchanger.exe
  WriteUninstaller "uninstall.exe"
  IfFileExists "$APPDATA\vchanger\vchanger.conf" +2 0
    CopyFiles "$INSTDIR\vchanger-example.conf" "$APPDATA\vchanger\vchanger.conf"
  WriteRegStr HKLM "Software\vchanger" "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "DisplayName" "vchanger"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\vchanger"
  CreateShortCut "$SMPROGRAMS\vchanger\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\vchanger\vchanger Howto.lnk" "$INSTDIR\vchangerHowto.html" "" "$INSTDIR\vchangerHowto.html" 0
  CreateShortcut "$SMPROGRAMS\vchanger\vchanger config.lnk" "$PROGRAMFILES32\Windows NT\Accessories\wordpad.exe" "$APPDATA\vchanger\vchanger.conf"
SectionEnd

Section "vchanger 64-bit" SEC0002
  SectionIn RO
  SetOutPath $INSTDIR
  ClearErrors
  CreateDirectory "$APPDATA\vchanger"
  IfErrors 0 +3
    MessageBox MB_OK 'Could not create vchanger work directory'
    Quit
  File license.txt
  File ReleaseNotes.txt
  File vchangerHowto.html
  File vchanger-example.conf
  File "/oname=vchanger.exe" vchanger64.exe
  WriteUninstaller "uninstall.exe"
  IfFileExists "$APPDATA\vchanger\vchanger.conf" +2 0
    CopyFiles "$INSTDIR\vchanger-example.conf" "$APPDATA\vchanger\vchanger.conf"
  WriteRegStr HKLM "Software\vchanger" "Install_Dir" "$INSTDIR"
  ; Write the uninstall keys for Windows
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "DisplayName" "vchanger"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger" "NoRepair" 1
  CreateDirectory "$SMPROGRAMS\vchanger"
  CreateShortCut "$SMPROGRAMS\vchanger\Uninstall.lnk" "$INSTDIR\uninstall.exe" "" "$INSTDIR\uninstall.exe" 0
  CreateShortCut "$SMPROGRAMS\vchanger\vchanger Howto.lnk" "$INSTDIR\vchangerHowto.html" "" "$INSTDIR\vchangerHowto.html" 0
  CreateShortcut "$SMPROGRAMS\vchanger\vchanger config.lnk" "$PROGRAMFILES64\Windows NT\Accessories\wordpad.exe" "$APPDATA\vchanger\vchanger.conf"
SectionEnd

# Uninstaller Section
Section "Uninstall"
  # Remove shortcuts, if any
  Delete "$SMPROGRAMS\vchanger\*.*"
  RMDir "$SMPROGRAMS\vchanger"

  # Remove files and uninstaller
  Delete $INSTDIR\vchanger.exe
  Delete $INSTDIR\license.txt
  Delete $INSTDIR\ReleaseNotes.txt
  Delete $INSTDIR\vchangerHowto.html
  Delete $INSTDIR\vchanger-example.conf
  Delete $INSTDIR\uninstall.exe
  RMDir "$INSTDIR"

  # Remove information from the registry
  DeleteRegKey HKLM "Software\vchanger"
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\vchanger"
SectionEnd

Function .onInit
  SetShellVarContext all
  #Determine the bitness of the OS and enable the correct section
  ${If} ${RunningX64}
    SectionSetFlags SEC0001  ${SECTION_OFF}
    SectionSetFlags SEC0002  ${SF_SELECTED}
    StrCpy $INSTDIR "$PROGRAMFILES64\vchanger"
    SetRegView 64
  ${Else}
    SectionSetFlags SEC0002  ${SECTION_OFF}
    SectionSetFlags SEC0001  ${SF_SELECTED}
    StrCpy $INSTDIR "$PROGRAMFILES32\vchanger"
    SetRegView 32
  ${EndIf}
FunctionEnd

Function un.onInit
  SetShellVarContext all
  #Determine the bitness of the OS and enable the correct section
  ${If} ${RunningX64}
    StrCpy $INSTDIR "$PROGRAMFILES64\vchanger"
    SetRegView 64
  ${Else}
    StrCpy $INSTDIR "$PROGRAMFILES32\vchanger"
    SetRegView 32
  ${EndIf}
FunctionEnd

Function .onInstSuccess
  MessageBox MB_OK "You have successfully installed vchanger."
FunctionEnd
