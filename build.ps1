$FLAGS = @("-g", "-D_CRT_SECURE_NO_WARNINGS", "-fms-extensions", "-Wno-microsoft")
# $FLAGS += @("-fsanitize=address")
$x = @(gci *.h | % LastWriteTime)
$f = (gci *.c) + ($args|%{gi $_})
$f | ? {($x + @(gi $_ | % LastWriteTime)) -ge ((gi "$_.o" 2>$null) | % LastWriteTime)} | % { Write-Host "Building $($_.FullName)" -Foreground green ; clang $FLAGS -c ($_.FullName) -o "$($_.Name).o" }
Write-Host "Linking..." -Foreground green
clang $FLAGS ($f | % Name | %{"$_.o"}) -o a.exe
