$FLAGS = @("-g")
$x = @(gci *.h | % LastWriteTime)
gci *.c | ? {($x + @(gi $_ | % LastWriteTime)) -ge ((gi "$_.o" 2>$null) | % LastWriteTime)} | % Name | % { Write-Host "Building $_" -Foreground green ; gcc $FLAGS -c $_ -o "$_.o" }
Write-Host "Linking..." -Foreground green
gcc $FLAGS (gci *.o) -o a.exe
