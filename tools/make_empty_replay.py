import hashlib
import json
from pathlib import Path
root=Path(__file__).resolve().parents[1]
def digest(path): return hashlib.sha256((root/path).read_bytes()).hexdigest()
replay={"schema_version":1,"core_build_id":"P0-format-fixture-not-simulation-recording",
        "scene_sha256":digest('tests/scenes/P0_empty.json'),
        "profile_sha256":digest('profiles/default.toml'),"numeric_backend":"std-cxx20-empty-world",
        "cpu_arch":"x86_64","seed":42,"fixed_dt":1/120,"snapshot_version":1,
        "start_tick":0,"intents":[],"events":[]}
(root/'replays/golden/P0_empty.json').write_text(json.dumps(replay,indent=2)+'\n',encoding='utf-8')
