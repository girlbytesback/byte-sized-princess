#!/usr/bin/env python3
"""
encrypt_vault.py — turns vault.json into vault-blob.h for the pocket vault charm.

put this file in portal/captive-portal/ (next to captive-portal.ino) and run:

    pip install cryptography        (once)
    python3 encrypt_vault.py

first run: writes an example vault.json — the pans ("shades") and the accounts ("entries") — then stops.
every run after: asks for your vault passphrase (twice), encrypts vault.json,
writes vault-blob.h next to the sketch. upload the sketch and you're done.

what it does, in one breath: key = PBKDF2-HMAC-SHA256(passphrase, random salt, 20000 rounds),
blob = AES-256-GCM(key, random nonce, the json). the chip repeats the same key derivation
when you type the passphrase on your phone. a wrong passphrase fails the GCM check,
so the chip never shows garbage — it just says no.

keep vault.json and vault-blob.h OUT of git (they're in .gitignore). vault.json is your
plaintext; vault-blob.h is only as safe as your passphrase, and a repo is forever.
"""
import json, os, sys, getpass, hashlib, secrets

ITER = 20000            # must match VAULT_ITER in the sketch; the script writes it into the header
VAULT = sys.argv[1] if len(sys.argv) > 1 else "vault.json"
OUT   = sys.argv[2] if len(sys.argv) > 2 else "vault-blob.h"

# vault.json is your whole portal in one file:
#   "shades"  = the pans on the palette page: shade (the name), what (the category word), color, finish
#               (matte, satin, shimmer, glitter or duochrome). rename, recolor, reorder, add, remove.
#   "entries" = the accounts: c = which shade's "what" it belongs to, s = site, u = username (can be ""), p = password.
#               several entries can share a shade; they show up as a list when you tap it.
EXAMPLE = {
    "shades": [
        {"shade": "Joy",         "what": "email",     "color": "#eccfcf", "finish": "satin"},
        {"shade": "Cherished",   "what": "socials",   "color": "#d8c0cf", "finish": "matte"},
        {"shade": "Happiness",   "what": "school",    "color": "#dfa8b9", "finish": "matte"},
        {"shade": "Self love",   "what": "games",     "color": "#e0cbc7", "finish": "shimmer"},
        {"shade": "Radiate",     "what": "streaming", "color": "#b59a9a", "finish": "matte"},
        {"shade": "Cosmic love", "what": "music",     "color": "#9e9dbe", "finish": "glitter"},
        {"shade": "Moon magic",  "what": "bank",      "color": "#b0a4b8", "finish": "glitter"},
        {"shade": "Blissful",    "what": "shopping",  "color": "#c7c5c7", "finish": "glitter"},
        {"shade": "Empowered",   "what": "wifi",      "color": "#807f94", "finish": "shimmer"},
        {"shade": "Adored",      "what": "work",      "color": "#7e3f45", "finish": "shimmer"},
        {"shade": "Devoted",     "what": "phone",     "color": "#80233d", "finish": "satin"},
        {"shade": "Fearless",    "what": "cloud",     "color": "#b87552", "finish": "shimmer"},
        {"shade": "Wild heart",  "what": "travel",    "color": "#5c462f", "finish": "duochrome"},
        {"shade": "Starlit",     "what": "notes",     "color": "#7c8e7b", "finish": "glitter"},
        {"shade": "Stardust",    "what": "photos",    "color": "#5b2a37", "finish": "glitter"}
    ],
    "entries": [
    ]
}

def die(msg):
    print("error:", msg, file=sys.stderr)
    sys.exit(1)

try:
    from cryptography.hazmat.primitives.ciphers.aead import AESGCM
except ImportError:
    die("missing library. run:  pip install cryptography")

if not os.path.exists(VAULT):
    with open(VAULT, "w", encoding="utf-8") as f:
        json.dump(EXAMPLE, f, indent=2, ensure_ascii=False)
        f.write("\n")
    print(f"wrote an example {VAULT}. edit it: \"shades\" are the pans, \"entries\" are the accounts (c = shade, s = site, u = username, p = password). then run me again.")
    sys.exit(0)

with open(VAULT, encoding="utf-8") as f:
    try:
        vault = json.load(f)
    except json.JSONDecodeError as e:
        die(f"{VAULT} isn't valid json: {e}")

if isinstance(vault, list):                      # old style: just a list of entries
    vault = {"entries": vault}
if not isinstance(vault, dict) or not isinstance(vault.get("entries"), list) or not vault["entries"]:
    die(f"{VAULT} needs an \"entries\" list with at least one entry (run me with no {VAULT} to get an example)")
for i, e in enumerate(vault["entries"]):
    if not isinstance(e, dict) or not all(isinstance(e.get(k, ""), str) for k in ("c", "s", "u", "p")) or not e.get("s") or not e.get("p"):
        die(f"entry {i} needs at least \"s\" (site) and \"p\" (password) as strings; \"c\" (shade) and \"u\" (username) are optional")
shades = vault.get("shades", [])
if not isinstance(shades, list):
    die("\"shades\" should be a list")
for i, sh in enumerate(shades):
    if not isinstance(sh, dict) or not sh.get("what") or not sh.get("shade"):
        die(f"shade {i} needs \"shade\" (its name) and \"what\" (its category word)")
known = {str(sh["what"]).lower() for sh in shades}
for e in vault["entries"]:
    if shades and e.get("c") and e["c"].lower() not in known:
        print(f"note: entry \"{e['s']}\" has c = \"{e['c']}\", which isn't one of your shades; it'll show under \"other\".")

pw = getpass.getpass("vault passphrase: ").strip()
if pw != getpass.getpass("again: ").strip():
    die("the two passphrases don't match")
if len(pw) < 16:
    print("warning: that's short. five random words (diceware) is the target — the blob is only as strong as this.")
    if input("continue anyway? [y/N] ").strip().lower() != "y":
        sys.exit(1)

salt  = secrets.token_bytes(16)
nonce = secrets.token_bytes(12)
key   = hashlib.pbkdf2_hmac("sha256", pw.encode("utf-8"), salt, ITER, dklen=32)
plain = json.dumps(vault, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
ct_tag = AESGCM(key).encrypt(nonce, plain, None)
ct, tag = ct_tag[:-16], ct_tag[-16:]

def c_array(name, data):
    rows = [", ".join(f"0x{b:02x}" for b in data[i:i + 16]) for i in range(0, len(data), 16)]
    return f"const uint8_t {name}[{len(data)}] = {{\n  " + ",\n  ".join(rows) + "\n};\n"

with open(OUT, "w", encoding="utf-8") as f:
    f.write("// generated by encrypt_vault.py — do not commit. re-run the script to change the vault.\n")
    f.write(f"// {len(vault['entries'])} entries, {len(shades)} shades, {len(plain)} bytes of json, AES-256-GCM, PBKDF2-HMAC-SHA256 x {ITER}\n")
    f.write("#pragma once\n#include <stdint.h>\n#include <stddef.h>\n\n")
    f.write(f"#define VAULT_ITER {ITER}u\n")
    f.write(f"#define VAULT_CT_LEN {len(ct)}u\n\n")
    f.write(c_array("VAULT_SALT", salt))
    f.write(c_array("VAULT_NONCE", nonce))
    f.write(c_array("VAULT_TAG", tag))
    f.write(c_array("VAULT_CT", ct))

print(f"ok: {len(vault['entries'])} entries, {len(shades)} shades -> {OUT} ({len(ct)} bytes). now upload the sketch.")
