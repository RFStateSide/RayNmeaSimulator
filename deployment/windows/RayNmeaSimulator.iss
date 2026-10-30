; Copyright (C) 2026 RF StateSide LLC; SPDX-License-Identifier: AGPL-3.0-or-later
; Adapted from RF StateSide BrickSuite deployment/windows/BrickSuite.iss.
#ifndef AppVersion
  #error Supply AppVersion, StageDir and OutputDir using scripts/package.py
#endif
[Setup]
AppId={{E3048447-B053-4C82-8A90-45C05B214BC7}
AppName=RayNmeaSimulator
AppVersion={#AppVersion}
AppPublisher=RF StateSide LLC
AppPublisherURL=https://rfstateside.com
DefaultDirName={localappdata}\Programs\RayNmeaSimulator
DefaultGroupName=RayNmeaSimulator
DisableProgramGroupPage=yes
PrivilegesRequired=lowest
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64
UsePreviousAppDir=yes
UninstallDisplayIcon={app}\RayNmeaSimulator.ico
SetupIconFile={#StageDir}\RayNmeaSimulator.ico
LicenseFile={#StageDir}\LICENSE
OutputDir={#OutputDir}
OutputBaseFilename=RayNmeaSimulator-{#AppVersion}-Windows-x64-Setup
Compression=lzma2
SolidCompression=yes
WizardStyle=modern
SetupLogging=yes
CloseApplications=yes
CloseApplicationsFilter=RayNmeaSimulator.exe
RestartApplications=no
VersionInfoVersion={#AppVersion}
VersionInfoCompany=RF StateSide LLC
VersionInfoDescription=RayNmeaSimulator Private Acceptance Installer
[Tasks]
Name: desktopicon; Description: "Create a &desktop shortcut"; Flags: unchecked
[Files]
Source: "{#StageDir}\*"; DestDir: "{app}"; Flags: ignoreversion recursesubdirs createallsubdirs
[Icons]
Name: "{group}\RayNmeaSimulator"; Filename: "{app}\RayNmeaSimulator.exe"
Name: "{group}\Uninstall RayNmeaSimulator"; Filename: "{uninstallexe}"
Name: "{userdesktop}\RayNmeaSimulator"; Filename: "{app}\RayNmeaSimulator.exe"; Tasks: desktopicon
; Deliberately no Run/Delete sections: never launch after install/upgrade or remove user data.
