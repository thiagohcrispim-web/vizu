@echo off
rem Arraste um arquivo (PDF, INDD, PSD...) para cima deste .bat.
rem Sem arquivo (dois cliques): diagnóstico do Espaço e do Alt.
"%~dp0Vizu.exe" /diag "%~1"
