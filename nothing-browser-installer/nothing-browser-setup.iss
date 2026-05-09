#define MyAppName      "Nothing Browser"
#define MyAppPublisher "Ernest Tech House"
#define MyAppURL       "https://nothing-browser-docs.pages.dev"
#define MyAppExeName   "nothing-browser.exe"
#define GitHubRepo     "BunElysiaReact/nothing-browser"
#define GitHubAPIURL   "https://api.github.com/repos/BunElysiaReact/nothing-browser/releases/latest"

; ─────────────────────────────────────────────────────────────────────────────
[Setup]
AppId={{A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
AppName={#MyAppName}
AppVersion=latest
AppPublisher={#MyAppPublisher}
AppPublisherURL={#MyAppURL}
AppSupportURL={#MyAppURL}
AppUpdatesURL={#MyAppURL}

DefaultDirName={autopf}\NothingBrowser
DefaultGroupName={#MyAppName}
DisableProgramGroupPage=yes
DisableDirPage=no

OutputDir=output
OutputBaseFilename=nothing-browser-setup
SetupIconFile=assets\icons\logo.ico

WizardStyle=modern
WizardSizePercent=120
WizardImageFile=assets\installer-banner.bmp
WizardSmallImageFile=assets\installer-icon.bmp

Compression=lzma2/ultra64
SolidCompression=yes
MinVersion=10.0
PrivilegesRequired=admin

; ─────────────────────────────────────────────────────────────────────────────
[Languages]
Name: "english"; MessagesFile: "compiler:Default.isl"

; ─────────────────────────────────────────────────────────────────────────────
[Tasks]
Name: "desktopicon"; \
  Description: "{cm:CreateDesktopIcon}"; \
  GroupDescription: "{cm:AdditionalIcons}"; \
  Flags: checkedonce

; ─────────────────────────────────────────────────────────────────────────────
[Icons]
Name: "{autodesktop}\{#MyAppName}"; \
  Filename: "{app}\{#MyAppExeName}"; \
  Tasks: desktopicon

Name: "{autoprograms}\{#MyAppName}"; \
  Filename: "{app}\{#MyAppExeName}"

; ─────────────────────────────────────────────────────────────────────────────
[Run]
Filename: "{app}\{#MyAppExeName}"; \
  Description: "{cm:LaunchProgram,{#StringChange(MyAppName, '&', '&&')}}"; \
  Flags: nowait postinstall skipifsilent

; ─────────────────────────────────────────────────────────────────────────────
[UninstallDelete]
Type: filesandordirs; Name: "{app}"

; ─────────────────────────────────────────────────────────────────────────────
[Code]

var
  LatestVersion  : String;
  ZipFileName    : String;
  DownloadURL    : String;
  DownloadPage   : TDownloadWizardPage;

function GetLatestVersion(): String;
var
  Http     : Variant;
  Response : String;
  Chunk    : String;
  P1, P2   : Integer;
begin
  Result := '';
  try
    Http := CreateOleObject('WinHttp.WinHttpRequest.5.1');
    Http.Open('GET', '{#GitHubAPIURL}', False);
    Http.SetRequestHeader('User-Agent', 'NothingBrowser-Installer/1.0');
    Http.Send('');
    if Http.Status = 200 then
    begin
      Response := Http.ResponseText;
      P1 := Pos('"tag_name"', Response);
      if P1 > 0 then
      begin
        Chunk := Copy(Response, P1 + 10, 30);
        P1 := Pos('"', Chunk);
        if P1 > 0 then
        begin
          Chunk := Copy(Chunk, P1 + 1, Length(Chunk));
          P2 := Pos('"', Chunk);
          if P2 > 0 then
            Result := Copy(Chunk, 1, P2 - 1);
        end;
      end;
    end;
  except
    Result := '';
  end;
end;

function StripV(S: String): String;
begin
  if (Length(S) > 0) and (S[1] = 'v') then
    Result := Copy(S, 2, MaxInt)
  else
    Result := S;
end;

procedure ExtractZip(ZipPath, DestDir: String);
var
  RetVal  : Integer;
  PSArgs  : String;
begin
  PSArgs := '-NoProfile -NonInteractive -Command ' +
            '"Expand-Archive -LiteralPath ''' + ZipPath + ''' ' +
            '-DestinationPath ''' + DestDir + ''' -Force"';

  if not Exec(
    ExpandConstant('{sys}\WindowsPowerShell\v1.0\powershell.exe'),
    PSArgs,
    '',
    SW_HIDE,
    ewWaitUntilTerminated,
    RetVal
  ) or (RetVal <> 0) then
  begin
    MsgBox(
      'Extraction failed (exit code: ' + IntToStr(RetVal) + ').' + #13#10 +
      'You can extract manually from:' + #13#10 + ZipPath,
      mbError, MB_OK
    );
    Abort;
  end;
end;

procedure InitializeWizard();
begin
  DownloadPage := CreateDownloadPage(
    'Downloading Nothing Browser',
    'Fetching the latest release from GitHub...',
    nil
  );
end;

function NextButtonClick(CurPageID: Integer): Boolean;
begin
  Result := True;

  if CurPageID = wpReady then
  begin
    LatestVersion := GetLatestVersion();

    if LatestVersion = '' then
    begin
      MsgBox(
        'Could not reach GitHub to check the latest version.' + #13#10 +
        'Please check your internet connection and try again.',
        mbError, MB_OK
      );
      Result := False;
      Exit;
    end;

    ZipFileName := 'nothing-browser-' + StripV(LatestVersion) + '-windows-x64.zip';
    DownloadURL := 'https://github.com/{#GitHubRepo}/releases/download/' +
                    LatestVersion + '/' + ZipFileName;

    DownloadPage.Clear;
    DownloadPage.Add(DownloadURL, ZipFileName, '');
    DownloadPage.Show;
    try
      try
        DownloadPage.Download;
      except
        MsgBox(
          'Download failed:' + #13#10 + GetExceptionMessage + #13#10#13#10 +
          'URL: ' + DownloadURL,
          mbError, MB_OK
        );
        Result := False;
      end;
    finally
      DownloadPage.Hide;
    end;
  end;
end;

procedure CurStepChanged(CurStep: TSetupStep);
var
  ZipTempPath : String;
begin
  if CurStep = ssInstall then
  begin
    ZipTempPath := ExpandConstant('{tmp}\') + ZipFileName;
    ExtractZip(ZipTempPath, ExpandConstant('{app}'));
  end;
end;
