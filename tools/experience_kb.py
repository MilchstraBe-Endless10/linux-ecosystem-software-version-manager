#!/usr/bin/env python3
"""Evidence-first incident library. Stdlib only; no network, shell or DSH access."""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / 'docs' / 'engineering-knowledge' / 'incidents'
STATUSES = {'candidate', 'verified', 'superseded'}
REQUIRED = ('schema_version', 'id', 'title', 'component', 'status', 'symptom',
            'root_cause', 'resolution', 'prevention', 'references', 'tags',
            'verification', 'reviewer')
PRIVATE = re.compile(r'(?i)(?:/mnt/|/home/|/root/|127\.0\.0\.1|localhost:\d+|sessionId=|(?<![\w])gh[pousr]_[a-z0-9]{12,}|-----BEGIN [A-Z ]*PRIVATE KEY-----)')
ID = re.compile(r'^BUG-[0-9]{4}-[0-9]{3,5}$')


def load_records(directory: Path) -> list[dict]:
    if not directory.is_dir() or directory.is_symlink():
        raise ValueError('knowledge directory not available or is a symlink')
    result = []
    paths = sorted(directory.glob('*.json'))
    if len(paths) > 1000:
        raise ValueError('record limit exceeded')
    for p in paths:
        if p.is_symlink() or p.stat().st_size > 65536:
            raise ValueError(f'unsafe or oversized record: {p.name}')
        item = json.loads(p.read_text(encoding='utf-8'))
        validate(item)
        result.append(item)
    keys = [r['id'] for r in result]
    if len(set(keys)) != len(keys):
        raise ValueError('duplicate incident id')
    return result


def validate(record: dict) -> None:
    if not isinstance(record, dict):
        raise ValueError('record must be an object')
    if any(key not in record for key in REQUIRED):
        raise ValueError('missing fields: ' + ', '.join(key for key in REQUIRED if key not in record))
    if record['schema_version'] != 1 or not isinstance(record['id'], str) or not ID.fullmatch(record['id']):
        raise ValueError('invalid schema version or ID')
    if not isinstance(record['status'], str) or record['status'] not in STATUSES:
        raise ValueError('invalid status')
    for key in ('title', 'component', 'symptom', 'root_cause', 'resolution', 'prevention', 'reviewer'):
        if not isinstance(record[key], str) or not record[key].strip():
            raise ValueError('missing or invalid ' + key)
    if not isinstance(record['references'], list) or not isinstance(record['tags'], list) or not isinstance(record['verification'], list):
        raise ValueError('references, tags, verification must be lists')
    if not all(isinstance(tag, str) and tag for tag in record['tags']):
        raise ValueError('invalid tags')
    if not all(isinstance(link, str) and re.fullmatch(r'https://github\.com/[A-Za-z0-9_.-]+/[A-Za-z0-9_.-]+/(?:issues|pull)/\d+(?:#issuecomment-\d+)?', link) for link in record['references']):
        raise ValueError('references must be public GitHub issue/PR URLs')
    if record['status'] == 'verified':
        if record['reviewer'].lower() in {'unknown', 'pending', 'unreviewed'}:
            raise ValueError('verified record needs reviewer')
        if record['root_cause'].strip().lower() in {'unknown', 'unconfirmed', 'pending', '待确认'}:
            raise ValueError('verified record needs a proven cause')
        if not record['verification'] or not all(isinstance(e, dict) and e.get('result') == 'passed' and isinstance(e.get('method'), str) and e['method'] for e in record['verification']):
            raise ValueError('verified record requires successful independent verification')
    if record['status'] == 'superseded' and not record.get('superseded_by'):
        raise ValueError('superseded record must identify replacement')
    blob = json.dumps(record, ensure_ascii=False)
    if PRIVATE.search(blob):
        raise ValueError('potential private environment/session/credential reference')


def search(records: list[dict], query: str, limit: int = 5) -> list[dict]:
    terms = [m.lower() for m in re.findall(r'[A-Za-z0-9][A-Za-z0-9_+.-]*|[\u4e00-\u9fff]{2,}', query)]
    if not terms:
        return []
    ranked = []
    for item in records:
        title = (item['title'] + ' ' + ' '.join(item['tags'])).lower()
        body = (' '.join(str(item[k]) for k in ('symptom', 'root_cause', 'resolution', 'prevention'))).lower()
        score = sum(4 for t in terms if t in title) + sum(1 for t in terms if t in body)
        if score:
            ranked.append((score, item))
    ranked.sort(key=lambda pair: (-pair[0], pair[1]['id']))
    return [item for _, item in ranked[:limit]]


def handoff(records: list[dict]) -> dict:
    # Structured payload only. Codex must invoke the *real* DSH memory API.
    entries = []
    for r in records:
        if r['status'] != 'verified':
            continue
        entries.append({'id': r['id'], 'title': r['title'], 'component': r['component'],
                        'root_cause': r['root_cause'], 'resolution': r['resolution'],
                        'prevention': r['prevention'], 'references': r['references'],
                        'tags': r['tags']})
    return {'schema_version': 1, 'integration_status': 'pending_native_memory_ingestion',
            'note': 'Codex must discover actual DSH memory tools, review these entries, then invoke them. Never upload private memory.',
            'entries': entries}


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('action', choices=('validate', 'search', 'handoff'))
    p.add_argument('query', nargs='?', default='')
    p.add_argument('--directory', type=Path, default=DEFAULT)
    p.add_argument('--limit', type=int, default=5)
    args = p.parse_args()
    try:
        records = load_records(args.directory)
        if args.action == 'validate':
            print(f'VALID {len(records)} incident records')
        elif args.action == 'search':
            if not args.query:
                raise ValueError('search requires a query')
            print(json.dumps(search(records, args.query, max(1, min(args.limit, 20))), ensure_ascii=False, indent=2))
        else:
            print(json.dumps(handoff(records), ensure_ascii=False, indent=2))
    except (OSError, ValueError, json.JSONDecodeError) as exc:
        print(f'knowledge check failed: {exc}', file=sys.stderr)
        return 1
    return 0

if __name__ == '__main__':
    raise SystemExit(main())
