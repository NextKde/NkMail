"""Keep both shipped catalogs complete, extracted from current sources, and format-safe."""
import collections
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
root = Path(__file__).resolve().parents[2]

def entries(path):
    tree = ET.parse(path)
    return {(context.findtext('name'), message.findtext('source')): message
            for context in tree.findall('context') for message in context.findall('message')
            if message.find('translation') is None or message.find('translation').get('type') not in ('obsolete', 'vanished')}

with tempfile.TemporaryDirectory() as temporary:
    extracted = Path(temporary) / 'source.ts'
    subprocess.run([sys.argv[1], str(root / 'src'), str(root / 'qml'), '-no-obsolete', '-ts', str(extracted)], check=True)
    expected = entries(extracted).keys()
    for locale in ('zh_CN', 'en'):
        catalog = entries(root / 'translations' / f'nkmail_{locale}.ts')
        assert catalog.keys() == expected, (locale, 'catalog/source mismatch', expected - catalog.keys(), catalog.keys() - expected)
        for key, message in catalog.items():
            translation = message.find('translation')
            assert translation is not None and translation.get('type') != 'unfinished', key
            text = ''.join(translation.itertext())
            assert text.strip(), key
            placeholders = lambda s: collections.Counter(re.findall(r'%(?:\d+|n)', s))
            assert placeholders(key[1]) == placeholders(text), key
        print(f'{locale}: {len(catalog)} complete entries with matching placeholders')
