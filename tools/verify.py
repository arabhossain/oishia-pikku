#!/usr/bin/env python3
"""Run host checks; optionally build a pinned release and archive its artifacts."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--firmware', action='store_true')
parser.add_argument('--installed', action='store_true', help='Use locally installed dependencies instead of isolated release profile')
parser.add_argument('--library', type=Path, action='append', default=[], help='Explicit local library for installed builds; pinned versions are still checked')
parser.add_argument('--browser', action='store_true', help='Chrome must already listen on localhost:9333')
parser.add_argument('--artifacts', type=Path, default=root / 'artifacts')
args = parser.parse_args()

def run(command, **kwargs):
    return subprocess.run(command, cwd=root, check=True, **kwargs)

run(['python3', 'tools/embed_dashboard.py', '--check'])
dashboard = (root / 'learning/dashboard.html').read_text()
zone_select = re.search(r'<select id="timezone"[^>]*>(.*?)</select>', dashboard, re.S)
dashboard_zones = set(re.findall(r'<option value="([^"]+)"', zone_select.group(1))) if zone_select else set()
firmware_zones = set(re.findall(r'\{"([^"]+)", "[^"]+"\}', (root / 'learning/OishiaTimeZones.h').read_text()))
if not dashboard_zones or dashboard_zones != firmware_zones:
    raise SystemExit(f'Timezone list mismatch: dashboard-only={sorted(dashboard_zones-firmware_zones)}, firmware-only={sorted(firmware_zones-dashboard_zones)}')
with tempfile.TemporaryDirectory(prefix='oishia-tests-') as temp:
    for name in ('network_rules', 'production_rules', 'pet_behavior', 'http_server', 'settings_json'):
        binary = str(Path(temp) / name)
        run(['g++', '-std=c++17', '-Wall', '-Wextra', '-Werror', '-I', 'tests/arduino_stubs',
             '-I', 'libraries/ArduinoJson/src',
             f'tests/{name}_test.cpp', '-o', binary])
        run([binary])
run(['node', '--check', 'learning/dashboard-ble.js'])
run(['node', '--check', 'learning/dashboard-face.js'])
run(['node', '--check', 'tests/dashboard_browser_test.mjs'])
if args.browser:
    run(['node', 'tests/dashboard_browser_test.mjs'])
    run(['node', 'tests/dashboard_browser_test.mjs'], env={**os.environ, 'OISHIA_TEST_BLE': '1'})
if args.firmware:
    cli = os.environ.get('ARDUINO_CLI') or shutil.which('arduino-cli')
    if not cli:
        raise SystemExit('Set ARDUINO_CLI or install Arduino CLI 1.5.1.')
    cli_version = run([cli, 'version'], capture_output=True, text=True).stdout.strip()
    if not re.search(r'Version:\s*1\.5\.1\b', cli_version):
        raise SystemExit('Release verification requires Arduino CLI 1.5.1.')
    args.artifacts.mkdir(parents=True, exist_ok=True)
    source_paths = [p for p in (root / 'learning').iterdir()
                    if p.is_file() and p.suffix in ('.ino', '.cpp', '.h', '.html', '.js', '.yaml')]
    source_paths += list((root / 'learning/assets').glob('*'))
    sources = {str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest() for p in source_paths}
    with tempfile.TemporaryDirectory(prefix='oishia-build-') as temp:
        command = [cli, 'compile', '--jobs', '2', '--build-path', temp]
        command += ['--fqbn', 'esp32:esp32:esp32:PartitionScheme=huge_app'] if args.installed else ['--profile', 'release']
        for library in args.library:
            command += ['--library', str(library.resolve())]
        result = run(command + ['learning'], capture_output=True, text=True)
        if any(hashlib.sha256((root / name).read_bytes()).hexdigest() != digest for name, digest in sources.items()):
            raise SystemExit('Source changed during the build; rerun verification before releasing artifacts.')
        print(result.stdout)
        print(result.stderr)
        (args.artifacts / 'build.log').write_text(result.stdout + result.stderr)
        expected = dict(re.findall(r'^\s+- ([^:\n]+) \(([^)]+)\)$', (root / 'learning/sketch.yaml').read_text(), re.M))
        dependencies = {}
        for directory in json.loads((Path(temp) / 'libraries.cache').read_text()):
            directory = Path(directory)
            properties = next((p / 'library.properties' for p in (directory, directory.parent)
                               if (p / 'library.properties').exists()), None)
            if properties:
                fields = dict(line.split('=', 1) for line in properties.read_text().splitlines() if '=' in line)
                if 'name' in fields and 'version' in fields:
                    dependencies[fields['name']] = fields['version']
        for name, version in expected.items():
            if dependencies.get(name) != version:
                raise SystemExit(f'{name}: expected {version}, got {dependencies.get(name)}')
        options = json.loads((Path(temp) / 'build.options.json').read_text())
        if args.installed and '/esp32/3.3.11' not in options.get('hardwareFolders', ''):
            raise SystemExit('Installed build did not use the pinned ESP32 core 3.3.11.')
        # This gate intentionally leaves growth room below a 1.875 MiB slot,
        # even though the shipping USB build still uses the 3 MiB partition.
        binary = Path(temp) / 'learning.ino.bin'
        if binary.stat().st_size > 1572864:
            raise SystemExit('Firmware exceeds the 1.5 MiB release size budget.')
        for pattern in ('*.bin', '*.elf', '*.map', 'build.options.json', 'partitions.csv'):
            for source in Path(temp).glob(pattern):
                shutil.copy2(source, args.artifacts / source.name)
    files = {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in args.artifacts.iterdir()
             if p.is_file() and p.suffix in ('.bin', '.elf', '.map')}
    (args.artifacts / 'manifest.json').write_text(json.dumps({
        'cli': cli_version, 'isolatedProfile': not args.installed, 'libraries': dependencies,
        'profileSha256': hashlib.sha256((root / 'learning/sketch.yaml').read_bytes()).hexdigest(),
        'files': files, 'sources': sources}, indent=2) + '\n')
print('Requested verification completed.')
