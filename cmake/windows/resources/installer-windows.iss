; Compiled by .github/scripts/Package-Windows.ps1, which passes these defines:
; ProductName, DisplayName, ProductVersion, Publisher, Website, StagingDir,
; OutputDir, OutputName

[Setup]
; Must stay the same across versions so upgrades replace the previous install
AppId=stream.breezy.obs-breezy
AppName={#DisplayName}
AppVersion={#ProductVersion}
AppVerName={#DisplayName} {#ProductVersion}
AppPublisher={#Publisher}
AppPublisherURL={#Website}
AppSupportURL={#Website}
VersionInfoVersion={#ProductVersion}

; OBS loads plugins for all users from ProgramData, which requires elevation
PrivilegesRequired=admin
DefaultDirName={commonappdata}\obs-studio\plugins\{#ProductName}
DisableDirPage=yes
DisableProgramGroupPage=yes

; Keep the uninstaller out of the folder that OBS scans for plugins
UninstallFilesDir={commonappdata}\Breezy\{#ProductName}
UninstallDisplayName={#DisplayName}

ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
MinVersion=10.0

; The plugin DLLs are locked while OBS is running
CloseApplications=yes
RestartApplications=no

OutputDir={#OutputDir}
OutputBaseFilename={#OutputName}
Compression=lzma2
SolidCompression=yes
WizardStyle=modern

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[InstallDelete]
; Remove files left behind by a previous version
Type: filesandordirs; Name: "{app}\bin"
Type: filesandordirs; Name: "{app}\data"

[Files]
Source: "{#StagingDir}\*"; DestDir: "{app}"; Excludes: "*.pdb"; Flags: ignoreversion recursesubdirs createallsubdirs

[UninstallDelete]
Type: dirifempty; Name: "{app}"
