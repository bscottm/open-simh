# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
# 
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
# 
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
# THE AUTHORS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
# IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
# CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
# 
# Except as contained in this notice, the names of The Authors shall not be
# used in advertising or otherwise to promote the sale, use or other dealings
# in this Software without prior written authorization from the Authors.
#
# Author: B. Scott Michel (scooter.phd@gmail.com)
# "scooter me fecit"

<#
.SYNOPSIS
Configure and build SIMH's dependencies and simulators using the Microsoft Visual
Studio C compiler or MinGW-W64-based gcc compiler.

.DESCRIPTION
This script executes the four (4) phases of building the entire suite of SIMH
simulators using the CMake meta-build tool. The phases are:

1. Configure and generate the build environment selected by '-flavor' option.
2. Build missing runtime dependencies and the simulator suite with the compiler
   configuration selected by the '-config' option. The "Release" configuration
   generates optimized executables; the "Debug" configuration generates
   development executables with debugger information.
3. Test the simulators
4. Install the simulators in the source directory's BIN subdirectory.

The test and install phases can be enabled or disabled by the appropriate command line
flag (e.g., '-noInstall', '-noTest', '-testOnly', '-installOnly'.)

Build environment and artifact locations:
-----------------------------------------
cmake/build-vs*          MSVC build products and artifacts
cmake/build-mingw        MinGW-W64 products and artifacts
cmake/build-ninja        Ninja builder products and artifacts

.EXAMPLE
PS> cmake-builder.ps1 -flavor vs2022 -config Release

Generate/configure, build, test and install the SIMH simulator suite using
the Visual Studio 2022 toolchain in the Release (optimized) compile
configuration.

.EXAMPLE
PS> cmake-builder.ps1 vs2022 Release

Another way to generate/configure, build, test and install the SIMH simulator
suite using the Visual Studio 2022 toolchain in the Release (optimized)
compile configuration.

.EXAMPLE
PS> cmake-builder.ps1 vs2019 Debug -notest -noinstall

Generate/configure and build the SIMH simulator suite with the Visual Studio
2019 toolchain in the Debug compile configuration. Does not execute tests and
does not install the simulators under the BIN subdirectory in the top of the
source tree.

.EXAMPLE

PS> cmake-builder.ps1 -flavor vs2019 -config Release -installonly

Install the simulators under the BIN subdirectory in the top of the source
tree. Does not generate/configure, but will build to ensure that compile
targets (simulator executables) are up-to-date.
#>

param (
    ## String arguments are positional, so if the user invokes this script
    ## as "cmake-builder.ps1 vs2022 Debug", it's the same as saying
    ## "cmake-builder.ps1 -flavor vs2022 -config Debug"


    ## The build environment's "flavor" that determines which CMake generator is used
    ## to create all of the build machinery to compile the SIMH simulator suite
    ## and the target compiler.
    ## 
    ## Supported flavors:
    ## ------------------
    ## vs2022          Visual Studio 2022 (default)
    ## vs2022-xp       Visual Studio 2022 XP compat
    ## vs2019          Visual Studio 2019
    ## vs2019-xp       Visual Studio 2019 XP compat
    ## vs2017          Visual Studio 2017
    ## vs2017-xp       Visual Studio 2017 XP compat
    ## vs2015          Visual Studio 2015
    ## mingw-make      MinGW GCC/mingw32-make
    ## mingw-ninja     MinGW GCC/ninja
    [Parameter(Mandatory=$false)]
    [string] $flavor         = "vs2022",

    ## The target build configuration. Valid values: "Release" and "Debug"
    [Parameter(Mandatory=$false)]
    [string] $config         = "Release",

    ## (optional) Simulator to build (e.g., 'vax', 'pdp11', 'pdp8', ...)
    [Parameter(Mandatory=$false)]
    [string] $target         = "",

    ## The rest are flag arguments

    ## Clean (remove) the CMake build directory before configuring
    [Parameter(Mandatory=$false)]
    [switch] $clean          = $false,
    [Parameter(Mandatory=$false)]

    ## Get help.
    [switch] $help           = $false,
    [Parameter(Mandatory=$false)]

    ## Compile the SIMH simulator suite without network support.
    [switch] $nonetwork      = $false,
    [Parameter(Mandatory=$false)]

    ## Compile the SIMH simulator suite without video support.
    [switch] $novideo        = $false,
    [Parameter(Mandatory=$false)]

    ## Disable the build's tests.
    [switch] $notest         = $false,
    [Parameter(Mandatory=$false)]

    ## Do not install the simulator suite in the source directory's BIN
    ## subdirectory.
    [switch] $noinstall      = $false,
    [Parameter(Mandatory=$false)]

    ## Enable parallel builds.
    [switch] $parallel       = $false,
    [Parameter(Mandatory=$false)]

    ## Configure and generate the build environment. Don't compile, test or install.
    [switch] $generate       = $false,
    [Parameter(Mandatory=$false)]

    ## Delete the CMake cache, configure and regenerate the build environment.
    ## Don't compile, test or install.
    [switch] $regenerate     = $false,

    ## Only run the tests.
    [Parameter(Mandatory=$false)]
    [switch] $testonly       = $false,

    ## Only install the SIMH simulator suite in the source directory's BIN
    ## subdirectory.
    [Parameter(Mandatory=$false)]
    [switch] $installOnly    = $false,

    ## Turn on Windows API deprecation warnings. NOTE: These warnings are OFF by
    ## default.
    [Parameter(Mandatory=$false)]
    [switch] $windeprecation = $false,

    ## Use CPack to create an installer package (EXPERIMENTAL)
    [Parameter(Mandatory=$false)]
    [switch] $package        = $false,

    ## Enable Link-Time Optimization (LTO).
    [Parameter(Mandatory=$false)]
    [switch] $lto            = $false,

    ## Turn on maximal compiler warnings for Debug builds (e.g. "-Wall" or "/W3")
    [Parameter(Mandatory=$false)]
    [switch] $debug_wall     = $false
)

$scriptName = $(Split-Path -Leaf $PSCommandPath)
$scriptCmd  = ${PSCommandPath}

function Show-Help
{
    Get-Help -full ${scriptCmd}
    exit 0
}


## CMake generator info:
class GeneratorInfo
{
    [string]  $Generator
    [bool]    $SingleConfig
    [bool]    $UCRT
    [string]  $UCRTVersion
    [string[]]$ArchArgs

    GeneratorInfo([string]$gen, $configFlag, $ucrtFlag, $ucrtVer, [string[]]$arch)
    {
        $this.Generator = $gen
        $this.SingleConfig = $configFlag
        $this.UCRT = $ucrtFlag
        $this.UCRTVersion = $ucrtVer
        $this.ArchArgs  = $arch
    }
}

## Multiple build configurations selected at compile time
$multiConfig = $false
## Single configuration selected at configuration time
$singleConfig = $true

$cmakeGenMap = @{
    "vs2022"      = [GeneratorInfo]::new("Visual Studio 17 2022", $multiConfig,  $false, "",     @("-A", "Win32"));
    "vs2022-xp"   = [GeneratorInfo]::new("Visual Studio 17 2022", $multiConfig,  $false, "",     @("-A", "Win32", "-T", "v141_xp"));
    "vs2019"      = [GeneratorInfo]::new("Visual Studio 16 2019", $multiConfig,  $false, "",     @("-A", "Win32"));
    "vs2019-xp"   = [GeneratorInfo]::new("Visual Studio 16 2019", $multiConfig,  $false, "",     @("-A", "Win32", "-T", "v141_xp"));
    "vs2017"      = [GeneratorInfo]::new("Visual Studio 15 2017", $multiConfig,  $false, "",     @("-A", "Win32"));
    "vs2017-xp"   = [GeneratorInfo]::new("Visual Studio 15 2017", $multiConfig,  $false, "",     @("-A", "Win32", "-T", "v141_xp"));
    "vs2015"      = [GeneratorInfo]::new("Visual Studio 14 2015", $multiConfig,  $false, "",     @());
    "mingw-make"  = [GeneratorInfo]::new("MinGW Makefiles",       $singleConfig, $false, "",     @());
    "mingw-ninja" = [GeneratorInfo]::new("Ninja",                 $singleConfig, $false, "",     @())
}


function Get-GeneratorInfo([string]$flavor)
{
    return $cmakeGenMap[$flavor]
}

function Quote-Args([string[]]$arglist)
{
    return ($arglist | foreach-object { if ($_ -like "* *") { "`"$_`"" } else { $_ } })
}


## Output help early and exit.
if ($help)
{
    Show-Help
}

### CTest params:
## timeout is 180 seconds
$ctestTimeout  = "300"

## Sanity checking: Check that utilities we expect exist...
## CMake: Save the location of the command because we'll invoke it later. Same
## with CTest
$cmakeCmd = $(Get-Command -Name cmake.exe -ErrorAction Ignore).Path
$ctestCmd = $(Get-Command -Name ctest.exe -ErrorAction Ignore).Path
$cpackCmd = $(Get-Command -Name cpack.exe -ErrorAction Ignore).Path
if ($cmakeCmd.Length -gt 0)
{
    Write-Host "** ${scriptName}: cmake is '${cmakeCmd}'"
    Write-Host "** $(& ${cmakeCmd} --version)"
}
else {
    @"
!! ${scriptName} error:

The 'cmake' command was not found. Please ensure that you have installed CMake
and that your PATH environment variable references the directory in which it
was installed.
"@

    exit 1
}

if ($package) {
    if ($cpackCmd.Length -gt 0)
    {
        Write-Host "** ${scriptName}: cpack is '${cpackCmd}'"
    }
    else {
        @"
!! ${scriptName} error:

The 'cpack' command was not found -- unable to package the SIMH simulator
suite. Please check your CMake installation and PATH environment variable.
"@

        exit 1
    }
}

if (!$testonly)
{
    ## Check for GCC and mingw32-make if user wants the mingw flavor build.
    if ($flavor -eq "mingw" -or $flavor -eq "ninja")
    {
        if ($(Get-Command gcc -ErrorAction Ignore).Path.Length -eq 0) {
            @"
    !! ${scriptName} error:

    Did not find 'gcc', the GNU C/C++ compiler toolchain. Please ensure you have
    installed gcc and that your PATH environment variables references the directory
    in which it was installed.
"@
            exit 1
        }

        if ($(Get-Command mingw32-make -ErrorAction Ignore).Path.Length -eq 0) {
            @"
    !! ${scriptName} error:

    Did not find 'mingw32-make'. Please ensure you have installed mingw32-make and
    that your PATH environment variables references the directory in which it was
    installed.

    See the .travis/deps.sh functions mingw64() and ucrt64() for the pacman packages
    that should be installed.
"@
            exit 1
        }
    }
}

## Validate the requested configuration.
if (!@("Release", "Debug").Contains($config))
{
    @"
${scriptName}: Invalid configuration: "${config}".

"@
    Show-Help
}

## Look for Git's /usr/bin subdirectory: CMake (and other utilities) have issues
## with the /bin/sh installed there (Git's version of MinGW.)

$tmp_path = $env:PATH
$git_usrbin = "${env:ProgramFiles}\Git\usr\bin"
$tmp_path = ($tmp_path.Split(';') | Where-Object { $_ -ne "${git_usrbin}"}) -join ';'
if ($tmp_path -ne ${env:PATH})
{
    Write-Host "** ${scriptName}: Removed ${git_usrbin} from PATH (Git MinGW problem)"
    $env:PATH = $tmp_path
}

## Also make sure that none of the other cmake-* directories are in the user's PATH
## because CMake's find_package does traverse PATH looking for potential candidates
## for dependency libraries.

$origPath = $env:PATH
$modPath  = $origPath

if (Test-Path -Path cmake\dependencies) {
  $bdirs = $(Get-ChildItem -Attribute Directory cmake\dependencies\*).ForEach({ $_.FullName + "\bin" })
  $modPath  = (${env:Path}.Split(';') | Where-Object { $bdirs -notcontains $_ }) -join ';'
  if ($modPath -ne $origPath) {
    Write-Host "** ${scriptName}: Removed cmake\dependencies 'bin' directories from PATH."
  }
}

## Setup:
$simhTopDir = $(Split-Path -Parent $(Resolve-Path -Path $PSCommandPath).Path)
While (!([String]::IsNullOrEmpty($simhTopDir) -or (Test-Path -Path ${simhTopDir}\CMakeLists.txt))) {
    $simhTopDir = $(Split-Path -Parent $simhTopDir)
}
if ([String]::IsNullOrEmpty($simhTopDir)) {
    @"
!! ${scriptName}: Cannot locate SIMH top-level source directory from
the script's path name. You should really not see this message.
"@

    exit 1
} else {
    Write-Host "** ${scriptName}: SIMH top-level source directory is ${simhTopDir}"
}

$buildDir  = "${simhTopDir}\cmake\build-${flavor}"
$genInfo = $(Get-GeneratorInfo $flavor)
if ($null -eq $genInfo)
{
    Write-Host ""
    Write-Host "!! ${scriptName}: Unrecognized build flavor '${flavor}'."
    Write-Host ""
    Show-Help
}

if ($regenerate)
{
  $generate = $true;
}

if ($testonly)
{
    $scriptPhases = @("test")
}
elseif ($generate)
{
    $scriptPhases = @("generate")
}
elseif ($installOnly)
{
    $scriptPhases = @("install")
}
elseif ($package) {
    $scriptPhases = @("package")
}
else
{
  $scriptPhases = @( "generate", "build", "test", "install")
  if ($notest)
  {
      $scriptPhases = $scriptPhases | Where-Object { $_ -ne 'test' }
  }
  if ($noinstall -or ![String]::IsNullOrEmpty($target))
  {
      $scriptPhases = $scriptPhases | Where-Object { $_ -ne 'install' }
  }
}

if (($scriptPhases -contains "generate") -or ($scriptPhases -contains "build"))
{
    ## Clean out the build subdirectory
    if ((Test-Path -Path ${buildDir}) -and $clean)
    {
        Write-Host "** ${scriptName}: Removing ${buildDir}"
        Remove-Item -recurse -force -Path ${buildDir} -ErrorAction Continue | Out-Null
    }

    if (!(Test-Path -Path ${buildDir}))
    {
        Write-Host "** ${scriptName}: Creating ${buildDir} subdirectory"
        New-Item -Path ${buildDir} -ItemType Directory | Out-Null
    }
    else
    {
        Write-Host "** ${scriptName}: ${buildDir} exists."
    }

    ## Need to regenerate?
    if ($regenerate)
    {
      Remove-Item          -Force -Path ${buildDir}/CMakeCache.txt -ErrorAction Continue | Out-Null
      Remove-Item -Recurse -Force -Path ${buildDir}/CMakeFiles     -ErrorAction Continue | Out-Null
    }
   
    ## Where we do the heaving lifting:
    $generateArgs = @("-G", $genInfo.Generator)
    if ($genInfo.SingleConfig) {
        ## Single configuration set at compile time:
        $generateArgs += @("-DCMAKE_BUILD_TYPE=${config}")
    }
    if ($genInfo.UCRT) {
        ## Universal Windows Platform
        $generateArgs += @("-DCMAKE_SYSTEM_NAME=WindowsStore", "-DCMAKE_SYSTEM_VERSION=$($genInfo.UCRTVersion)")
    }
    $generateArgs += $genInfo.ArchArgs + @("-Wno-dev", "--no-warn-unused-cli")
    if ($nonetwork)
    {
        $generateArgs += @("-DWITH_NETWORK:Bool=Off")
    }
    if ($novideo)
    {
      $generateArgs += @("-DWITH_VIDEO:Bool=Off")
    }
    if ($lto)
    {
        $generateArgs += @("-DRELEASE_LTO:Bool=On")
    }
    if ($debug_wall)
    {
        $generateArgs += @("-DDEBUG_WALL:Bool=On")
    }

    $buildArgs     =  @("--build", "${buildDir}", "--config", "${config}")
    if ($parallel)
    {
      $buildArgs += "--parallel"
    }
    if ($verbose)
    {
      $buildArgs += "--verbose"
    }
    if ($windeprecation)
    {
        $buildArgs += "-DWINAPI_DEPRECATION:Bool=TRUE"
    }
    if (![String]::IsNullOrEmpty($target)) {
        $buildArgs += @("--target", "$target")
    }
    
    $buildSpecificArgs = @()
    if ($flavor -eq "mingw" -and $parallel)
    {
      ## Limit the number of parallel jobs mingw32-make can spawn. Otherwise
      ## it'll overwhelm the machine.
      $buildSpecificArgs += @("-j",  "8")
    }
}

$exitval = 0

foreach ($phase in $scriptPhases) {
    $savedPATH = $env:PATH
    $argList = @()
    $phaseCommand = "Write-Output"

    switch -exact ($phase)
    {
        "generate" {
            $generateArgs += @("-S", ${simhTopDir})
            $generateArgs += @("-B", ${buildDir})

            Write-Host "** ${scriptName}: Configuring and generating"

            $phaseCommand = ${cmakeCmd}
            $argList = Quote-Args $generateArgs
        }

        "build" {
            Write-Host "** ${scriptName}: Building simulators."

            $phaseCommand = ${cmakeCmd}
            $argList = $(Quote-Args $buildArgs) + $(Quote-Args $buildSpecificArgs)
        }

        "test" {
            Write-Host "** ${scriptName}: Testing simulators."

            ## CTest arguments:
            $testArgs = @("-C", $config, "--timeout", $ctestTimeout, "-T", "test",
                          "--output-on-failure")

            ## Output gets confusing (and tests can time out when executing in parallel)
            ## if ($parallel)
            ## {
            ##     $testArgs += @("--parallel", $ctestParallel)
            ## }

            if ($verbose)
            {
                $testArgs += @("--verbose")
            }

            if (![String]::IsNullOrEmpty($target)) {
                $testArgs += @("-R", "simh-${target}")
            }
         
            $phaseCommand = ${ctestCmd}
            $argList = Quote-Args $testArgs

            $env:PATH = $modPath

            $depTopDir = $(& $cmakeCmd -L -N ${buildDir} | Select-String "SIMH_DEP_TOPDIR")
            if ($depTopDir) {
                ## RHS of the cached variable's value.
                $depTopDir = $depTopDir.Line.Split('=')[1]
                $env:PATH =  "${depTopdir}\bin;${env:PATH}"
            }
        }

        "install" {
            Write-Host "** ${scriptName}: Installing simulators."

            $installPrefix = $(& $cmakeCmd -L -N ${buildDir} | Select-String "CMAKE_INSTALL_PREFIX")
            $installPrefix = $installPrefix.Line.Split('=')[1]
            $installPath = $installPrefix

            Write-Host "** ${scriptName}: Install directory ${installPath}"
            if (!(Test-Path -Path ${installPath}))
            {
                Write-Host "** ${scriptName}: Creating ${installPath}"
                New-Item -${installPath} -ItemType Directory -ErrorAction SilentlyContinue
            }

            $phaseCommand = ${cmakeCmd}
            $argList = Quote-Args @( "--install", "${buildDir}", "--config", "${config}")
        }

        "package" {
            Write-Host "** ${scriptName}: Packaging simulators."

            $pkgDir = "${simhTopDir}\PACKAGES"
            if (!(Test-Path -Path ${pkgDir}))
            {
                Write-Host "** ${scriptName}: Creating ${pkgDir} subdirectory"
                New-Item -Path ${pkgDir} -ItemType Directory | Out-Null
            }
            else
            {
                Write-Host "** ${scriptName}: ${pkgDir} exists."
            }


            $phaseCommand = ${cpackCmd}
            $argList = Quote-Args @( "-G", "ZIP", "-C", "${config}")
        }
    }

    try {
        Push-Location ${buildDir}
        Write-Host "** ${phaseCommand} ${argList}"
        & $phaseCommand @arglist
        if ($LastExitCode -gt 0) {
            $printPhase = (Get-Culture).TextInfo.ToTitleCase($phase)
            Write-Error $("${printPhase} phase exited with non-zero status: " + $LastExitCode)
            exit 1
        }
    }
    catch {
        Write-Host "Error running '$($psi.FileName) $($psi.Arguments)' command: $($_.Exception.Message)" -ForegroundColor Red
        throw $_
    }
    finally {
        Pop-Location
    }

    $env:PATH = $savedPATH
}

exit $exitval
