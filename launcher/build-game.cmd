@echo off
rem Builds the game from your own UMK3 1.2.59 .ipa without opening the
rem launcher -- the same steps as its COMPILE YOUR .IPA button. The game is
rem built into this folder.
rem
rem     build-game.cmd C:\Users\you\Downloads\UMK3.ipa [--arch x86]
rem
rem The launcher's Electron runs launcher\app\cli.js as plain Node.
set ELECTRON_RUN_AS_NODE=1
"%~dp0launcher\bin\UMK3-Launcher.exe" "%~dp0launcher\bin\resources\app.asar\cli.js" --root "%~dp0." %*
