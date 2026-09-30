"""Verify the imported source against its reviewed content fingerprint."""

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sources = json.loads((ROOT / "tools/sources.json").read_text(encoding="utf-8"))["sources"]
for source in sources:
    content = (ROOT / source["destination"]).read_text(encoding="utf-8").replace("\r\n", "\n").encode("utf-8")
    if hashlib.sha256(content).hexdigest() != source["sha256"]:
        raise SystemExit(f"Imported source changed: {source['destination']}")
print(f"Verified {len(sources)} imported files")
