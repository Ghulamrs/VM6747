#!/usr/bin/env python3
"""The judge's Xcode project for a lab, written from the lab's .pro.

    tools/make-xcode.py c      -> c/xcode/CC1Lab.xcodeproj
    tools/make-xcode.py cpp    -> cpp/xcode/CXX1Lab.xcodeproj

Apple clang builds the same sources RIDE builds with cc1i or cxx1i, from a
project of its own: a command-line tool, one target, the files by reference
from the lab directory, C as gnu99 and C++ as C++11 - the levels the two
compilers implement - with -Wall and nothing else Xcode's template adds. It is
the oracle on macOS, so its settings are Xcode's defaults where the level does
not force a choice. Build it with

    xcodebuild -project c/xcode/CC1Lab.xcodeproj -target CC1Lab -configuration Release build
"""
import hashlib
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
TRILAB = os.path.normpath(os.path.join(HERE, ".."))


def uid(text):
    return hashlib.sha1(text.encode()).hexdigest()[:24].upper()


def write(lab):
    root = os.path.join(TRILAB, lab)
    pro = [f for f in os.listdir(root) if f.endswith(".pro")]
    if len(pro) != 1:
        sys.exit("%s: one .pro expected, found %d" % (lab, len(pro)))
    p = json.load(open(os.path.join(root, pro[0])))
    name = p["name"]
    srcs, hdrs = [], []
    for g in p["groups"].values():
        for f in (g["files"] if isinstance(g, dict) else g):   # a list, or an object with its own toolchain
            (srcs if f.endswith((".c", ".cpp")) else hdrs).append(f)
    cxx = any(f.endswith(".cpp") for f in srcs)

    files, builds, kids = [], [], []
    for f in srcs + hdrs:
        fid, bid = uid("f:" + f), uid("b:" + f)
        kind = ("sourcecode.cpp.cpp" if f.endswith(".cpp") else
                "sourcecode.c.c" if f.endswith(".c") else "sourcecode.c.h")
        files.append('\t\t%s /* %s */ = {isa = PBXFileReference; lastKnownFileType = %s; '
                     'name = %s; path = "../%s"; sourceTree = "<group>"; };' % (fid, f, kind, f, f))
        kids.append('\t\t\t\t%s /* %s */,' % (fid, f))
        if f in srcs:
            builds.append('\t\t%s /* %s in Sources */ = {isa = PBXBuildFile; fileRef = %s /* %s */; };'
                          % (bid, f, fid, f))
    phase = "\n".join('\t\t\t\t%s /* %s in Sources */,' % (uid("b:" + f), f) for f in srcs)
    ids = {k: uid(lab + ":" + k) for k in ("project", "target", "product", "products", "main",
                                            "sources", "cfgP", "cfgT", "dbgP", "relP", "dbgT", "relT")}
    level = ('\t\t\t\tCLANG_CXX_LANGUAGE_STANDARD = "c++11";\n\t\t\t\tCLANG_CXX_LIBRARY = "libc++";\n'
             if cxx else '\t\t\t\tGCC_C_LANGUAGE_STANDARD = gnu99;\n')
    common = ('\t\t\t\tALWAYS_SEARCH_USER_PATHS = NO;\n'
              '\t\t\t\tONLY_ACTIVE_ARCH = YES;\n'
              '\t\t\t\tCODE_SIGN_STYLE = Automatic;\n'
              '\t\t\t\tWARNING_CFLAGS = ("-Wall");\n'
              + level +
              '\t\t\t\tPRODUCT_NAME = "%s";\n' % name)
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
		{ids['product']} /* {name} */ = {{isa = PBXFileReference; explicitFileType = "compiled.mach-o.executable"; includeInIndex = 0; path = {name}; sourceTree = BUILT_PRODUCTS_DIR; }};
/* End PBXFileReference section */

/* Begin PBXGroup section */
		{ids['main']} = {{
			isa = PBXGroup;
			children = (
{chr(10).join(kids)}
				{ids['products']} /* Products */,
			);
			sourceTree = "<group>";
		}};
		{ids['products']} /* Products */ = {{
			isa = PBXGroup;
			children = (
				{ids['product']} /* {name} */,
			);
			name = Products;
			sourceTree = "<group>";
		}};
/* End PBXGroup section */

/* Begin PBXNativeTarget section */
		{ids['target']} /* {name} */ = {{
			isa = PBXNativeTarget;
			buildConfigurationList = {ids['cfgT']};
			buildPhases = (
				{ids['sources']} /* Sources */,
			);
			dependencies = ();
			name = {name};
			productName = {name};
			productReference = {ids['product']} /* {name} */;
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
			buildConfigurationList = {ids['cfgP']};
			compatibilityVersion = "Xcode 14.0";
			developmentRegion = en;
			hasScannedForEncodings = 0;
			knownRegions = (en, Base);
			mainGroup = {ids['main']};
			productRefGroup = {ids['products']} /* Products */;
			projectDirPath = "";
			projectRoot = "";
			targets = (
				{ids['target']} /* {name} */,
			);
		}};
/* End PBXProject section */

/* Begin PBXSourcesBuildPhase section */
		{ids['sources']} /* Sources */ = {{
			isa = PBXSourcesBuildPhase;
			buildActionMask = 2147483647;
			files = (
{phase}
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
				PRODUCT_NAME = "{name}";
			}};
			name = Debug;
		}};
		{ids['relT']} /* Release */ = {{
			isa = XCBuildConfiguration;
			buildSettings = {{
				PRODUCT_NAME = "{name}";
			}};
			name = Release;
		}};
/* End XCBuildConfiguration section */

/* Begin XCConfigurationList section */
		{ids['cfgP']} = {{
			isa = XCConfigurationList;
			buildConfigurations = (
				{ids['dbgP']} /* Debug */,
				{ids['relP']} /* Release */,
			);
			defaultConfigurationIsVisible = 0;
			defaultConfigurationName = Release;
		}};
		{ids['cfgT']} = {{
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
    proj = os.path.join(root, "xcode", name + ".xcodeproj")
    os.makedirs(proj, exist_ok=True)
    open(os.path.join(proj, "project.pbxproj"), "w").write(text)
    print("wrote", os.path.relpath(proj, TRILAB), "-", len(srcs), "sources")


for lab in sys.argv[1:] or ("c", "cpp"):
    write(lab)
