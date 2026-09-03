[Setup]
AppId={{1DE84F42-BF10-4FC5-98C0-EEF88B0E4880}
AppName=halo
AppVersion=1.1
AppPublisher=ryphe
DefaultDirName={autopf}\halo
UsePreviousAppDir=no
DefaultGroupName=halo
SetupIconFile=halo.ico
UninstallDisplayIcon={app}\halo.ico
Compression=lzma2/ultra64
SolidCompression=yes
OutputDir=.
OutputBaseFilename=halo_setup_1.1
WizardStyle=modern
ChangesAssociations=no
DirExistsWarning=no

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Tasks]
Name: "desktopicon"; Description: "{cm:CreateDesktopIcon}"; GroupDescription: "{cm:AdditionalIcons}"; Flags: unchecked

[Files]
Source: "halo.exe"; DestDir: "{app}"; Flags: ignoreversion
Source: "halo.ico"; DestDir: "{app}"; Flags: ignoreversion

[Icons]
Name: "{group}\halo"; Filename: "{app}\halo.exe"; IconFilename: "{app}\halo.ico"
Name: "{group}\{cm:UninstallProgram,halo}"; Filename: "{uninstallexe}"
Name: "{autodesktop}\halo"; Filename: "{app}\halo.exe"; IconFilename: "{app}\halo.ico"; Tasks: desktopicon

[UninstallDelete]
Type: files; Name: "{app}\halo.exe"
Type: files; Name: "{app}\halo.ico"
Type: files; Name: "{app}\unins*.exe"
Type: files; Name: "{app}\unins*.dat"

[Run]
Filename: "{app}\halo.exe"; Description: "{cm:LaunchProgram,halo}"; Flags: nowait postinstall skipifsilent