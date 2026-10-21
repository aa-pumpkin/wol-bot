#!/usr/bin/env python3
"""Create the shared secret for the Android widget.

Adds WIDGET_TOPIC and WIDGET_KEY to include/secrets.h (only if they are not
there yet) and copies them to android/secrets.properties for the app build.
The values are never printed.
"""

import re
import secrets
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SECRETS_H = ROOT / "include" / "secrets.h"
PROPERTIES = ROOT / "android" / "secrets.properties"


def read_define(text, name):
    match = re.search(rf'^#define {name} "([^"]*)"', text, re.MULTILINE)
    return match.group(1) if match else None


def main():
    if not SECRETS_H.exists():
        raise SystemExit("include/secrets.h is missing, create it from secrets.h.example first")

    text = SECRETS_H.read_text()
    topic = read_define(text, "WIDGET_TOPIC")
    key = read_define(text, "WIDGET_KEY")

    if topic and key:
        print("Widget secret already exists in include/secrets.h, reusing it.")
    else:
        topic = "wol-" + secrets.token_urlsafe(24).replace("-", "x").replace("_", "y")
        key = secrets.token_hex(32)
        text = text.rstrip("\n") + (
            "\n\n"
            "// Android widget, written by tools/widget_secret.py\n"
            f'#define WIDGET_TOPIC "{topic}"\n'
            f'#define WIDGET_KEY "{key}"\n'
        )
        SECRETS_H.write_text(text)
        print("Added a new widget secret to include/secrets.h.")

    PROPERTIES.write_text(f"topic={topic}\nkey={key}\n")
    PROPERTIES.chmod(0o600)
    print("Wrote android/secrets.properties.")


if __name__ == "__main__":
    main()
