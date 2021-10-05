mkdir build_linux
cd build_linux
wsl cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
wsl cmake --build . -j24 --config MinSizeRel
cd ..

mkdir build_win
cd build_win
cmake .. -DCMAKE_BUILD_TYPE=MinSizeRel
cmake --build . -j24 --config MinSizeRel
cd ..

mkdir distribute
mkdir distribute\example
copy sk_gpu.h distribute\sk_gpu.h
copy build_win\skshaderc\MinSizeRel\skshaderc.exe distribute\skshaderc.exe
copy build_linux\skshaderc\skshaderc distribute\skshaderc

copy build_win\MinSizeRel\skg_flatscreen.exe distribute\example\skg_flatscreen.exe
copy build_linux\skg_flatscreen distribute\example\skg_flatscreen
copy examples\platform.ply distribute\example\platform.ply