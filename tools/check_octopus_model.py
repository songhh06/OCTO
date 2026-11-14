#!/usr/bin/env python3
"""Validate the exact user-supplied Godot GLB rig before import.

No third-party dependencies; validates the GLB container and glTF node graph.
This is NOT a full watertight/tetrahedral-body or visual deformation test.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path

def inspect_glb(path):
    with Path(path).open("rb") as stream:
        header = stream.read(12)
        if len(header) != 12:
            raise ValueError("truncated GLB")
        magic, version, size = struct.unpack("<4sII", header)
        if magic != b"glTF" or version != 2 or size != path.stat().st_size:
            raise ValueError("invalid glTF 2.0 binary header")
        length, kind = struct.unpack("<I4s", stream.read(8))
        if kind != b"JSON":
            raise ValueError("GLB JSON chunk missing")
        data = json.loads(stream.read(length))
        names = [node.get("name", "") for node in data.get("nodes", [])]
        for a in range(8):
            for j in range(24):
                name = f"A{a}_{j:02}"
                if names.count(name) != 1:
                    raise ValueError(f"required arm bone missing or duplicated: {name}")
        skins = data.get("skins", [])
        if len(skins) != 1 or len(skins[0].get("joints", [])) != 193:
            raise ValueError("expected a skin with 192 arm bones + Root")
        if "Root" not in names or "Octopus_Skin" not in names:
            raise ValueError("expected Root and Octopus_Skin nodes")
        skin = next(n for n in data["nodes"] if n.get("name") == "Octopus_Skin")
        if skin.get("skin") != 0 or "mesh" not in skin:
            raise ValueError("Octopus_Skin must reference the skinned mesh")
        result = {
            "glb": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
            "bytes": size, "arms": 8, "segments_per_arm": 24,
            "skin_joints": len(skins[0]["joints"]),
            "animations": len(data.get("animations", [])),
            "meshes": len(data.get("meshes", [])),
        }
        return result

if __name__ == "__main__":
    cli = argparse.ArgumentParser()
    cli.add_argument("glb", nargs="?", default="game/assets/Octopus_GameReady_v5.glb")
    args = cli.parse_args()
    print(json.dumps(inspect_glb(Path(args.glb)), indent=2))
