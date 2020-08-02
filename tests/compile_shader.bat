C:\Tools\DXShaderCompiler\bin\dxc.exe -T vs_6_0 -E vs -Fo test.vs.spirv -spirv test.hlsl
C:\Tools\DXShaderCompiler\bin\dxc.exe -T ps_6_0 -E ps -Fo test.ps.spirv -spirv test.hlsl
C:\Tools\spirv-cross\bin\spirv-cross.exe test.vs.spirv --es --version 450 --output test.vs.glsl
C:\Tools\spirv-cross\bin\spirv-cross.exe test.ps.spirv --es --version 450 --output test.ps.glsl

C:\Tools\DXShaderCompiler\bin\dxc.exe -T vs_6_0 -E vs -Fo cubemap.vs.spirv -spirv cubemap.hlsl
C:\Tools\DXShaderCompiler\bin\dxc.exe -T ps_6_0 -E ps -Fo cubemap.ps.spirv -spirv cubemap.hlsl
C:\Tools\spirv-cross\bin\spirv-cross.exe cubemap.vs.spirv --es --version 450 --output cubemap.vs.glsl
C:\Tools\spirv-cross\bin\spirv-cross.exe cubemap.ps.spirv --es --version 450 --output cubemap.ps.glsl