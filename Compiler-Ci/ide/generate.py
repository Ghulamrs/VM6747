#!/usr/bin/env python3
"""Write the Xcode and Visual Studio projects for cc1 from the source tree.

The Makefile finds its sources with a wildcard, so a project file listing them
by hand rots the first time somebody adds one. This writes both from what is on
disk now:

    ./generate.py            regenerate both projects
    ./generate.py --check    say whether they are up to date, and change nothing

Every flag here is the one the Makefile uses, or the one the hand-kept
msvc/cc1.vcxproj used on Windows before this replaced it; where the two
toolchains differ, the difference is commented at the line that makes it. The
twin of Compiler-Cppi's ide/generate.py, laid out the same way.
"""
import hashlib
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.normpath(os.path.join(HERE, ".."))            # the checkout
NAME = "cc1"                 # the project files keep their name...
PRODUCT = "c90"              # ...and build the program src/Name.h names
VS_GUID = "{6C1B4A2E-5D3F-4A18-9E77-2B0C4F8A1D31}"   # msvc/cc1.vcxproj's, which RIDE.sln names
EXE = PRODUCT + ".exe"       # the file every build of it writes: make, msvc/build.cmd, RIDE
UP = ".."                    # from ide/ to the tree, in both project dialects
SHARED_BUILD = "$(TMPDIR)/ride-xcode"   # the build folder RIDE's projects share: see xcode()


def sources():
    """Every .cpp the build compiles, in the Makefile's own order."""
    out = []
    for d in ("", "backend"):       # SRCS: src/*.cpp and src/backend/*.cpp
        base = os.path.join(ROOT, "src", d)
        for f in sorted(os.listdir(base)):
            if f.endswith(".cpp") and " " not in f:      # macOS " 2.cpp" copies
                out.append(os.path.join("src", d, f).replace("\\", "/").replace("//", "/"))
    return out


def headers():
    out = []
    for d in ("", "backend"):
        base = os.path.join(ROOT, "src", d)
        for f in sorted(os.listdir(base)):
            if f.endswith(".h") and " " not in f:
                out.append(os.path.join("src", d, f).replace("\\", "/").replace("//", "/"))
    return out


def uid(text):
    """A stable 24-hex-digit id. Xcode only asks that they be unique and stable;
    deriving them from the path keeps a regenerated project diffable."""
    return hashlib.sha1(text.encode()).hexdigest()[:24].upper()


def ride_ident(*parts):
    """The target's and the product's ids by RIDE's rule, sha1("<exe>:<part>"): RIDE's
    workspace and shalimar's project point at this project by them, and write nothing here."""
    return hashlib.sha1((EXE + ":" + ":".join(parts)).encode()).hexdigest()[:24].upper()


# ---------------------------------------------------------------- Xcode

def xcode(srcs, hdrs):
    """project.pbxproj for a command-line tool target.

    The flags are the Makefile's: -std=c++14 -O2 -g -Wall -Wextra -Werror
    -pedantic, and CC1_INCLUDE_DIR pointing at the checkout's lib/, which the
    driver compiles into the binary as an absolute path.
    """
    files, builds, groups = [], [], {}
    for p in srcs + hdrs:
        fid, bid = uid("f:" + p), uid("b:" + p)
        kind = "sourcecode.cpp.cpp" if p.endswith(".cpp") else "sourcecode.c.h"
        files.append('\t\t%s /* %s */ = {isa = PBXFileReference; lastKnownFileType = %s; '
                     'name = %s; path = "%s"; sourceTree = "<group>"; };'
                     % (fid, os.path.basename(p), kind, os.path.basename(p),
                        UP + "/" + p))
        if p.endswith(".cpp"):
            builds.append('\t\t%s /* %s in Sources */ = {isa = PBXBuildFile; fileRef = %s /* %s */; };'
                          % (bid, os.path.basename(p), fid, os.path.basename(p)))
        groups.setdefault(os.path.dirname(p), []).append((fid, os.path.basename(p)))

    group_secs, group_children = [], []
    for d in sorted(groups):
        gid = uid("g:" + d)
        kids = "\n".join('\t\t\t\t%s /* %s */,' % (f, n) for f, n in sorted(groups[d], key=lambda x: x[1]))
        group_secs.append('\t\t%s /* %s */ = {\n\t\t\tisa = PBXGroup;\n\t\t\tchildren = (\n%s\n\t\t\t);\n'
                          '\t\t\tname = %s;\n\t\t\tsourceTree = "<group>";\n\t\t};'
                          % (gid, d or "src", kids, '"%s"' % (d or "src")))
        group_children.append('\t\t\t\t%s /* %s */,' % (gid, d or "src"))

    src_phase = "\n".join('\t\t\t\t%s /* %s in Sources */,' % (uid("b:" + p), os.path.basename(p))
                          for p in srcs)
    inc = "$(SRCROOT)/%s/lib" % UP          # INCDIR = $(CURDIR)/lib, relative: an absolute path names one machine
    common = ('\t\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\n'
              # **The Makefile's warning line and no other.** Xcode's template
              # adds -Wshorten-64-to-32, which -Wall -Wextra do not, and it fires
              # on the bitfield arithmetic that is deliberately done in long long
              # and narrowed after a check. A project that builds this tree with
              # a different warning set is a fourth opinion nothing else gates on.
              '\t\t\t\tGCC_WARN_64_TO_32_BIT_CONVERSION = NO;\n'
              # One architecture, as `make` builds: the target is a run-time
              # choice here, so a universal binary buys nothing but time.
              '\t\t\t\tONLY_ACTIVE_ARCH = YES;\n'
              '\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = "c++14";\n'
              '\t\t\t\tCLANG_CXX_LIBRARY = "libc++";\n'
              '\t\t\t\tCLANG_ENABLE_OBJC_ARC = YES;\n'
              '\t\t\t\tCODE_SIGN_STYLE = Automatic;\n'
              # the Makefile's own warning set, and -Werror with it
              '\t\t\t\tGCC_TREAT_WARNINGS_AS_ERRORS = YES;\n'
              '\t\t\t\tWARNING_CFLAGS = (\n\t\t\t\t\t"-Wall",\n\t\t\t\t\t"-Wextra",\n'
              '\t\t\t\t\t"-pedantic",\n\t\t\t\t);\n'
              # the include directory the driver bakes in, as the Makefile does
              '\t\t\t\tGCC_PREPROCESSOR_DEFINITIONS = (\n'
              '\t\t\t\t\t"CC1_INCLUDE_DIR=\\\\\\"%s\\\\\\"",\n\t\t\t\t);\n'
              '\t\t\t\tPRODUCT_NAME = "%s";\n'
              '\t\t\t\tHEADER_SEARCH_PATHS = "$(SRCROOT)/%s/src";\n'
              # RIDE's editor takes c90.exe from its own BUILT_PRODUCTS_DIR,
              # so every project in RIDE's workspace builds into one place, and
              # for the Mac the release is built for.
              '\t\t\t\tSYMROOT = "%s";\n'
              '\t\t\t\tOBJROOT = "%s";\n'
              '\t\t\t\tMACOSX_DEPLOYMENT_TARGET = 12.0;\n'
              % (inc, EXE, UP, SHARED_BUILD, SHARED_BUILD))
    return files, builds, group_secs, group_children, src_phase, common


def write_xcode(srcs, hdrs, check):
    files, builds, group_secs, group_children, src_phase, common = xcode(srcs, hdrs)
    proj = os.path.join(HERE, NAME + ".xcodeproj")
    pb = os.path.join(proj, "project.pbxproj")

    ids = {k: uid(k) for k in ("project", "productgroup",
                               "mainGroup", "sources", "cfgProject", "cfgTarget",
                               "dbgP", "relP", "dbgT", "relT")}
    ids["target"], ids["product"] = ride_ident("target"), ride_ident("product")
    text = f"""// !$*UTF8*$!
{{
	archiveVersion = 1;
	classes = {{}};
	objectVersion = 54;
	objects = {{

/* Begin PBXBuildFile section */
{chr(10).join(builds)}
/* End PBXBuildFile section */

/* Begin PBXFileReference section */
{chr(10).join(files)}
		{ids['product']} /* {EXE} */ = {{isa = PBXFileReference; explicitFileType = "compiled.mach-o.executable"; includeInIndex = 0; path = {EXE}; sourceTree = BUILT_PRODUCTS_DIR; }};
/* End PBXFileReference section */

/* Begin PBXGroup section */
		{ids['mainGroup']} = {{
			isa = PBXGroup;
			children = (
{chr(10).join(group_children)}
				{ids['productgroup']} /* Products */,
			);
			sourceTree = "<group>";
		}};
		{ids['productgroup']} /* Products */ = {{
			isa = PBXGroup;
			children = (
				{ids['product']} /* {EXE} */,
			);
			name = Products;
			sourceTree = "<group>";
		}};
{chr(10).join(group_secs)}
/* End PBXGroup section */

/* Begin PBXNativeTarget section */
		{ids['target']} /* {NAME} */ = {{
			isa = PBXNativeTarget;
			buildConfigurationList = {ids['cfgTarget']};
			buildPhases = (
				{ids['sources']} /* Sources */,
			);
			dependencies = ();
			name = {NAME};
			productName = {EXE};
			productReference = {ids['product']} /* {EXE} */;
			productType = "com.apple.product-type.tool";
		}};
/* End PBXNativeTarget section */

/* Begin PBXProject section */
		{ids['project']} /* Project object */ = {{
			isa = PBXProject;
			attributes = {{
				BuildIndependentTargetsInParallel = 1;
				LastUpgradeCheck = 2600;
			}};
			buildConfigurationList = {ids['cfgProject']};
			compatibilityVersion = "Xcode 14.0";
			developmentRegion = en;
			hasScannedForEncodings = 0;
			knownRegions = (en, Base);
			mainGroup = {ids['mainGroup']};
			productRefGroup = {ids['productgroup']} /* Products */;
			projectDirPath = "";
			projectRoot = "";
			targets = (
				{ids['target']} /* {NAME} */,
			);
		}};
/* End PBXProject section */

/* Begin PBXSourcesBuildPhase section */
		{ids['sources']} /* Sources */ = {{
			isa = PBXSourcesBuildPhase;
			buildActionMask = 2147483647;
			files = (
{src_phase}
			);
			runOnlyForDeploymentPostprocessing = 0;
		}};
/* End PBXSourcesBuildPhase section */

/* Begin XCBuildConfiguration section */
		{ids['dbgP']} /* Debug */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
{common}				GCC_OPTIMIZATION_LEVEL = 0;
			}};
			name = Debug;
		}};
		{ids['relP']} /* Release */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
{common}				GCC_OPTIMIZATION_LEVEL = 2;
			}};
			name = Release;
		}};
		{ids['dbgT']} /* Debug */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
				PRODUCT_NAME = "{EXE}";
			}};
			name = Debug;
		}};
		{ids['relT']} /* Release */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
				PRODUCT_NAME = "{EXE}";
			}};
			name = Release;
		}};
/* End XCBuildConfiguration section */

/* Begin XCConfigurationList section */
		{ids['cfgProject']} = {{
			isa = XCConfigurationList;
			buildConfigurations = (
				{ids['dbgP']} /* Debug */,
				{ids['relP']} /* Release */,
			);
			defaultConfigurationIsVisible = 0;
			defaultConfigurationName = Release;
		}};
		{ids['cfgTarget']} = {{
			isa = XCConfigurationList;
			buildConfigurations = (
				{ids['dbgT']} /* Debug */,
				{ids['relT']} /* Release */,
			);
			defaultConfigurationIsVisible = 0;
			defaultConfigurationName = Release;
		}};
/* End XCConfigurationList section */
	}};
	rootObject = {ids['project']} /* Project object */;
}}
"""
    ws = os.path.join(HERE, NAME + ".xcworkspace")
    wsdata = ('<?xml version="1.0" encoding="UTF-8"?>\n<Workspace version = "1.0">\n'
              '   <FileRef location = "group:%s.xcodeproj"></FileRef>\n</Workspace>\n' % NAME)

    # **A shared scheme, because an implicit one is not a file.** Xcode makes one
    # when a person opens the project and keeps it under xcuserdata, where it is
    # nobody else's; xcodebuild -scheme and any CI want one that is checked in.
    scheme = f"""<?xml version="1.0" encoding="UTF-8"?>
<Scheme LastUpgradeVersion = "2600" version = "1.7">
   <BuildAction parallelizeBuildables = "YES" buildImplicitDependencies = "YES">
      <BuildActionEntries>
         <BuildActionEntry buildForTesting = "YES" buildForRunning = "YES" buildForProfiling = "YES" buildForArchiving = "YES" buildForAnalyzing = "YES">
            <BuildableReference
               BuildableIdentifier = "primary"
               BlueprintIdentifier = "{ids['target']}"
               BuildableName = "{EXE}"
               BlueprintName = "{NAME}"
               ReferencedContainer = "container:{NAME}.xcodeproj">
            </BuildableReference>
         </BuildActionEntry>
      </BuildActionEntries>
   </BuildAction>
   <LaunchAction buildConfiguration = "Release" selectedDebuggerIdentifier = "Xcode.DebuggerFoundation.Debugger.LLDB" selectedLauncherIdentifier = "Xcode.DebuggerFoundation.Launcher.LLDB" launchStyle = "0" useCustomWorkingDirectory = "NO" ignoresPersistentStateOnLaunch = "NO" debugDocumentVersioning = "YES" debugServiceExtension = "internal" allowLocationSimulation = "YES">
      <BuildableProductRunnable runnableDebuggingMode = "0">
         <BuildableReference
            BuildableIdentifier = "primary"
            BlueprintIdentifier = "{ids['target']}"
            BuildableName = "{EXE}"
            BlueprintName = "{NAME}"
            ReferencedContainer = "container:{NAME}.xcodeproj">
         </BuildableReference>
      </BuildableProductRunnable>
   </LaunchAction>
   <AnalyzeAction buildConfiguration = "Release"></AnalyzeAction>
   <ArchiveAction buildConfiguration = "Release" revealArchiveInOrganizer = "YES"></ArchiveAction>
</Scheme>
"""
    schemedir = os.path.join(proj, "xcshareddata", "xcschemes")
    spath = os.path.join(schemedir, NAME + ".xcscheme")

    if check:
        old = open(pb).read() if os.path.exists(pb) else ""
        oldscheme = open(spath).read() if os.path.exists(spath) else ""
        return old == text and oldscheme == scheme
    os.makedirs(proj, exist_ok=True)
    os.makedirs(ws, exist_ok=True)
    os.makedirs(schemedir, exist_ok=True)
    open(pb, "w").write(text)
    open(spath, "w").write(scheme)
    open(os.path.join(ws, "contents.xcworkspacedata"), "w").write(wsdata)
    return True


# ------------------------------------------------------- Visual Studio 2022

def validXml(text, what):
    """**A project file is XML, and MSBuild will not read a control byte.**
    One backslash in a comment - `msvc\\build.cmd` written with one rather than
    two inside an f-string - put 0x08 in the file, and Visual Studio answered
    `MSB4025: hexadecimal value 0x08, is an invalid character` rather than
    anything about the compiler. Checked here so the generator cannot ship one
    again."""
    import xml.dom.minidom
    xml.dom.minidom.parseString(text.encode("utf-8"))
    for ch in text:
        if ord(ch) < 9 or ord(ch) in (11, 12) or 14 <= ord(ch) <= 31:
            raise ValueError("%s holds a control byte 0x%02X" % (what, ord(ch)))
    return text


def write_vs(srcs, hdrs, check):
    """cc1.vcxproj and cc1.sln, carrying the hand-kept msvc/cc1.vcxproj's flags.

    /std:c++14 as the Makefile's -std=c++14; /W4 /WX, which the old project kept
    in both configurations, with five warnings off - 4996 (getenv and friends
    are standard), 4267/4244 (size_t narrowing the other builds do not warn
    about), 4456 (shadowing, -Wshadow elsewhere) and 4146 (unary minus on an
    unsigned, deliberate) - and the static runtime it linked.
    """
    def win(p):
        return "..\\" + p.replace("/", "\\")

    cl = "\n".join('    <ClCompile Include="%s" />' % win(p) for p in srcs)
    hd = "\n".join('    <ClInclude Include="%s" />' % win(p) for p in hdrs)
    guid = VS_GUID

    # The include directory is compiled into the binary as a C string literal,
    # so it must be spelled with forward slashes: a backslash there starts an
    # escape and the error lands in Driver.cpp, which is not the file at fault.
    # Compiled into the binary as a C string literal, so forward slashes: a
    # backslash there starts an escape and the error lands in Driver.cpp.
    incdir = "$([System.String]::Copy('$(ProjectDir)..\\lib').Replace('\\','/'))"

    proj = f"""<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64"><Configuration>Release</Configuration><Platform>x64</Platform></ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <VCProjectVersion>17.0</VCProjectVersion>
    <ProjectGuid>{guid}</ProjectGuid>
    <RootNamespace>{NAME}</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)'=='Release'" Label="Configuration">
    <UseDebugLibraries>false</UseDebugLibraries>
    <WholeProgramOptimization>false</WholeProgramOptimization>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)'=='Debug'" Label="Configuration">
    <UseDebugLibraries>true</UseDebugLibraries>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <!-- **The output follows the solution that builds it.** Its own, ide\\{NAME}.sln (or
       none), keeps build\\<config>\\; any other - RIDE.sln - puts {EXE} beside the rest of
       that solution's programs, which is where the editor looks. The objects stay here. -->
  <PropertyGroup>
    <OutDir Condition="'$(SolutionName)'=='{NAME}' Or '$(SolutionDir)'=='' Or '$(SolutionDir)'=='*Undefined*'">$(ProjectDir)build\\$(Configuration)\\</OutDir>
    <OutDir Condition="'$(SolutionName)'!='{NAME}' And '$(SolutionDir)'!='' And '$(SolutionDir)'!='*Undefined*'">$(SolutionDir)$(Platform)\\$(Configuration)\\</OutDir>
    <IntDir>$(ProjectDir)build\\$(Configuration)\\obj\\</IntDir>
    <TargetName>{PRODUCT}</TargetName>
  </PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile>
      <LanguageStandard>stdcpp14</LanguageStandard>
      <ConformanceMode>true</ConformanceMode>
      <ExceptionHandling>Sync</ExceptionHandling>
      <WarningLevel>Level4</WarningLevel>
      <TreatWarningAsError>true</TreatWarningAsError>
      <DisableSpecificWarnings>4996;4267;4244;4456;4146</DisableSpecificWarnings>
      <AdditionalIncludeDirectories>$(ProjectDir)..\\msvc\\compat;$(ProjectDir)..\\src;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <PreprocessorDefinitions>_CRT_SECURE_NO_WARNINGS;CC1_INCLUDE_DIR="{incdir}";%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <MultiProcessorCompilation>true</MultiProcessorCompilation>
    </ClCompile>
    <Link><SubSystem>Console</SubSystem></Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)'=='Release'">
    <ClCompile>
      <Optimization>MaxSpeed</Optimization>
      <RuntimeLibrary>MultiThreaded</RuntimeLibrary>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
    <Link><GenerateDebugInformation>true</GenerateDebugInformation></Link>
  </ItemDefinitionGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)'=='Debug'">
    <ClCompile>
      <Optimization>Disabled</Optimization>
      <RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
    </ClCompile>
    <Link><GenerateDebugInformation>true</GenerateDebugInformation></Link>
  </ItemDefinitionGroup>
  <ItemGroup>
{cl}
  </ItemGroup>
  <ItemGroup>
{hd}
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>
"""
    sln = f"""Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
VisualStudioVersion = 17.0.31903.59
MinimumVisualStudioVersion = 10.0.40219.1
Project("{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}") = "{NAME}", "{NAME}.vcxproj", "{guid}"
EndProject
Global
\tGlobalSection(SolutionConfigurationPlatforms) = preSolution
\t\tDebug|x64 = Debug|x64
\t\tRelease|x64 = Release|x64
\tEndGlobalSection
\tGlobalSection(ProjectConfigurationPlatforms) = postSolution
\t\t{guid}.Debug|x64.ActiveCfg = Debug|x64
\t\t{guid}.Debug|x64.Build.0 = Debug|x64
\t\t{guid}.Release|x64.ActiveCfg = Release|x64
\t\t{guid}.Release|x64.Build.0 = Release|x64
\tEndGlobalSection
EndGlobal
"""
    # **The filters file is what makes it look like a project in the IDE.**
    # Without one Visual Studio shows every file in one flat list; with it the
    # tree is the tree - src and src\backend - as the Xcode project presents it.
    def folder(p):
        parts = p.split("/")
        return "\\".join(parts[:-1]).replace("src", "Source Files", 1)

    def hfolder(p):
        parts = p.split("/")
        return "\\".join(parts[:-1]).replace("src", "Header Files", 1)

    dirs = sorted({folder(p) for p in srcs} | {hfolder(p) for p in hdrs})
    unique = []
    for d in dirs:                      # every parent folder, once each
        parts = d.split("\\")
        for i in range(1, len(parts) + 1):
            joined = "\\".join(parts[:i])
            if joined and joined not in unique:
                unique.append(joined)
    filt_dirs = "\n".join(
        '    <Filter Include="%s"><UniqueIdentifier>{%s}</UniqueIdentifier></Filter>'
        % (d, uid("filt:" + d)[:8] + "-" + uid("f1:" + d)[:4] + "-" +
           uid("f2:" + d)[:4] + "-" + uid("f3:" + d)[:4] + "-" +
           uid("f4:" + d)[:12]) for d in unique)
    filt_src = "\n".join(
        '    <ClCompile Include="%s"><Filter>%s</Filter></ClCompile>'
        % (win(p), folder(p)) for p in srcs)
    filt_hdr = "\n".join(
        '    <ClInclude Include="%s"><Filter>%s</Filter></ClInclude>'
        % (win(p), hfolder(p)) for p in hdrs)
    filters = f"""<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
{filt_dirs}
  </ItemGroup>
  <ItemGroup>
{filt_src}
  </ItemGroup>
  <ItemGroup>
{filt_hdr}
  </ItemGroup>
</Project>
"""

    pp = os.path.join(HERE, NAME + ".vcxproj")
    sp = os.path.join(HERE, NAME + ".sln")
    fp = pp + ".filters"
    if check:
        for path, want in ((pp, proj), (sp, sln), (fp, filters)):
            if not os.path.exists(path) or open(path).read() != want:
                return False
        return True
    validXml(proj, NAME + ".vcxproj")
    validXml(filters, NAME + ".vcxproj.filters")
    for path, want in ((pp, proj), (sp, sln), (fp, filters)):
        open(path, "w", newline="\r\n").write(want)
    return True


if __name__ == "__main__":
    check = "--check" in sys.argv
    s, h = sources(), headers()
    x, v = write_xcode(s, h, check), write_vs(s, h, check)
    if check:
        print("up to date" if (x and v) else "STALE - run ./generate.py")
        sys.exit(0 if (x and v) else 1)
    print("wrote %s.xcodeproj, %s.xcworkspace, %s.vcxproj and %s.sln for %d sources"
          % (NAME, NAME, NAME, NAME, len(s)))
