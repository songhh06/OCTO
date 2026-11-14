"""Version 2 public file contracts; native semantic validation is exercised in P2.V12_io."""
import copy
import json
from pathlib import Path
import jsonschema
ROOT=Path(__file__).resolve().parents[2]
def load(name):
    return json.loads((ROOT/name).read_text(encoding="utf-8-sig"))
scene=load("tests/scenes/V12_runtime.json")
jsonschema.validate(load("tests/scenes/P2_eight_cups.json"),load("schemas/scene-v2.schema.json"))
inputs=load("tests/scenes/V12_inputs.json")
for name,doc in [("scene",scene),("inputs",inputs)]:
    schema=load(f"schemas/{name}-v2.schema.json")
    jsonschema.Draft202012Validator.check_schema(schema)
    jsonschema.validate(doc,schema)
    bad=copy.deepcopy(doc);bad["schema_version"]=999
    assert list(jsonschema.Draft202012Validator(schema).iter_errors(bad))
bad=copy.deepcopy(inputs);bad["events"][0]["activation"]=[2]*7
assert list(jsonschema.Draft202012Validator(load("schemas/inputs-v2.schema.json")).iter_errors(bad))
replay=ROOT/"docs/progress/physics/V12-input-replay.json"
if replay.exists():jsonschema.validate(json.loads(replay.read_text(encoding="utf-8-sig")),load("schemas/replay-v2.schema.json"))
print("P2 versioned scene/input/replay schemas passed")
