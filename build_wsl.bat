wsl cmake --preset skshaderc_Linux_x64_Release
wsl cmake --build --preset skshaderc_Linux_x64_Release

mkdir bin\distribute
mkdir bin\distribute\tools
mkdir bin\distribute\tools\linux_x64
mkdir bin\distribute\src

copy bin\intermediate\Linux_x64_Release\skshaderc_exe bin\distribute\tools\linux_x64\skshaderc
copy sk_gpu.h bin\distribute\src\sk_gpu.h
copy skshaderc\CMakeLists.distribute.txt bin\distribute\CMakeLists.txt