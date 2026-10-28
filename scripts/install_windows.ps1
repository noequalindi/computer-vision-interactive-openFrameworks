param([switch]$Voice, [switch]$Images)
if ($Images) { $Voice = $true }
$ErrorActionPreference = 'Stop'
$kitDir = Split-Path -Parent $PSScriptRoot
$env:UV_UNMANAGED_INSTALL = Join-Path $kitDir '.tools\uv'
$env:UV_PYTHON_INSTALL_DIR = Join-Path $kitDir '.tools\python'
$env:UV_CACHE_DIR = Join-Path $kitDir '.tools\cache'
$uv = Join-Path $env:UV_UNMANAGED_INSTALL 'uv.exe'
New-Item -ItemType Directory -Force (Join-Path $kitDir '.tools\downloads') | Out-Null
if (!(Test-Path $uv)) {
    $installer = Join-Path $kitDir '.tools\downloads\install-uv.ps1'
    Invoke-WebRequest 'https://astral.sh/uv/install.ps1' -OutFile $installer
    & $installer
    if (!(Test-Path $uv)) { throw 'No se pudo instalar uv.' }
}
$python = Join-Path $kitDir '.venv\Scripts\python.exe'
if (!(Test-Path (Join-Path $kitDir '.venv'))) {
    & $uv python install 3.11
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo preparar Python.' }
    & $uv venv --python 3.11 --managed-python --seed (Join-Path $kitDir '.venv')
    if ($LASTEXITCODE -ne 0) { throw 'No se pudo crear el entorno.' }
}
if ($Voice) {
    & $python -c 'import sys; sys.path.insert(0, sys.argv[1]); from realtime.llm import find_ollama; assert find_ollama()' $kitDir
    if ($LASTEXITCODE -ne 0) {
        $ollamaInstaller = Join-Path $kitDir '.tools\downloads\OllamaSetup.exe'
        Write-Host 'Instalando Ollama. Completar el asistente que se abre.'
        Invoke-WebRequest 'https://ollama.com/download/OllamaSetup.exe' -OutFile $ollamaInstaller
        $install = Start-Process -FilePath $ollamaInstaller -Wait -PassThru
        if ($install.ExitCode -ne 0) { throw 'Instalacion de Ollama cancelada o fallida.' }
        $env:Path = [Environment]::GetEnvironmentVariable('Path','Machine') + ';' + [Environment]::GetEnvironmentVariable('Path','User')
    }
}
$setupArgs = @((Join-Path $kitDir 'setup_local.py'))
if ($Voice) { $setupArgs += '--voice' }
if ($Images) { $setupArgs += '--images' }
& $python @setupArgs
if ($LASTEXITCODE -ne 0) { throw 'No se pudo completar la instalacion.' }
