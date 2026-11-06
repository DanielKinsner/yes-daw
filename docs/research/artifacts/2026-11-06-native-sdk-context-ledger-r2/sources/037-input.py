import json
from pathlib import Path
p=Path('build-ci/20261106-native-sdk-context-ledger-r2')
x=json.loads((p/'selected-sdk-ledger.json').read_text())
for r in x['rows']:
 print(r['id'],r['source'],r['lines'], 'params='+str(r['value_parameters']), 'locals='+str(r['locals']), 'mat='+str(r['materializations']), 'refs='+str(r['reference_parameters']), 'children='+','.join(r['children']))
