:: Debug version with extra debug info
emcc main.cpp app.cpp ../src/sk_gpu_gl.cpp ../src/sk_gpu_common.cpp -std=c++11 -D SKR_OPENGL -D _DEBUG -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s WASM=1 -s -Oz -o index.js -s EXTRA_EXPORTED_RUNTIME_METHODS=['UTF8ToString'] -g4

:: Release version with minifying flags
::emcc main.cpp app.cpp ../src/sk_gpu_gl.cpp ../src/sk_gpu_common.cpp -std=c++11 -D SKR_OPENGL -s MIN_WEBGL_VERSION=2 -s MAX_WEBGL_VERSION=2 -s ENVIRONMENT=web -s FILESYSTEM=0 -s WASM=1 -s -Oz --closure 1 -o index.js