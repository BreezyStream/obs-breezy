# Builds and packages the Windows plugin.
# Output: release\obs-breezy-<version>-windows-x64.zip
#         release\obs-breezy-<version>-windows-x64.exe

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot

$Spec = Get-Content buildspec.json -Raw | ConvertFrom-Json
$Version = $Spec.version
$OutputName = "obs-breezy-$Version-windows-x64"
$Output = "release\$OutputName.zip"
$Installer = "release\$OutputName.exe"

# Check before building so a missing tool doesn't waste a full build
$Iscc = (Get-Command -Name "iscc" -ErrorAction SilentlyContinue).Source
if (-not $Iscc) {
	# Inno Setup doesn't add itself to PATH, and winget may install it per user
	$Iscc = @(
		"${Env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe"
		"$Env:LOCALAPPDATA\Programs\Inno Setup 6\ISCC.exe"
	) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Iscc) {
	throw "Inno Setup not found. Install it with: winget install JRSoftware.InnoSetup"
}

Write-Host "==> Releasing obs-breezy $Version"

Write-Host "==> Building"
# build-win-release.ps1 errors on a missing build_x64, don't stop on that
$ErrorActionPreference = "Continue"
& .\build-win-release.ps1
if ($LASTEXITCODE -ne 0) { throw "Build failed" }
$ErrorActionPreference = "Stop"

Write-Host "==> Installing"
if (Test-Path release) { Remove-Item release -Recurse -Force }
cmake --install build_x64 --config Release --prefix release
if ($LASTEXITCODE -ne 0) { throw "Install failed" }

if (-not (Test-Path release\obs-breezy\bin\64bit\obs-breezy.dll)) {
	throw "release\obs-breezy\bin\64bit\obs-breezy.dll not found"
}

Write-Host "==> Packaging"
Compress-Archive -Path release\obs-breezy -DestinationPath $Output

Write-Host "==> Building installer"
& $Iscc `
	"/DProductName=$($Spec.name)" `
	"/DDisplayName=$($Spec.displayName)" `
	"/DProductVersion=$Version" `
	"/DPublisher=$($Spec.author)" `
	"/DWebsite=$($Spec.website)" `
	"/DStagingDir=$PSScriptRoot\release\obs-breezy" `
	"/DOutputDir=$PSScriptRoot\release" `
	"/DOutputName=$OutputName" `
	"cmake\windows\resources\installer-windows.iss"
if ($LASTEXITCODE -ne 0) { throw "Installer build failed" }

Write-Host "==> Done: $Output"
Write-Host "==> Done: $Installer"
