; QSSTV Windows Installer Script (NSIS)
; Builds a self-extracting installer for QSSTV

!include "MUI2.nsh"

; --- General ---
Name "QSSTV"
OutFile "..\qsstv-setup.exe"
InstallDir "$PROGRAMFILES64\QSSTV"
InstallDirRegKey HKLM "Software\QSSTV" "InstallDir"
RequestExecutionLevel admin

; --- Interface ---
!define MUI_ABORTWARNING
!define MUI_ICON "${NSISDIR}\Contrib\Graphics\Icons\modern-install.ico"
!define MUI_UNICON "${NSISDIR}\Contrib\Graphics\Icons\modern-uninstall.ico"

; --- Pages ---
!insertmacro MUI_PAGE_WELCOME
!insertmacro MUI_PAGE_DIRECTORY
!insertmacro MUI_PAGE_INSTFILES
!insertmacro MUI_PAGE_FINISH

!insertmacro MUI_UNPAGE_CONFIRM
!insertmacro MUI_UNPAGE_INSTFILES

!insertmacro MUI_LANGUAGE "English"

; --- Install Section ---
Section "QSSTV" SecMain
  SectionIn RO
  SetOutPath "$INSTDIR"

  ; Main executable
  File "..\package\qsstv.exe"

  ; Qt DLLs
  File "..\package\Qt5Core.dll"
  File "..\package\Qt5Gui.dll"
  File "..\package\Qt5Network.dll"
  File "..\package\Qt5SerialPort.dll"
  File "..\package\Qt5Widgets.dll"
  File "..\package\Qt5Xml.dll"

  ; Runtime DLLs
  File "..\package\libgcc_s_seh-1.dll"
  File "..\package\libstdc++-6.dll"
  File "..\package\libwinpthread-1.dll"

  ; Library DLLs
  File "..\package\libhamlib-4.dll"
  File "..\package\libportaudio.dll"
  File "..\package\libfftw3-3.dll"
  File "..\package\libfftw3f-3.dll"
  File "..\package\libopenjp2-7.dll"
  File "..\package\libavif-16.dll"
  File "..\package\libaom.dll"
  File "..\package\libdav1d-7.dll"
  File "..\package\librav1e.dll"
  File "..\package\libSvtAv1Enc-4.dll"
  File "..\package\libsharpyuv-0.dll"
  File "..\package\libyuv.dll"
  File "..\package\libjpeg-8.dll"

  ; Support DLLs
  File "..\package\zlib1.dll"
  File "..\package\libzstd.dll"
  File "..\package\libmd4c.dll"
  File "..\package\libpng16-16.dll"
  File "..\package\libfreetype-6.dll"
  File "..\package\libbz2-1.dll"
  File "..\package\libharfbuzz-0.dll"
  File "..\package\libgraphite2.dll"
  File "..\package\libglib-2.0-0.dll"
  File "..\package\libintl-8.dll"
  File "..\package\libiconv-2.dll"
  File "..\package\libpcre2-8-0.dll"
  File "..\package\libpcre2-16-0.dll"
  File "..\package\libdouble-conversion.dll"
  File "..\package\libbrotlidec.dll"
  File "..\package\libbrotlicommon.dll"
  File "..\package\libicudt78.dll"
  File "..\package\libicuin78.dll"
  File "..\package\libicuuc78.dll"

  ; Qt plugins
  SetOutPath "$INSTDIR\platforms"
  File "..\package\platforms\qwindows.dll"

  SetOutPath "$INSTDIR\imageformats"
  File "..\package\imageformats\qgif.dll"
  File "..\package\imageformats\qico.dll"
  File "..\package\imageformats\qjpeg.dll"

  SetOutPath "$INSTDIR\styles"
  File "..\package\styles\qwindowsvistastyle.dll"

  ; Back to install dir
  SetOutPath "$INSTDIR"

  ; Create uninstaller
  WriteUninstaller "$INSTDIR\uninstall.exe"

  ; Registry
  WriteRegStr HKLM "Software\QSSTV" "InstallDir" "$INSTDIR"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QSSTV" "DisplayName" "QSSTV"
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QSSTV" "UninstallString" '"$INSTDIR\uninstall.exe"'
  WriteRegStr HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QSSTV" "InstallLocation" "$INSTDIR"
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QSSTV" "NoModify" 1
  WriteRegDWORD HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QSSTV" "NoRepair" 1

  ; Start menu shortcut
  CreateDirectory "$SMPROGRAMS\QSSTV"
  CreateShortcut "$SMPROGRAMS\QSSTV\QSSTV.lnk" "$INSTDIR\qsstv.exe"
  CreateShortcut "$SMPROGRAMS\QSSTV\Uninstall.lnk" "$INSTDIR\uninstall.exe"

  ; Desktop shortcut
  CreateShortcut "$DESKTOP\QSSTV.lnk" "$INSTDIR\qsstv.exe"
SectionEnd

; --- Uninstall Section ---
Section "Uninstall"
  ; Remove files
  Delete "$INSTDIR\qsstv.exe"
  Delete "$INSTDIR\*.dll"
  Delete "$INSTDIR\platforms\qwindows.dll"
  Delete "$INSTDIR\imageformats\*.dll"
  Delete "$INSTDIR\styles\*.dll"
  Delete "$INSTDIR\uninstall.exe"

  ; Remove directories
  RMDir "$INSTDIR\platforms"
  RMDir "$INSTDIR\imageformats"
  RMDir "$INSTDIR\styles"
  RMDir "$INSTDIR"

  ; Remove shortcuts
  Delete "$SMPROGRAMS\QSSTV\QSSTV.lnk"
  Delete "$SMPROGRAMS\QSSTV\Uninstall.lnk"
  RMDir "$SMPROGRAMS\QSSTV"
  Delete "$DESKTOP\QSSTV.lnk"

  ; Remove registry
  DeleteRegKey HKLM "Software\Microsoft\Windows\CurrentVersion\Uninstall\QSSTV"
  DeleteRegKey HKLM "Software\QSSTV"
SectionEnd
