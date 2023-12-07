@echo off
echo "This build script expects the 'cl' compiler to be available. It can be prepared by running vcvarsall.bat"

cl /Od /DIS_WINDOWS /Zi /FeOutput\\itj_csv_test.exe /FoOutput\\ /EHsc test.c
cl /DIS_WINDOWS /Zi /FeOutput\\itj_csv_example_usage.exe /FoOutput\\ /EHsc example_usage.c
cl /O2 /DIS_WINDOWS /Zi /FeOutput\\itj_csv_generate.exe /FoOutput\\ /EHsc generate.c

