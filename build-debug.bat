@echo off
call "C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat"
MSBuild.exe Builds\VisualStudio2026\ROOMPAN.sln /p:Configuration=Debug /p:Platform=x64 /m:8