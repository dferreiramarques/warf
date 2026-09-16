; Inno Setup script for Warf (Beta) + Warf Synth (Beta) VST3.
; Build both plugins first (Release config), then compile this with:
;   "C:\Users\david\AppData\Local\Programs\Inno Setup 6\ISCC.exe" installer\Warf.iss
; Output lands in ..\downloads.

#define MyAppName "Warf (Beta)"
#define MyAppVersion "0.1.0"
#define MyAppPublisher "Monco.io"
#define MyAppURL "https://warf.monco.io"
#define MyVst3Name "Warf (Beta).vst3"
#define MyBuiltVst3Dir "..\build\Warf_artefacts\Release\VST3\Warf (Beta).vst3"
#define MySynthVst3Name "Warf Synth (Beta).vst3"
#define MySynthBuiltVst3Dir "..\build\WarfSynth_artefacts\Release\VST3\Warf Synth (Beta).vst3"

[Setup]
AppId={{9C4E2B7A-5D1F-4A2E-8B6C-3F0A7D1E9C42}
AppName={#MyAppName}
AppVersion={#MyAppVersion}
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
DefaultDirName={commoncf64}\VST3\{#MyVst3Name}
DisableDirPage=yes
DisableProgramGroupPage=yes
DisableReadyPage=yes
DisableWelcomePage=no
UsePreviousAppDir=no
Compression=lzma2
SolidCompression=yes
ArchitecturesAllowed=x64compatible
ArchitecturesInstallIn64BitMode=x64compatible
PrivilegesRequired=admin
OutputDir=..\downloads
OutputBaseFilename=Warf_{#MyAppVersion}_Beta_VST3_x64-setup
WizardStyle=modern
UninstallDisplayIcon={commoncf64}\VST3\{#MyVst3Name}\Contents\Resources\moduleinfo.json

[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

[Files]
Source: "{#MyBuiltVst3Dir}\*"; DestDir: "{commoncf64}\VST3\{#MyVst3Name}"; Flags: recursesubdirs ignoreversion
Source: "{#MySynthBuiltVst3Dir}\*"; DestDir: "{commoncf64}\VST3\{#MySynthVst3Name}"; Flags: recursesubdirs ignoreversion

[UninstallDelete]
Type: filesandordirs; Name: "{commoncf64}\VST3\{#MyVst3Name}"
Type: filesandordirs; Name: "{commoncf64}\VST3\{#MySynthVst3Name}"

[Messages]
WelcomeLabel2=This installs the beta VST3 build of %n%nWarf and Warf Synth%n%ninto your system's VST3 folder. Warf is a normal audio effect - insert it on an audio track, then pick a MIDI output device right in its editor (a virtual MIDI port like loopMIDI works well if you want that MIDI to land on another track in the same DAW). Warf Synth is a minimal companion instrument you can put on that receiving track to actually hear the result.%n%nThis is a beta - please report anything that sounds wrong or behaves unexpectedly.
FinishedLabelNoIcons=Setup has finished installing {#MyAppName}. Rescan plugins in your DAW to see them.
