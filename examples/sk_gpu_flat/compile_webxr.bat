:: Debug version with extra debug info
emcc^
 main.cpp^
 ../common/app.cpp^
 ../../src/sk_gpu_gl.cpp^
 ../../src/sk_gpu_common.cpp^
 -std=c++11^
 -D _DEBUG^
 -s MIN_WEBGL_VERSION=2^
 -s MAX_WEBGL_VERSION=2^
 -s WASM=1^
 -s -Oz^
 -o index.js^
 --js-library ../common/library_webxr.js^
 -g4

:: Release version with minifying flags
::emcc^
:: main.cpp^
:: ../common/app.cpp^
:: ../../src/sk_gpu_gl.cpp^
:: ../../src/sk_gpu_common.cpp^
:: -std=c++11^
:: -s MIN_WEBGL_VERSION=2^
:: -s MAX_WEBGL_VERSION=2^
:: -s WASM=1^
:: -s -Oz^
:: -s ENVIRONMENT=web^
:: -s FILESYSTEM=0^
:: --closure 1^
:: -o index.js