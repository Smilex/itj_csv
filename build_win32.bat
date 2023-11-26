@echo off
echo "This build script expects the 'cl' compiler to be available. It can be prepared by running vcvarsall.bat"

cl /Itracy\\public /Od /DIS_WINDOWS /Zi /FeDebug/itj_csv_test.exe /FoDebug\\ /EHsc test.c
cl /DIS_WINDOWS /Zi /FeDebug/itj_csv_example_usage.exe /FoDebug\\ /EHsc example_usage.c
cl /Ot /DIS_WINDOWS /Zi /FeDebug/itj_csv_generate.exe /FoDebug\\ /EHsc generate.c

