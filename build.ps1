$FLAGS = @("-g", "-fsanitize=address", "-D_CRT_SECURE_NO_WARNINGS", "-fms-extensions", "-Wno-microsoft")
$x = @(gci *.h | % LastWriteTime)
(gci *.c) + ($args|%{gi $_}) | ? {($x + @(gi $_ | % LastWriteTime)) -ge ((gi "$_.o" 2>$null) | % LastWriteTime)} | % { Write-Host "Building $($_.FullName)" -Foreground green ; clang $FLAGS -c ($_.FullName) -o "$($_.Name).o" }
Write-Host "Linking..." -Foreground green
clang $FLAGS (gci *.o) -o a.exe
