from pathlib import Path
root = Path(__file__).resolve().parents[1]
for path in (root / "core").rglob("*"):
    if path.suffix in {".hpp", ".cpp"}:
        text = path.read_text(encoding="utf-8").lower()
        if "#include" in text and any("godot" in line for line in text.splitlines() if "#include" in line):
            raise SystemExit(f"engine dependency in core: {path}")
print("P0 core boundary: passed")
