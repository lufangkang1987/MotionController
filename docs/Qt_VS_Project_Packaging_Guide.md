# Qt/VS 项目打包安装包教程

本文适用于使用 Visual Studio + Qt VS Tools 创建的 Qt Widgets/C++ 项目，目标是在 Windows 上生成一个带图标、带依赖、可安装的 `.exe` 安装包。

示例项目路径：

```text
D:\demo\source\repos\MotionController
```

## 1. 准备图标

Windows 程序建议准备一个 `.ico` 图标文件。PNG 可以用于界面显示，但 exe 文件图标、桌面快捷方式图标、安装包图标推荐使用 `.ico`。

推荐尺寸：

```text
16x16
24x24
32x32
48x48
64x64
128x128
256x256
```

建议放到项目目录内，例如：

```text
resources\logo.ico
```

不要长期依赖项目外部路径，例如 `D:\xxx\logo.ico`，否则换电脑或复制项目后容易失效。

## 2. 配置 exe 图标

在项目根目录新建资源文件：

```text
MotionController.rc
```

内容如下：

```rc
IDI_ICON1 ICON "resources\\logo.ico"
```

然后在 `.vcxproj` 中加入资源编译项：

```xml
<ItemGroup>
  <ResourceCompile Include="MotionController.rc" />
</ItemGroup>
```

如果希望 Visual Studio 项目树里也能看到它，可以在 `.vcxproj.filters` 中加入：

```xml
<ItemGroup>
  <ResourceCompile Include="MotionController.rc">
    <Filter>Resource Files</Filter>
  </ResourceCompile>
</ItemGroup>
```

这一步决定 exe 文件自身图标，以及桌面快捷方式默认显示的图标。

## 3. 配置 Qt 窗口图标

修改 `.qrc` 文件，例如 `MotionController.qrc`：

```xml
<RCC>
    <qresource prefix="/">
        <file>resources/logo.ico</file>
    </qresource>
</RCC>
```

然后在 `main.cpp` 中设置应用窗口图标：

```cpp
#include "MotionController.h"
#include <QtWidgets/QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon(":/resources/logo.ico"));

    MotionController window;
    window.show();
    return app.exec();
}
```

这一步决定程序运行时，窗口标题栏和任务栏里显示的图标。

## 4. 配置 Release 构建依赖

打包一般使用 `Release | x64`。如果项目依赖第三方库，Debug 和 Release 都要配置 include/lib/dll。

例如项目依赖 `ThirdParty\Zmotion\x64`：

```xml
<ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
  <ClCompile>
    <DisableSpecificWarnings>4996;4828;%(DisableSpecificWarnings)</DisableSpecificWarnings>
    <AdditionalIncludeDirectories>$(ProjectDir)ThirdParty\Zmotion\x64;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
  </ClCompile>
  <Link>
    <AdditionalLibraryDirectories>$(ProjectDir)ThirdParty\Zmotion\x64;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
    <AdditionalDependencies>zauxdll.lib;zmotion.lib;%(AdditionalDependencies)</AdditionalDependencies>
  </Link>
  <PostBuildEvent>
    <Command>copy "$(ProjectDir)ThirdParty\Zmotion\x64\*.dll" "$(OutDir)"</Command>
  </PostBuildEvent>
</ItemDefinitionGroup>
```

如果第三方头文件编码不是 UTF-8，而 Qt/MSBuild 编译命令带了 `-utf-8`，可能会出现大量 `C4828` 警告。可以把 `4828` 加入禁用警告，避免 Visual Studio 输出窗口被刷爆。

## 5. 使用 Release x64 编译

在 Visual Studio 顶部选择：

```text
Release | x64
```

然后执行：

```text
生成 -> 重新生成解决方案
```

成功后通常会生成：

```text
x64\Release\YourApp.exe
```

例如：

```text
x64\Release\MotionController.exe
```

## 6. 使用 windeployqt 收集 Qt 依赖

Qt 程序不能只复制 exe，通常还需要 Qt DLL、platforms 插件等运行时文件。

可以手动执行：

```powershell
cd D:\demo\source\repos\MotionController

mkdir package\MotionController
copy x64\Release\MotionController.exe package\MotionController\
copy ThirdParty\Zmotion\x64\*.dll package\MotionController\

D:\Qt\6.11.1\msvc2022_64\bin\windeployqt.exe --release --compiler-runtime package\MotionController\MotionController.exe
```

执行完成后，`package\MotionController` 就是一个基本可运行的发布目录。

## 7. 编写打包脚本

可以在项目根目录创建 `package.ps1`：

```powershell
$ErrorActionPreference = "Stop"

$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$QtBin = "D:\Qt\6.11.1\msvc2022_64\bin"
$AppName = "MotionController"
$ExePath = Join-Path $ProjectRoot "x64\Release\$AppName.exe"
$PackageDir = Join-Path $ProjectRoot "package\$AppName"
$PackageExe = Join-Path $PackageDir "$AppName.exe"
$Windeployqt = Join-Path $QtBin "windeployqt.exe"
$IssFile = Join-Path $ProjectRoot "installer\MotionController.iss"

if (-not (Test-Path $ExePath)) {
    throw "Release exe not found: $ExePath. Build Release x64 in Visual Studio first."
}

if (-not (Test-Path $Windeployqt)) {
    throw "windeployqt not found: $Windeployqt"
}

if (Test-Path $PackageDir) {
    Remove-Item $PackageDir -Recurse -Force
}

New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
Copy-Item $ExePath $PackageExe -Force
Copy-Item (Join-Path $ProjectRoot "ThirdParty\Zmotion\x64\*.dll") $PackageDir -Force

& $Windeployqt --release --compiler-runtime $PackageExe

$Iscc = Get-Command iscc.exe -ErrorAction SilentlyContinue
if (-not $Iscc) {
    $CandidatePaths = @(
        "D:\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles}\Inno Setup 6\ISCC.exe",
        "${env:ProgramFiles(x86)}\Inno Setup 6\ISCC.exe",
        "${env:LOCALAPPDATA}\Programs\Inno Setup 6\ISCC.exe"
    )

    foreach ($CandidatePath in $CandidatePaths) {
        if ($CandidatePath -and (Test-Path $CandidatePath)) {
            $Iscc = [pscustomobject]@{ Source = $CandidatePath }
            break
        }
    }
}

if (-not $Iscc) {
    Write-Host "Package folder is ready: $PackageDir"
    Write-Host "Inno Setup compiler was not found. Install Inno Setup 6, then compile: $IssFile"
    exit 0
}

& $Iscc.Source $IssFile
```

如果 PowerShell 提示禁止运行脚本，可以用：

```powershell
powershell -ExecutionPolicy Bypass -File .\package.ps1
```

## 8. 使用 Inno Setup 生成安装包

安装 Inno Setup 6 后，在项目中创建：

```text
installer\MotionController.iss
```

示例内容：

```iss
[Setup]
AppId={{67DC4617-2D1B-4E9C-9315-FF8FA718953F}
AppName=MotionController
AppVersion=1.0.0
DefaultDirName={autopf}\MotionController
DefaultGroupName=MotionController
OutputDir=..\dist
OutputBaseFilename=MotionControllerSetup
SetupIconFile=..\resources\logo.ico
Compression=lzma
SolidCompression=yes
ArchitecturesAllowed=x64
ArchitecturesInstallIn64BitMode=x64

[Files]
Source: "..\package\MotionController\*"; DestDir: "{app}"; Flags: recursesubdirs ignoreversion

[Tasks]
Name: "desktopicon"; Description: "Create a desktop shortcut"; GroupDescription: "Additional tasks:"

[Icons]
Name: "{group}\MotionController"; Filename: "{app}\MotionController.exe"
Name: "{commondesktop}\MotionController"; Filename: "{app}\MotionController.exe"; Tasks: desktopicon

[Run]
Filename: "{app}\MotionController.exe"; Description: "Launch MotionController"; Flags: nowait postinstall skipifsilent
```

其中：

- `SetupIconFile` 控制安装包 exe 图标。
- `[Files]` 把 `package\MotionController` 下的所有发布文件装到 `{app}`。
- `[Icons]` 创建开始菜单和桌面快捷方式。
- `[Run]` 控制安装结束后是否启动程序。

运行 `package.ps1` 后，安装包通常输出到：

```text
dist\MotionControllerSetup.exe
```

## 9. 后续版本更新后如何重新打包

程序后续有新版本时，不需要重新做整套配置，只要按固定发布流程重新构建和打包。

### 1. 修改代码并确认功能

先完成代码修改，在 Debug 或 Release 下确认程序功能正常。

### 2. 修改安装包版本号

打开：

```text
installer\MotionController.iss
```

修改：

```iss
AppVersion=1.0.1
OutputBaseFilename=MotionControllerSetup_1.0.1
```

其中：

- `AppVersion` 是安装包版本号。
- `OutputBaseFilename` 是最终生成的安装包文件名。

如果不修改 `OutputBaseFilename`，新安装包会覆盖旧的 `MotionControllerSetup.exe`。

### 3. 重新生成 Release 程序

在 Visual Studio 中切换到：

```text
Release | x64
```

然后执行：

```text
生成 -> 重新生成解决方案
```

确认生成：

```text
x64\Release\MotionController.exe
```

### 4. 重新执行打包脚本

在 PowerShell 中进入项目目录：

```powershell
cd D:\demo\source\repos\MotionController
```

执行：

```powershell
powershell -ExecutionPolicy Bypass -File .\package.ps1
```

脚本会重新清理并生成：

```text
package\MotionController
```

然后调用 Inno Setup 生成安装包。

### 5. 获取新安装包

安装包输出目录：

```text
dist
```

例如：

```text
dist\MotionControllerSetup_1.0.1.exe
```

### 6. 更新时需要额外检查的内容

如果新版本增加了文件，需要确认它们被复制进发布目录：

- 新增第三方 DLL。
- 新增配置文件。
- 新增数据库文件。
- 新增模型、图片、模板等资源文件。
- 新增 Qt 模块或插件。

如果只是修改 C++ 代码或 UI，一般重新生成 Release 并重新运行 `package.ps1` 即可。

注意：正式发布建议始终使用 `Release | x64`，不要把 Debug 版本作为安装包发布。

## 10. 常见问题

### PowerShell 禁止运行脚本

错误类似：

```text
因为在此系统上禁止运行脚本
```

使用：

```powershell
powershell -ExecutionPolicy Bypass -File .\package.ps1
```

### 找不到 Inno Setup Compiler

确认 `ISCC.exe` 的真实路径，例如：

```text
D:\Inno Setup 6\ISCC.exe
```

然后把该路径加入 `package.ps1` 的 `$CandidatePaths`。

### 程序安装后无法启动

常见原因：

- 没有运行 `windeployqt`。
- 少复制了第三方 DLL。
- 用 Debug 版本打包，目标机器没有 Debug 运行库。
- Qt 插件目录 `platforms` 缺失。

建议始终使用 `Release | x64` 打包。

### exe 没有显示图标

检查：

- `.rc` 是否加入 `.vcxproj`。
- `.rc` 中图标路径是否正确。
- 是否重新生成了项目。
- Windows 图标缓存可能未刷新，可以换文件名或重启资源管理器。

### 窗口运行时图标不显示

检查：

- `.qrc` 是否包含图标文件。
- `main.cpp` 是否调用 `app.setWindowIcon(...)`。
- Qt 资源路径是否写成 `:/resources/logo.ico`。

## 11. 推荐最终目录结构

```text
MotionController
├─ MotionController.vcxproj
├─ MotionController.qrc
├─ MotionController.rc
├─ main.cpp
├─ resources
│  └─ logo.ico
├─ ThirdParty
│  └─ Zmotion
├─ installer
│  └─ MotionController.iss
├─ package.ps1
├─ package
│  └─ MotionController
└─ dist
   └─ MotionControllerSetup.exe
```

实际提交代码时，通常提交：

- `resources\logo.ico`
- `MotionController.rc`
- `.vcxproj` / `.vcxproj.filters`
- `.qrc`
- `main.cpp`
- `installer\MotionController.iss`
- `package.ps1`

通常不要提交：

- `.vs`
- `x64`
- `package`
- `dist`
- 其他临时构建目录
