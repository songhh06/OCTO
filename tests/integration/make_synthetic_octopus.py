"""Generate a tiny 193-joint GLB only for the Godot rig integration test.

This synthetic triangle is NOT the user's GameReady art or a physics collider.
"""
import json
import math
import struct
import sys
from pathlib import Path


def write_fixture(dest):
    nodes = [{"name": "Octopus_Rig", "children": [1]},
             {"name": "Root", "translation": [0, .15, .1], "children": []}]
    xyz = {1: (0., .15, .1)}
    joints = [1]
    for a in range(8):
        parent = 1
        phi = 2 * math.pi * a / 8 + math.pi / 8
        for j in range(24):
            i = len(nodes)
            t = [0.26 * math.sin(phi), .02, .26 * math.cos(phi)] if j == 0 else [0, .076, 0]
            nodes.append({"name": f"A{a}_{j:02d}", "translation": t})
            nodes[parent].setdefault("children", []).append(i)
            xyz[i] = tuple(x + y for x, y in zip(xyz[parent], t))
            joints.append(i)
            parent = i
    assert len(joints) == 193
    nodes[0]["children"].append(len(nodes))
    nodes.append({"name": "Octopus_Skin", "mesh": 0, "skin": 0})
    binary = bytearray()
    views, accessors = [], []
    def add(raw, typ, comp, count, lo=None, hi=None):
        while len(binary) % 4: binary.append(0)
        off = len(binary)
        binary.extend(raw)
        v = len(views)
        views.append({"buffer": 0, "byteOffset": off, "byteLength": len(raw)})
        accessor = {"bufferView": v, "componentType": comp,
                    "count": count, "type": typ}
        if lo is not None: accessor["min"] = lo
        if hi is not None: accessor["max"] = hi
        accessors.append(accessor)
        return len(accessors) - 1
    pos = add(struct.pack('<9f', 0, 0, 0, .1, 0, 0, 0, .1, 0),
              "VEC3", 5126, 3, [0, 0, 0], [.1, .1, 0])
    norm = add(struct.pack('<3f', 0, 0, 1) * 3, "VEC3", 5126, 3)
    uv = add(struct.pack('<6f', 0, 0, 1, 0, 0, 1), "VEC2", 5126, 3)
    jj = add(struct.pack('<4H', 0, 0, 0, 0) * 3, "VEC4", 5123, 3)
    ww = add(struct.pack('<4f', 1, 0, 0, 0) * 3, "VEC4", 5126, 3)
    indices = add(struct.pack('<3H', 0, 1, 2), "SCALAR", 5123, 3)
    inverse = []
    for i in joints:
        x, y, z = xyz[i]
        inverse += [1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, -x, -y, -z, 1]
    inverse_idx = add(struct.pack('<' + 'f'*len(inverse), *inverse),
                      "MAT4", 5126, len(joints))
    while len(binary) % 4: binary.append(0)
    gltf = {"asset": {"version": "2.0", "generator": "OCTO test-only rig"},
            "scene": 0, "scenes": [{"nodes": [0]}], "nodes": nodes,
            "skins": [{"name": "Octopus_Rig", "joints": joints, "skeleton": 1,
                       "inverseBindMatrices": inverse_idx}],
            "meshes": [{"name": "Octopus_Skin_Mesh",
                        "primitives": [{"attributes": {"POSITION": pos,
                          "NORMAL": norm, "TEXCOORD_0": uv,
                          "JOINTS_0": jj, "WEIGHTS_0": ww}, "indices": indices}]}],
            "buffers": [{"byteLength": len(binary)}],
            "bufferViews": views, "accessors": accessors}
    data = json.dumps(gltf, separators=(",", ":")).encode()
    data += b' ' * (-len(data) % 4)
    glb = struct.pack("<4sII", b"glTF", 2, 12 + 8 + len(data) + 8 + len(binary))
    glb += struct.pack("<I4s", len(data), b"JSON") + data
    glb += struct.pack("<I4s", len(binary), b"BIN\x00") + binary
    path = Path(dest)
    path.parent.mkdir(exist_ok=True, parents=True)
    path.write_bytes(glb)
    print(f"SYNTHETIC_RIG_CREATED bones=193 bytes={len(glb)}")


if __name__ == "__main__":
    write_fixture(sys.argv[1])
