@echo off
echo "This build script expects the 'cl' compiler to be available. It can be prepared by running vcvarsall.bat"

cl /O2 /DIS_WINDOWS /Zi /FeOutput/itj_csv_test.exe /FoOutput\\ /EHsc test.c tracy\\public\\TracyClient.cpp
cl /DIS_WINDOWS /O2 /Zi /FeOutput/itj_csv_example_usage.exe /FoOutput\\ /EHsc example_usage.c
cl /Ot /DIS_WINDOWS /Zi /FeOutput/itj_csv_generate.exe /FoOutput\\ /EHsc generate.c
