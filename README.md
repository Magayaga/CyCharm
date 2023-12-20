# CyCharm

**CyCharm** is a simple text editor created and developed by [Cyril John Magayaga](https://github.com/magayaga). It was available for Microsoft Windows.

## Build CyCharm

**Individual components:**

  * **Microsoft Visual Studio 2022**: MSVC v143 - VS 2022 C++ x64/x86 build tools (Latest)
  * **GCC** GNU Compiler Collection: `v13` or later
  * **Windows 11 SDK**

**Pre-requisites**
  * **Code editor** or **IDE**: Microsoft VSCode, JetBrains Fleet, Apache NetBeans, Microsoft Visual Studio, and more.

### Build `cycharm.exe`

  1. Open the [`CyCharm/src`](src)
  2. Build and run the `cycharm.exe`:
     `gcc -o cycharm.exe main.c main.h -mwindows -lcomdlg32 -lcomctl32`

## Copyright

Copyright (c) 2023 Cyril John Magayaga. All rights reserved.
License under the [MIT](LICENSE) license.
