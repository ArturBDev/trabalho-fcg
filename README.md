# Trabalho Final - OpenGL (C++ / OpenGL 3.3+)

Este workspace contém um scaffolding mínimo para um projeto em C++ que usa OpenGL 3.3 (core profile), GLFW, GLAD e GLM.

Estrutura mínima:

- `CMakeLists.txt` - configuração do projeto usando FetchContent para baixar dependências
- `src/` - código-fonte (ex.: `main.cpp`)
- `shaders/` - shaders GLSL
- `.vscode/` - tarefas/launch para build e debug

Build e execução (PowerShell - Windows)

1) Gerar com Visual Studio (recomendado):

```powershell
mkdir build; cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

Abra o binário gerado (ex.: `build\Release\app.exe`) ou depure a partir do Visual Studio/VS Code.

2) Gerar com MinGW (exemplo):

```powershell
mkdir build; cd build
cmake -G "MinGW Makefiles" ..
cmake --build . --config Release
```

Observações:
- Você precisará do CMake (>=3.14), um compilador C++17 (Visual Studio ou MinGW) e conexão à internet para o FetchContent baixar dependências.
- Se preferir controlar as dependências manualmente, adapte o `CMakeLists.txt`.
