$x = @(gci *.h | % LastWriteTime)
gci *.c | ? {($x + @(gi $_ | % LastWriteTime)) -ge ((gi "$_.o" 2>$null) | % LastWriteTime)} | % Name | % { Write-Host "Building $_" -Foreground green ; gcc -c $_ -o "$_.o" }
Write-Host "Linking..." -Foreground green
gcc (gci *.o) -o a.exe
