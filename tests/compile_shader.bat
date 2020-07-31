C:\Tools\DXShaderCompiler\bin\dxc.exe -T vs_6_0 -E vs -Fo test.vs.spirv -spirv test.hlsl
C:\Tools\DXShaderCompiler\bin\dxc.exe -T ps_6_0 -E ps -Fo test.ps.spirv -spirv test.hlsl
:: C:\Tools\DXShaderCompiler\bin\dxc.exe -T vs_6_0 -E vs -Fre test.vs.meta -spirv test.hlsl
:: C:\Tools\DXShaderCompiler\bin\dxc.exe -T ps_6_0 -E ps -Fre test.ps.meta -spirv test.hlsl
C:\Tools\spirv-cross\bin\spirv-cross.exe test.vs.spirv --es --version 450 --output test.vs.glsl
C:\Tools\spirv-cross\bin\spirv-cross.exe test.ps.spirv --es --version 450 --output test.ps.glsl