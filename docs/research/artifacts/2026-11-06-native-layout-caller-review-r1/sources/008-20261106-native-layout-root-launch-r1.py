"""Root's separate layout-purpose caller. Requires Python -I -S; no Clock run."""
import sys
from pathlib import Path
import hashlib
import runpy

assert sys.flags.isolated and sys.flags.no_site and sys.flags.ignore_environment
assert not sys.flags.optimize and sys.maxsize == (1 << 63) - 1
host = Path('C:/Users/SM - Dan/AppData/Local/Programs/Python/Python313/python.exe')
assert Path(sys.executable).resolve() == host.resolve()
assert sys.version_info[:3] == (3, 13, 5)
assert hashlib.sha256(host.read_bytes()).hexdigest().upper() == '5341746F92483A93E44C313DE830F2FBA2956F0759094404A16B2FED06C9A2ED'
assert len(sys.argv) == 1
root = Path(__file__).resolve().parents[1]
source = root / 'build-ci/20261106-native-object-layout-purpose-source-r1'
launcher = source / 'launch_compile.py'
assert hashlib.sha256(launcher.read_bytes()).hexdigest().upper() == '8E1CA919CD6478C3B42BAD48D531B7EBA02AB4EA3BC3A4263CDD5207FD164F9B'
sys.dont_write_bytecode = True
sys.path.insert(0, str(source))
runpy.run_path(str(launcher), run_name='__main__')
