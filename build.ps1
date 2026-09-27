param(
    [ValidateSet('Release','Debug')][string]$Configuration = 'Release',
    [switch]$TestsOnly,
    [string]$JuceDir = 'C:\SDK\JUCE'
)

$ErrorActionPreference = 'Stop'
$vsRoot = 'C:\Program Files\Microsoft Visual Studio\18\Community'
$cmake = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe'
$ninja = Join-Path $vsRoot 'Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja\ninja.exe'
$devShell = Join-Path $vsRoot 'Common7\Tools\VsDevCmd.bat'
$buildDir = Join-Path $PSScriptRoot $(if ($TestsOnly) { 'build-tests' } else { 'build-plugin' })
$pluginFlag = if ($TestsOnly) { 'OFF' } else { 'ON' }

if (!(Test-Path -LiteralPath $JuceDir)) { throw "JUCE not found at $JuceDir" }
if (!(Test-Path -LiteralPath $cmake)) { throw "CMake not found at $cmake" }

$command = 'call "{0}" -arch=amd64 -host_arch=amd64 >nul && "{1}" -S "{2}" -B "{3}" -G Ninja -DCMAKE_MAKE_PROGRAM="{4}" -DCMAKE_BUILD_TYPE={5} -DJUCE_DIR="{6}" -DLONGLAND_BUILD_PLUGIN={7} -DLONGLAND_BUILD_TESTS=ON && "{1}" --build "{3}" --config {5} && "{1}" --build "{3}" --target test --config {5}' -f $devShell,$cmake,$PSScriptRoot,$buildDir,$ninja,$Configuration,$JuceDir,$pluginFlag
cmd.exe /d /c $command
if ($LASTEXITCODE -ne 0) { throw "Long Land build or tests failed with exit code $LASTEXITCODE" }
