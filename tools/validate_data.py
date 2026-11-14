"""Strict JSON Schema + SI semantic validation. Not a simulation scene loader."""
import argparse
import json
import math
from pathlib import Path
import jsonschema

ROOT = Path(__file__).resolve().parents[1]
def load(path):
    def reject(value):
        raise ValueError(f"non-finite JSON constant: {value}")
    def unique(pairs):
        result = {}
        for key, value in pairs:
            if key in result:
                raise ValueError(f"duplicate JSON key: {key}")
            result[key] = value
        return result
    if Path(path).stat().st_size > 50 * 1024 * 1024:
        raise ValueError("input exceeds 50 MiB")
    return json.loads(Path(path).read_text(encoding="utf-8-sig"),
                      parse_constant=reject, object_pairs_hook=unique)

def finite(value):
    if isinstance(value,float) and not math.isfinite(value):
        raise ValueError("non-finite number")
    if isinstance(value,dict):
        for child in value.values(): finite(child)
    if isinstance(value,list):
        for child in value: finite(child)

def validate(kind, data):
    finite(data)
    jsonschema.Draft202012Validator(load(ROOT/"schemas"/f"{kind}.schema.json")).validate(data)
    if kind == "scene":
        def ids(items):
            values = [item["id"] for item in items]
            if len(set(values)) != len(values): raise ValueError("duplicate stable ID")
            return set(values)
        materials = ids(data["surface_materials"])
        bodies = ids(data["dynamic_bodies"])
        statics = ids(data["static_colliders"])
        if bodies & statics: raise ValueError("collider ID collision")
        for item in data["dynamic_bodies"] + data["static_colliders"]:
            if item["material_id"] not in materials: raise ValueError("unknown material")
            pose = item.get("pose", item.get("transform"))
            if abs(sum(v*v for v in pose["rotation_xyzw"])-1) > 1e-6:
                raise ValueError("quaternion must be normalized")
        for body in data["dynamic_bodies"]:
            a = body["inertia_body"]
            if any(abs(a[i][j]-a[j][i]) > 1e-12 for i in range(3) for j in range(3)):
                raise ValueError("inertia must be symmetric")
            # Cholesky verifies positive definiteness (including off-diagonal terms).
            lower = [[0.0]*3 for _ in range(3)]
            for i in range(3):
                for j in range(i+1):
                    value = a[i][j]-sum(lower[i][k]*lower[j][k] for k in range(j))
                    if i == j:
                        if value <= 0: raise ValueError("inertia must be positive definite")
                        lower[i][j] = math.sqrt(value)
                    else: lower[i][j] = value/lower[j][j]
        for objective in data["objectives"]:
            if objective["body_id"] not in bodies: raise ValueError("unknown objective body")
    else:
        for key in ("intents","events"):
            ticks = [item["tick"] for item in data[key]]
            if ticks != sorted(ticks) or any(t < data["start_tick"] for t in ticks):
                raise ValueError("replay tick order violation")
        ticks = [item["tick"] for item in data["intents"]]
        if len(set(ticks)) != len(ticks): raise ValueError("one intent per tick in schema v1")

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("kind", choices=["scene","replay"])
    parser.add_argument("path")
    args = parser.parse_args()
    validate(args.kind, load(args.path))
    print(f"{args.kind}: valid")
