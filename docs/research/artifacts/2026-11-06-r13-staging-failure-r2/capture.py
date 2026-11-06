"""Preserve a failed delivery check; no subject is executed as code."""
from pathlib import Path
import base64, hashlib, json, subprocess, sys
root=Path.cwd().resolve()
out=Path(__file__).resolve().parent
assert not (out/'failure-and-index.json').exists()
checker=root/'docs/research/2026-11-06-r13-delivery-check.py'
raw=checker.read_bytes()
sha=lambda b:hashlib.sha256(b).hexdigest().upper()
assert sha(raw)=='D71CFF476DA4290FFED8221D2C1135A3E02693B1B3136DE79047F212FAD96BED'
result=subprocess.run(['git','diff','--cached','--check'],cwd=root,stdin=subprocess.DEVNULL,stdout=subprocess.PIPE,stderr=subprocess.PIPE,creationflags=subprocess.CREATE_NO_WINDOW if sys.platform=='win32' else 0)
assert result.returncode!=0
index=(root/'.git/index').read_bytes()
(out/'failed-check-source.py').write_bytes(raw)
record=dict(scope='FAILED_DELIVERY_CHECK_NOT_TARGET_EXECUTION',original_checker_observed_child_raw_exit=2,current_read_only_reproduction_exit=result.returncode,stdout_raw_base64=base64.b64encode(result.stdout).decode('ascii'),stderr_raw_base64=base64.b64encode(result.stderr).decode('ascii'),checker_sha256=sha(raw),postfailure_precorrection_git_index_bytes=len(index),postfailure_precorrection_git_index_sha256=sha(index),postfailure_precorrection_git_index_raw_base64=base64.b64encode(index).decode('ascii'),all410_source_and7_frozen_delivery_and266_partial_output_checks_reached_before_git_whitespace_failure=True,source_archive_manifest_sha256='AC5AAE3957A58B56A7CB3128E95FFE35C4084DAA5A6024ABD9CC36D26AF17BE6',target_execution=False)
assert base64.b64decode(record['postfailure_precorrection_git_index_raw_base64'],validate=True)==index
(out/'failure-and-index.json').write_bytes((json.dumps(record,indent=2)+'\n').encode())
print(json.dumps(dict(reproduction_raw_exit=result.returncode,stdout_bytes=len(result.stdout),stderr_bytes=len(result.stderr),index_bytes=len(index),index_sha256=sha(index))))
