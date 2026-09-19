#!/usr/bin/env python3
"""The judge's Visual Studio 2022 project for a lab, written from the lab's .pro.

    tools/make-vs.py c      -> c/vs/CC1Lab.vcxproj (+ .sln)
    tools/make-vs.py cpp    -> cpp/vs/CXX1Lab.vcxproj

cl.exe builds the same sources RIDE builds with cc1i or cxx1i, from a project of
its own: a console application, Release|x64, the files by reference from the lab
directory, C compiled as C (/TC) and C++ as C++ with exceptions (/EHsc, the
lowest standard cl offers is /std:c++14). The oracle on Windows, so everything
else is what Visual Studio's template gives a new console project. Build with

    msbuild c\\vs\\CC1Lab.sln /p:Configuration=Release /p:Platform=x64
"""
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TRILAB = os.path.normpath(os.path.join(HERE, ".."))


def guid(text):
    h = hashlib.sha1(text.encode()).hexdigest().upper()
    return "{%s-%s-%s-%s-%s}" % (h[:8], h[8:12], h[12:16], h[16:20], h[20:32])


def write(lab):
    root = os.path.join(TRILAB, lab)
    pro = [f for f in os.listdir(root) if f.endswith(".pro")]
    if len(pro) != 1:
        sys.exit("%s: one .pro expected, found %d" % (lab, len(pro)))
    p = json.load(open(os.path.join(root, pro[0])))
    name = p["name"]
    srcs, hdrs = [], []
    # A group is a list of files, or an object holding "files" and its own
    # "toolchain" - RIDE writes the first form when the group names none, and
    # rewrote CC1Lab.pro that way on 2026-09-19, which this then could not read.
    for g in p["groups"].values():
        for f in (g["files"] if isinstance(g, dict) else g):
            (srcs if f.endswith((".c", ".cpp")) else hdrs).append(f)
    cxx = any(f.endswith(".cpp") for f in srcs)
    g = guid(lab + ":project")
    compile_items = "\n".join('    <ClCompile Include="..\\%s" />' % f for f in srcs)
    header_items = "\n".join('    <ClInclude Include="..\\%s" />' % f for f in hdrs)
    lang = ('      <CompileAs>CompileAsCpp</CompileAs>\n      <ExceptionHandling>Sync</ExceptionHandling>\n'
            '      <LanguageStandard>stdcpp14</LanguageStandard>\n' if cxx else
            '      <CompileAs>CompileAsC</CompileAs>\n')
    proj = f'''<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>
  <PropertyGroup Label="Globals">
    <ProjectGuid>{g}</ProjectGuid>
    <RootNamespace>{name}</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.Default.props" />
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>Application</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>Unicode</CharacterSet>
  </PropertyGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.props" />
  <PropertyGroup>
    <OutDir>$(SolutionDir)x64\\$(Configuration)\\</OutDir>
    <IntDir>$(SolutionDir)x64\\$(Configuration)\\obj\\</IntDir>
  </PropertyGroup>
  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <Optimization>MaxSpeed</Optimization>
      <PreprocessorDefinitions>NDEBUG;_CONSOLE;_CRT_SECURE_NO_WARNINGS;%(PreprocessorDefinitions)</PreprocessorDefinitions>
{lang}    </ClCompile>
    <Link>
      <SubSystem>Console</SubSystem>
    </Link>
  </ItemDefinitionGroup>
  <ItemGroup>
{compile_items}
  </ItemGroup>
  <ItemGroup>
{header_items}
  </ItemGroup>
  <Import Project="$(VCTargetsPath)\\Microsoft.Cpp.targets" />
</Project>
'''
    sln = f'''Microsoft Visual Studio Solution File, Format Version 12.00
# Visual Studio Version 17
Project("{{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}}") = "{name}", "{name}.vcxproj", "{g}"
EndProject
Global
	GlobalSection(SolutionConfigurationPlatforms) = preSolution
		Release|x64 = Release|x64
	EndGlobalSection
	GlobalSection(ProjectConfigurationPlatforms) = postSolution
		{g}.Release|x64.ActiveCfg = Release|x64
		{g}.Release|x64.Build.0 = Release|x64
	EndGlobalSection
EndGlobal
'''
    out = os.path.join(root, "vs")
    os.makedirs(out, exist_ok=True)
    open(os.path.join(out, name + ".vcxproj"), "w", newline="\r\n").write(proj)
    open(os.path.join(out, name + ".sln"), "w", newline="\r\n").write(sln)
    print("wrote", os.path.relpath(out, TRILAB), "-", len(srcs), "sources")


for lab in sys.argv[1:] or ("c", "cpp"):
    write(lab)
