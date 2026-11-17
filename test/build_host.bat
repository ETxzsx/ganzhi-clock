@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Insiders\VC\Auxiliary\Build\vcvars64.bat" >nul
cd /d "G:\AI data\esp32-ganzhi"
cl /nologo /utf-8 /std:c11 /O2 /I main /I test\host_shim test\host_test.c main\gz_core.c /Fe:test\host_test.exe /Fo:test\