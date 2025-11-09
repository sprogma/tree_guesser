gci *.c | ? {(gi $_ | % LastWriteTime) -ge ((gi "$_.o" 2>$null) | % LastWriteTime)} | % Name | % { gcc -c $_ -o "$_.o" }
gcc (gci *.o) -o a.exe
