[Setup]
AppName=Asteria L
AppVersion=0.9
AppPublisher=lejean2000
DefaultDirName={commonpf32}\Asteria
DefaultGroupName=Asteria L
OutputDir=installer_output
OutputBaseFilename=AsteriaL_0.9_Setup
Compression=lzma2
SolidCompression=yes
ArchitecturesInstallIn64BitMode=x64compatible
SetupIconFile=icons\asteria-l.ico

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
; Main exe, Qt DLLs/plugins, and QML modules
Source: "deploy\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
; Ephemeris files at the baked-in path C:\Program Files (x86)\Asteria\share\Asteria\ephemeris
Source: "deploy\share\Asteria\ephemeris\*"; DestDir: "{app}\share\Asteria\ephemeris"; Flags: ignoreversion
; MSVC runtime (windeployqt dropped it alongside the exe)
Source: "deploy\vc_redist.x64.exe"; DestDir: "{tmp}"; Flags: deleteafterinstall

[Icons]
Name: "{group}\Asteria L"; Filename: "{app}\Asteria.exe"; IconFilename: "{app}\Asteria.exe"
Name: "{commondesktop}\Asteria L"; Filename: "{app}\Asteria.exe"; IconFilename: "{app}\Asteria.exe"

[Run]
; Install MSVC runtime silently first
Filename: "{tmp}\vc_redist.x64.exe"; Parameters: "/quiet /norestart"; StatusMsg: "Installing Visual C++ runtime..."; Flags: waituntilterminated
; Offer to launch after install
Filename: "{app}\Asteria.exe"; Description: "Launch Asteria"; Flags: nowait postinstall skipifsilent
