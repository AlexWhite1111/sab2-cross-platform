$ErrorActionPreference = 'Stop'
Set-Location (Join-Path $PSScriptRoot '..')
cmake -S . -B build-windows -DCMAKE_BUILD_TYPE=Release -DSAB2_WITH_OPENCV=ON
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build build-windows --config Release --parallel
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
ctest --test-dir build-windows -C Release --output-on-failure
exit $LASTEXITCODE
