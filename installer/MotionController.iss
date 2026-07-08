[Setup]
AppId={{67DC4617-2D1B-4E9C-9315-FF8FA718953F}
AppName=MotionController
AppVersion=1.0.0
DefaultDirName={autopf}\MotionController
DefaultGroupName=MotionController
OutputDir=..\dist
OutputBaseFilename=MotionControllerSetup
SetupIconFile=..\resources\logo.ico
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "..\package\MotionController\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional tasks:"

[Icons]
Name: "{group}\MotionController"; Filename: "{app}\MotionController.exe"
Name: "{commondesktop}\MotionController"; Filename: "{app}\MotionController.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MotionController.exe"; Description: "Launch MotionController"; Flags: nowait postinstall skipifsilent
