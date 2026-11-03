param([string]$BuildDir='build-windows')
$ErrorActionPreference='Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
$root=Join-Path (Resolve-Path $BuildDir).Path 'reaper-test'
if (Test-Path $root) { throw "Use a fresh test directory: $root" }
New-Item -ItemType Directory $root | Out-Null
$plugins=Join-Path $root 'plugins'
New-Item -ItemType Directory $plugins | Out-Null
Copy-Item -Recurse "$BuildDir/TB303_DevilAcid_artefacts/Release/VST3/Punk Fish.vst3" $plugins
# Official evaluation installer, standard TLS and Authenticode validation retained.
$page=Invoke-WebRequest 'https://www.reaper.fm/download.php' -UseBasicParsing
$match=[regex]::Match($page.Content, '(?:https://www\.reaper\.fm/)?files/\d+\.x/reaper\d+_x64-install\.exe')
if (!$match.Success) { throw 'Official REAPER Windows x64 installer link was not found' }
$url=$match.Value
if (!$url.StartsWith('https://')) { $url='https://www.reaper.fm/'+$url }
$installer=Join-Path $root 'reaper-installer.exe'
Invoke-WebRequest $url -OutFile $installer -UseBasicParsing
$signature=Get-AuthenticodeSignature $installer
if ($signature.Status -ne 'Valid' -or $signature.SignerCertificate.Subject -notmatch 'Cockos') {
    throw "REAPER installer signature validation failed: $($signature.Status)"
}
$portable=Join-Path $root 'portable'
Start-Process $installer -ArgumentList @('/S','/PORTABLE',"/D=$portable") -Wait
$exe=Join-Path $portable 'reaper.exe'
if (!(Test-Path $exe)) { throw 'Portable REAPER installation failed' }
$cfg=Join-Path $root 'reaper.ini'
@"
[reaper]
vstpath64=$plugins
vstpath=$plugins
verchk=0
splash=0
lastproject=
[audioconfig]
mode=4
dummy_srate=44100
dummy_blocksize=512
"@ | Set-Content $cfg -Encoding ASCII
$env:PUNK_FISH_TEST_OUT=$root
Add-Type -AssemblyName UIAutomationClient
Add-Type -AssemblyName UIAutomationTypes
Add-Type -AssemblyName System.Drawing
Add-Type -AssemblyName System.Windows.Forms
$script=(Resolve-Path 'tools/reaper/smoke.lua').Path
foreach ($phase in @('initial','recall')) {
    $env:PUNK_FISH_TEST_PHASE=$phase
    $process=Start-Process $exe -ArgumentList "-newinst -nosplash -ignoreerrors -cfgfile `"$cfg`" `"$script`"" -PassThru
    try {
        $deadline=(Get-Date).AddMinutes(3)
        $captured=$false
        while ((Get-Date) -lt $deadline) {
            if (Test-Path (Join-Path $root "failed-$phase.txt")) {
                throw (Get-Content (Join-Path $root "failed-$phase.txt") -Raw)
            }
            # Use REAPER's normal evaluation button, respecting its built-in delay.
            # Do not modify licensing files, registration state, or evaluation limits.
            $desktop=[System.Windows.Automation.AutomationElement]::RootElement
            $condition=[System.Windows.Automation.PropertyCondition]::new(
                [System.Windows.Automation.AutomationElement]::ProcessIdProperty,$process.Id)
            $windows=$desktop.FindAll([System.Windows.Automation.TreeScope]::Children,$condition)
            foreach ($window in $windows) {
                $button=$window.FindFirst([System.Windows.Automation.TreeScope]::Descendants,
                    [System.Windows.Automation.PropertyCondition]::new(
                        [System.Windows.Automation.AutomationElement]::NameProperty,'Still Evaluating'))
                if ($null -ne $button -and $button.Current.IsEnabled) {
                    try { $button.GetCurrentPattern([System.Windows.Automation.InvokePattern]::Pattern).Invoke() } catch {}
                }
            }
            if (!$captured -and (Test-Path (Join-Path $root "editor-ready-$phase.txt"))) {
                try {
                    $screen=[System.Windows.Forms.Screen]::PrimaryScreen.Bounds
                    $bitmap=[System.Drawing.Bitmap]::new($screen.Width,$screen.Height)
                    $graphics=[System.Drawing.Graphics]::FromImage($bitmap)
                    $graphics.CopyFromScreen($screen.Location,[System.Drawing.Point]::Empty,$screen.Size)
                    $bitmap.Save((Join-Path $root "reaper-$phase.png"))
                    $graphics.Dispose();$bitmap.Dispose()
                    $captured=$true
                } catch { Write-Warning "Diagnostic screen capture unavailable: $_" }
            }
            if (Test-Path (Join-Path $root "passed-$phase.txt")) { break }
            Start-Sleep -Milliseconds 500
        }
        if (!(Test-Path (Join-Path $root "passed-$phase.txt"))) { throw "REAPER $phase test timed out" }
    } finally {
        if (!$process.HasExited) { Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue }
    }
}
python tools/reaper/check_render.py $root
if ($LASTEXITCODE -ne 0) { throw 'REAPER host render validation failed' }
Write-Host "Windows REAPER tests passed: $root/reaper-result.json"
