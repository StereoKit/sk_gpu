cmake --preset skshaderc_Win32_x64_Release
cmake --build --preset skshaderc_Win32_x64_Release

mkdir bin\distribute
mkdir bin\distribute\tools
mkdir bin\distribute\tools\win32_x64
mkdir bin\distribute\src

copy bin\intermediate\Win32_x64\Release\skshaderc_exe.exe bin\distribute\tools\win32_x64\skshaderc.exe
copy sk_gpu.h bin\distribute\src\sk_gpu.h
copy skshaderc\CMakeLists.distribute.txt bin\distribute\CMakeLists.txt