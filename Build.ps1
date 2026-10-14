param(
    [string]$Image = 'devkitpro/devkita64',
    [string]$PortlibsVolume = 'switch-music-portlibs'
)

$ErrorActionPreference = 'Stop'
$sourceRoot = [IO.Path]::GetFullPath($PSScriptRoot)
& docker info --format '{{.ServerVersion}}'
if ($LASTEXITCODE -ne 0) { throw 'Docker engine is unavailable.' }

$buildLog = Join-Path $sourceRoot 'build.log'
$ErrorActionPreference = 'Continue'
& docker run --rm --mount "type=bind,source=$sourceRoot,target=/work" --mount "type=volume,source=$PortlibsVolume,target=/opt/devkitpro/portlibs/switch" --workdir /work $Image bash /work/build-container.sh 2>&1 | Tee-Object -FilePath $buildLog
$buildExit = $LASTEXITCODE
$ErrorActionPreference = 'Stop'
if ($buildExit -ne 0) { throw "Build failed. See $buildLog" }
Write-Host "Built: $(Join-Path $sourceRoot 'build-switch/echonx.nro')"
