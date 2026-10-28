"""Syntax-check the vision addon and sketches with an installed macOS OF SDK."""
import subprocess
import sys
from pathlib import Path

sdk = Path(sys.argv[1]).resolve()
root = Path(__file__).resolve().parents[1]
includes = [sdk / "libs/openFrameworks", root / "ofxLocalAI/src", root / "poseLive/src"]
includes.extend(p for p in (sdk / "libs/openFrameworks").iterdir() if p.is_dir())
for p in (sdk / "libs").glob("*/include"):
    includes.append(p)
    includes.extend(child for child in p.iterdir() if child.is_dir())
box2d = sdk / "addons/ofxBox2d"
includes += [box2d / "src", box2d / "libs", box2d / "libs/Box2D"]
command = ["clang++", "-std=c++17", "-fsyntax-only", "-DGL_SILENCE_DEPRECATION", "-Wno-deprecated-declarations"]
for p in includes:
    command.extend(["-I", str(p)])
sources = list((root / "ofxLocalAI/src").glob("*.cpp"))
for example in ("poseLive", "objectLive", "bodyPoem", "voiceScene", "blobPhysics"):
    sources += list((root / example / "src").glob("*.cpp"))
subprocess.run(command + [str(p) for p in sources], check=True)
print("ofxLocalAI addon and selected sketches: syntax OK")
