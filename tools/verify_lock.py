import argparse
import hashlib
import json
import re
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument('--require-verified',action='store_true')
args = parser.parse_args()
lock=json.loads((Path(__file__).resolve().parents[1]/'toolchain.lock.json').read_text(encoding='utf-8-sig'))
failures=[]
for name,item in lock['tools'].items():
    if item['status'] != 'verified': failures.append(f'{name}: {item["status"]}')
    if item.get('sha256') and not re.fullmatch('[0-9a-f]{64}',item['sha256']):
        raise SystemExit(f'invalid hash: {name}')
    if args.require_verified and (not item.get('url') or not item.get('sha256') or not item.get('version')):
        failures.append(f'{name}: incomplete version/source/archive SHA256')
    if args.require_verified:
        archive = item.get('archive_path')
        if not archive or not Path(archive).is_file():
            failures.append(f'{name}: official archive not materialized for verification')
        elif hashlib.sha256(Path(archive).read_bytes()).hexdigest() != item.get('sha256'):
            failures.append(f'{name}: archive SHA256 mismatch')
print('\n'.join(failures) or 'all tool provenance verified')
if args.require_verified and failures: raise SystemExit(2)
