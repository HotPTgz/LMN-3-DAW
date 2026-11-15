#!/usr/bin/env python3
"""Writes the Android Studio project for LMN-3.

The desktop and Pi builds use CMake, but the JUCE version in this repo can't
build Android apps with CMake. So this script writes a Projucer project
(Android/LMN-3.jucer) from the CMake file lists, plus the two binary-data
files the CMake build normally generates; the Projucer then turns it into a
Gradle project in Android/Builds/Android.

    python3 Android/make_project.py
    Projucer --resave Android/LMN-3.jucer
    python3 Android/make_project.py --fix-up
"""
import pathlib, re, hashlib, os, sys
from xml.sax.saxutils import quoteattr

ROOT = pathlib.Path(__file__).resolve().parent.parent
HERE = ROOT / "Android"

if "--fix-up" in sys.argv:
    # The Projucer adds 32-bit ARM flags to every Android build; the NDK's
    # CMake refuses them for 64-bit phones
    gradle = HERE / "Builds/Android/app/build.gradle"
    text = gradle.read_text()
    for flag in ['"-DANDROID_ARM_MODE=arm"', '"-DANDROID_ARM_NEON=TRUE"']:
        text = text.replace(", " + flag, "").replace(flag + ", ", "")
    gradle.write_text(text)
    print("fixed up", gradle)
    sys.exit(0)
GEN = HERE / "Generated"
cmake = (ROOT / "CMakeLists.txt").read_text()


def block(start):
    m = re.search(re.escape(start) + r"(.*?)\n\)", cmake, re.S)
    return [l.strip() for l in m.group(1).splitlines()
            if l.strip() and not l.strip().startswith("#")]


def rel(p):  # paths in the .jucer are relative to the .jucer's folder
    return os.path.relpath(ROOT / p, HERE).replace(os.sep, "/")


def uid(s):
    return hashlib.md5(s.encode()).hexdigest()[:6]


# ---------------------------------------------------------------- binary data
def binary_data(namespace, files):
    """Same API as juce_add_binary_data: NAMESPACE::name and name + Size."""
    GEN.mkdir(exist_ok=True)
    h = [f"#pragma once\nnamespace {namespace} {{"]
    c = [f'#include "{namespace}.h"\nnamespace {namespace} {{']
    for f in files:
        data = (ROOT / f).read_bytes()
        name = re.sub(r"[^A-Za-z0-9_]", "", pathlib.Path(f).name.replace(".", "_"))
        h.append(f"extern const char *{name};\nconst int {name}Size = {len(data)};")
        body = ",".join(str(b) for b in data)
        c.append(f"static const unsigned char {name}_data[] = {{{body},0}};\n"
                 f"const char *{name} = (const char *){name}_data;")
    h.append("}")
    c.append("}")
    (GEN / f"{namespace}.h").write_text("\n".join(h) + "\n")
    (GEN / f"{namespace}.cpp").write_text("\n".join(c) + "\n")


def data_files(header):
    m = re.search(r"juce_add_binary_data\(" + header + r".*?SOURCES(.*?)\n\)", cmake, re.S)
    return [l.strip() for l in m.group(1).splitlines() if l.strip()]


binary_data("ImageData", data_files("ImageData"))
binary_data("FontData", data_files("FontData"))

# ---------------------------------------------------------------- sources
sources = [s for s in block("target_sources(LMN-3  PRIVATE") if s.endswith(".cpp")]
yaml = sorted(str(p.relative_to(ROOT)) for p in (ROOT / "yaml-cpp/src").rglob("*.cpp"))
generated = ["Android/Generated/ImageData.cpp", "Android/Generated/FontData.cpp"]

includes = [rel(p) for p in block("target_include_directories(LMN-3  PUBLIC")]
includes += [rel("yaml-cpp/include"), rel("Android/Generated")]

juce_modules = ["juce_audio_basics", "juce_audio_devices", "juce_audio_formats",
                "juce_audio_processors", "juce_audio_utils", "juce_core",
                "juce_data_structures", "juce_dsp", "juce_events", "juce_graphics",
                "juce_gui_basics", "juce_gui_extra", "juce_osc"]
other_modules = {"tracktion_engine": "tracktion_engine/modules",
                 "tracktion_graph": "tracktion_engine/modules"}
for m in ["app_navigation", "app_services", "app_models", "internal_plugins",
          "app_view_models", "app_configuration"]:
    other_modules[m] = "Source/Modules"
module_paths = {m: rel("tracktion_engine/modules/juce/modules") for m in juce_modules}
module_paths.update({m: rel(p) for m, p in other_modules.items()})

version = re.search(r"project\(LMN-3 VERSION ([0-9.]+)\)", cmake).group(1)
defines = "\n".join([
    "JUCE_MODAL_LOOPS_PERMITTED=1",
    "TRACKTION_ENABLE_TIMESTRETCH_SOUNDTOUCH=1",
    "YAML_CPP_STATIC_DEFINE=1",
    "LMN3_TOUCH_CONTROLS=1",
    'JUCE_APPLICATION_NAME_STRING="LMN-3"',
    f'JUCE_APPLICATION_VERSION_STRING="{version}"',
])

def file_xml(path, compile_=True):
    name = pathlib.Path(path).name
    return (f'      <FILE id="{uid(path)}" name="{name}" compile="{int(compile_)}" '
            f'resource="0" file={quoteattr(rel(path))}/>')

files = "\n".join(file_xml(p) for p in sources + generated + yaml)
modules = "\n".join(f'    <MODULE id="{m}" showAllCode="0" useLocalCopy="0" useGlobalPath="0"/>'
                    for m in module_paths)
paths = "\n".join(f'        <MODULEPATH id="{m}" path={quoteattr(p)}/>'
                  for m, p in module_paths.items())
version = re.search(r"project\(LMN-3 VERSION ([0-9.]+)\)", cmake).group(1)

keystore = "${keystore}"
jucer = f'''<?xml version="1.0" encoding="UTF-8"?>
<JUCERPROJECT id="LmN3aNd" name="LMN-3" projectType="guiapp" version="{version}"
              bundleIdentifier="com.fundamentalfrequency.lmn3" companyName="LMN-3"
              displaySplashScreen="0" reportAppUsage="0" useAppConfig="0"
              addUsingNamespaceToJuceHeader="0" cppLanguageStandard="17"
              jucerFormatVersion="1" defines={quoteattr(defines)}>
  <MAINGROUP id="mAiNgRp" name="LMN-3">
    <GROUP id="sRcGrP" name="Source">
{files}
    </GROUP>
  </MAINGROUP>
  <MODULES>
{modules}
  </MODULES>
  <JUCEOPTIONS JUCE_PLUGINHOST_VST3="0" JUCE_PLUGINHOST_AU="0" JUCE_WEB_BROWSER="0"
               JUCE_USE_CURL="0" JUCE_USE_ANDROID_OBOE="1" JUCE_STRICT_REFCOUNTEDPOINTER="1"/>
  <EXPORTFORMATS>
    <ANDROIDSTUDIO targetFolder="Builds/Android" androidMinimumSDK="26" androidTargetSDK="33"
                   androidScreenOrientation="landscape" androidInternetNeeded="0"
                   microphonePermissionNeeded="0" androidBluetoothNeeded="0"
                   androidExternalReadNeeded="0" androidExternalWriteNeeded="0"
                   gradleVersion="7.6.4" gradleWrapperVersion="7.4.2" androidPushNotifications="0"
                   androidVersionCode="{version.replace('.', '')}"
                   androidKeyStore="../../../lmn3-android.keystore" androidKeyStorePass="lmn3lmn3"
                   androidKeyAlias="lmn3" androidKeyAliasPass="lmn3lmn3"
                   extraCompilerFlags="-Wno-everything"
                   headerPath={quoteattr(chr(10).join(includes))}>
      <CONFIGURATIONS>
        <CONFIGURATION name="Debug" isDebug="1" androidArchitectures="arm64-v8a" optimisation="1"/>
        <CONFIGURATION name="Release" isDebug="0" androidArchitectures="arm64-v8a" optimisation="3"/>
      </CONFIGURATIONS>
      <MODULEPATHS>
{paths}
      </MODULEPATHS>
    </ANDROIDSTUDIO>
  </EXPORTFORMATS>
</JUCERPROJECT>
'''
(HERE / "LMN-3.jucer").write_text(jucer)
print("wrote", HERE / "LMN-3.jucer", f"({len(sources)} app files, {len(yaml)} yaml-cpp files)")
