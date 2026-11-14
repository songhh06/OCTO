"""Generate versioned P0 schemas; changes require reviewed schema version changes."""
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
NUMBER = {"type": "number"}
POSITIVE = {"type": "number", "exclusiveMinimum": 0}
NONNEG = {"type": "number", "minimum": 0}
ID = {"type": "integer", "minimum": 0, "maximum": 4294967295}
U64 = {"type": "integer", "minimum": 0, "maximum": 18446744073709551615}
HASH = {"type": "string", "pattern": "^[0-9a-f]{64}$"}
TEXT = {"type": "string", "minLength": 1}

def array(item, count=None):
    result = {"type": "array", "items": item}
    if count is not None:
        result.update(minItems=count, maxItems=count)
    return result

def obj(properties):
    return {"type": "object", "properties": properties,
            "required": list(properties), "additionalProperties": False}

VEC = array(NUMBER, 3)
QUAT = array(NUMBER, 4)  # x,y,z,w, unit length checked semantically
POSE = obj({"position": VEC, "rotation_xyzw": QUAT})
material = obj({"id": ID, "sealability": {"type":"number","minimum":0,"maximum":1},
                "roughness": NONNEG, "leak_conductance": NONNEG, "friction": NONNEG,
                "peel_energy": POSITIVE, "source": TEXT,
                "grade": {"enum":["ASSUMED","MEASURED","TUNED_FOR_GAMEPLAY"]}})
body = obj({"id": ID, "mass": POSITIVE, "inertia_body": array(VEC,3),
            "pose": POSE, "linear_velocity": VEC, "angular_velocity": VEC,
            "shape": obj({"type":{"const":"box"}, "half_extents": array(POSITIVE,3)}),
            "material_id": ID})
scene = obj({"schema_version":{"const":1}, "unit_system":{"const":"SI"},
             "coordinate_system":{"const":"RH_Y_UP"}, "gravity": VEC,
             "water":obj({"density":POSITIVE,"velocity":VEC}),
             "octopus_profile":TEXT,
             "static_colliders":array(obj({"id":ID,"mesh_sha256":HASH,
                                            "transform":POSE,"material_id":ID})),
             "dynamic_bodies":array(body), "surface_materials":array(material),
             "objectives":array(obj({"type":{"const":"hold_box"},"body_id":ID,
                                     "target":VEC,"radius":POSITIVE,"duration":POSITIVE}))})
intent = obj({"tick":U64,"selected":{"type":"integer","minimum":0,"maximum":7},
              "target_world":VEC,"reach":{"type":"number","minimum":-1,"maximum":1},
              "twist":{"type":"number","minimum":-1,"maximum":1},
              "suction":{"type":"boolean"},"jet":{"type":"number","minimum":0,"maximum":1}})
replay = obj({"schema_version":{"const":1}, "core_build_id":TEXT,
              "scene_sha256":HASH,"profile_sha256":HASH,"numeric_backend":TEXT,
              "cpu_arch":TEXT,"seed":U64,"fixed_dt":{"const":1/120},
              "snapshot_version":{"const":1},"start_tick":U64,
              "intents":array(intent),"events":array(obj({"tick":U64,"kind":{"const":"checkpoint"},"state_sha256":HASH}))})
for name, schema in [("scene",scene),("replay",replay)]:
    schema["$schema"] = "https://json-schema.org/draft/2020-12/schema"
    schema["$id"] = f"urn:octopus:{name}:1"
    (ROOT / "schemas" / f"{name}.schema.json").write_text(json.dumps(schema,indent=2)+"\n",encoding="utf-8")
