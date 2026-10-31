#!/usr/bin/env python3
"""Check the shipped Chinese catalog against Qt's actual source extraction."""
import argparse
from collections import Counter
from pathlib import Path
import re
import subprocess
import tempfile
import xml.etree.ElementTree as ET


def messages(path):
    root = ET.parse(path).getroot()
    result = {}
    for context in root.findall("context"):
        for message in context.findall("message"):
            translation = message.find("translation")
            if translation is not None and translation.get("type") in {"obsolete", "vanished"}:
                continue
            key = (context.findtext("name"), message.findtext("source"), message.findtext("comment", ""))
            if key in result:
                raise ValueError(f"Duplicate translation: {key}")
            result[key] = translation
    return root, result


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--lupdate", required=True)
    args = parser.parse_args()
    source = Path(__file__).resolve().parents[2]
    catalog = source / "translations/nkmail_zh_CN.ts"
    root, translated = messages(catalog)
    if root.get("language") != "zh_CN":
        raise ValueError("Expected a zh_CN catalog")
    inputs = [source / "qml/Main.qml", source / "qml/MailComposer.qml",
              source / "qml/AccountDialog.qml", source / "src/MailController.cpp"]
    with tempfile.TemporaryDirectory(prefix="translation-check-", dir=Path.cwd()) as work:
        extracted = Path(work) / "sources.ts"
        subprocess.run([args.lupdate, *map(str, inputs), "-no-obsolete", "-ts", str(extracted)], check=True)
        _, expected = messages(extracted)
    if translated.keys() != expected.keys():
        raise ValueError(f"Missing: {expected.keys() - translated.keys()}; stale: {translated.keys() - expected.keys()}")
    for key, translation in translated.items():
        if translation is None or translation.get("type") == "unfinished":
            raise ValueError(f"Unfinished translation: {key}")
        text = "".join(translation.itertext())
        if not text.strip():
            raise ValueError(f"Empty translation: {key}")
        placeholders = lambda value: Counter(re.findall(r"%(?:L?\d+|n)", value))
        if placeholders(key[1]) != placeholders(text):
            raise ValueError(f"Placeholder mismatch: {key}")
    print(f"Verified {len(translated)} complete Chinese translations against application sources.")


if __name__ == "__main__":
    main()
