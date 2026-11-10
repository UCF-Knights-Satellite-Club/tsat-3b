# Builds and runs the PC simulator (tools/pc_sim.cpp) against the real guidance code.
#
# Needs a C++ compiler. Either:
#   - g++ on the PATH (MSYS2 / MinGW), or
#   - Zig from PyPI:   py -m pip install ziglang
#
# Examples (from the sketch folder):
#   powershell -File tools\run_pc_sim.ps1              all scenarios, summary table
#   powershell -File tools\run_pc_sim.ps1 -v           plus every single drop
#   powershell -File tools\run_pc_sim.ps1 1 run1.csv   one scenario with a full log
#   py tools\plot_sim.py run1.csv                      -> run1.svg (open in a browser)
param([Parameter(ValueFromRemainingArguments = $true)] $SimArgs)

$root = Split-Path $PSScriptRoot -Parent
$sources = @('tools\pc_sim.cpp', 'Guidance.cpp', 'WindEstimator.cpp', 'HeadingController.cpp',
             'FlightCore.cpp', 'ParafoilSim.cpp') | ForEach-Object { Join-Path $root $_ }
$exe = Join-Path $PSScriptRoot 'pc_sim.exe'

if (Get-Command g++ -ErrorAction SilentlyContinue) {
  g++ -O2 -std=c++17 -I $root @sources -o $exe
} else {
  py -m ziglang c++ -O2 -std=c++17 -I $root @sources -o $exe
}
if ($LASTEXITCODE -ne 0) { Write-Error 'build failed (is a compiler installed? see the top of this file)'; exit 1 }
& $exe @SimArgs
