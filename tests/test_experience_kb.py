import json
from pathlib import Path
import sys
import tempfile
import unittest

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'tools'))
from experience_kb import validate, search, handoff, load_records


def record(**changes):
    obj = {'schema_version':1,'id':'BUG-2026-001','title':'Relay early succeeded',
           'component':'relay','status':'verified',
           'symptom':'Relay reports success while DSH running',
           'root_cause':'Relay completion and native session completion are distinct',
           'resolution':'Confirm native terminal plus artifact stable before review',
           'prevention':'Use idempotent run_id dedupe',
           'references':['https://github.com/acme/demo/issues/3'],
           'tags':['relay','completion'],
           'verification':[{'method':'Codex reviewed native session and retested','result':'passed'}],
           'reviewer':'Codex'}
    obj.update(changes)
    return obj


class Tests(unittest.TestCase):
    def test_valid_verified(self):
        validate(record())

    def test_missing(self):
        x = record()
        x.pop('symptom')
        with self.assertRaises(ValueError): validate(x)

    def test_reject_premature_verify(self):
        with self.assertRaises(ValueError): validate(record(root_cause='unknown'))

    def test_reject_unreviewed(self):
        with self.assertRaises(ValueError): validate(record(reviewer='pending'))

    def test_reject_unproven(self):
        with self.assertRaises(ValueError): validate(record(verification=[]))

    def test_candidate_unresolved(self):
        validate(record(status='candidate',root_cause='unconfirmed',verification=[],reviewer='pending'))

    def test_superseded_requires_link(self):
        with self.assertRaises(ValueError): validate(record(status='superseded'))

    def test_prevent_private_paths(self):
        with self.assertRaises(ValueError): validate(record(symptom='see /mnt/private/codex'))

    def test_prevent_loopback(self):
        with self.assertRaises(ValueError): validate(record(symptom='http://127.0.0.1:42356/session'))

    def test_references_public_only(self):
        with self.assertRaises(ValueError): validate(record(references=['http://host.local/log']))

    def test_search(self):
        self.assertEqual(search([record()], 'Relay completion')[0]['id'], 'BUG-2026-001')

    def test_handoff_only_verified(self):
        draft=record(id='BUG-2026-002',status='candidate',
                     root_cause='unconfirmed',verification=[],reviewer='pending')
        bundle=handoff([record(),draft])
        self.assertEqual(len(bundle['entries']),1)
        self.assertEqual(bundle['integration_status'],'pending_native_memory_ingestion')

    def test_duplicate(self):
        with tempfile.TemporaryDirectory() as td:
            for name in ('a','b'):
                (Path(td)/(name+'.json')).write_text(json.dumps(record()))
            with self.assertRaises(ValueError): load_records(Path(td))

    def test_valid_directory(self):
        with tempfile.TemporaryDirectory() as td:
            (Path(td)/'a.json').write_text(json.dumps(record()))
            self.assertEqual(len(load_records(Path(td))),1)


if __name__ == '__main__':
    unittest.main()
