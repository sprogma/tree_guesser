$FLAGS = @("-g", "-fsanitize=address", "-D_CRT_SECURE_NO_WARNINGS", "-fms-extensions", "-Wno-microsoft")
$x = @(gci *.h | % LastWriteTime)
gci *.c | ? {($x + @(gi $_ | % LastWriteTime)) -ge ((gi "$_.o" 2>$null) | % LastWriteTime)} | % Name | % { Write-Host "Building $_" -Foreground green ; clang $FLAGS -c $_ -o "$_.o" }
Write-Host "Linking..." -Foreground green
clang $FLAGS (gci *.o) -o a.exe
