$ErrorActionPreference = 'Stop'

$mode = if ($args -contains '--check') { 'check' } else { 'write' }

$files = git ls-files |
  Select-String -Pattern '\.(c|cc|cpp|cxx|h|hpp|hh|inc)$' |
  Select-String -NotMatch -Pattern '(^|[\/])Shaders[\/]' |
  Select-String -NotMatch -Pattern '(^|[\/])src[\/]Freya[\/]Vendor[\/]' |
  ForEach-Object { $_.ToString().Trim() }

if ($files.Count -eq 0) {
  Write-Host 'No source files to format.'
  exit 0
}

switch ($mode) {
  'write' {
    clang-format -i --Werror $files
    Write-Host "Formatted $($files.Count) files."
  }
  'check' {
    $failed = $false
    clang-format --Werror -n $files
    if ($LASTEXITCODE -ne 0) { $failed = $true }
    if ($failed) {
      Write-Error 'Above files are not clang-format clean. Run .\scripts\format.ps1.'
      exit 1
    }
    Write-Host "All $($files.Count) files are clang-format clean."
  }
}
