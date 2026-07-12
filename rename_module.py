#!/usr/bin/env python3
"""
rename_module.py  —  Rename / move a source module in a Renesas e2 studio / FSP project.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
MODES  (auto-detected)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

MODE 1 – Rename directory AND files
    old_dir != new_dir  AND  new_dir does not yet exist
    → renames src/<old_dir>/  →  src/<new_dir>/
    → renames <old_stem>.c/.h  →  <new_stem>.c/.h  inside the new dir

MODE 2 – Rename files only  (directory stays the same)
    old_dir == new_dir  OR  --file-only flag
    → renames <old_stem>.c/.h  →  <new_stem>.c/.h  inside src/<old_dir>/

MODE 3 – Move file to an existing directory  (and optionally rename)
    old_dir != new_dir  AND  src/<new_dir> already exists
    → moves  src/<old_dir>/<old_stem>.[c|h]
          →  src/<new_dir>/<new_stem>.[c|h]
    → the old directory is LEFT in place (other files may still be there)

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
COMMON ACTIONS (all modes)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  • Updates  "File :"  comment inside .c / .h
  • Updates  #include guards  in .h
  • Updates  self-#include  in .c
  • Scans ALL .c/.h under src/ and fixes  #include "<old_stem>.h"
  • Updates  .cproject  include paths
  • Deletes  stale build artifacts  PtxIotReaderApp_SPI_Debug/src/<old_dir>/
    (MODE 1 only — directory was renamed entirely)
  • Updates  PtxIotReaderApp_SPI_Debug/sources.mk

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
EXAMPLES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  # MODE 1 – rename directory + files
  python script/rename_module.py auc_app auc_nfc_card_reader auc_app_main auc_nfc_card_reader

  # MODE 2 – rename files only (same directory)
  python script/rename_module.py nfc_card_reader nfc_card_reader nfc_main nfc_core
  python script/rename_module.py nfc_card_reader --file-only nfc_main nfc_core   # equivalent

  # MODE 3 – move file from one existing dir to another + rename
  python script/rename_module.py old_dir existing_dir old_stem new_stem
"""

import argparse
import os
import shutil
import sys

# ─────────────────────────────────────────────────────────────────────────────
# I/O helpers
# ─────────────────────────────────────────────────────────────────────────────

def read(path: str) -> str:
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()

def write(path: str, text: str) -> None:
    with open(path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(f"  [updated]  {path}")

def replace_if_changed(path: str, old: str, new: str) -> bool:
    """Replace all occurrences of old→new in a file. Returns True if changed."""
    text = read(path)
    updated = text.replace(old, new)
    if updated != text:
        write(path, updated)
        return True
    return False

# ─────────────────────────────────────────────────────────────────────────────
# Per-file content updates
# ─────────────────────────────────────────────────────────────────────────────

def update_file_content(path: str, old_stem: str, new_stem: str, ext: str,
                        old_guard: str, new_guard: str) -> None:
    """Update internal comments, guards, and self-includes inside a source file."""
    text = read(path)

    # "File :" comment line
    text = text.replace(
        f"File        : {old_stem}{ext}",
        f"File        : {new_stem}{ext}",
    )

    if ext == ".h":
        text = text.replace(f"#ifndef {old_guard}", f"#ifndef {new_guard}")
        text = text.replace(f"#define {old_guard}", f"#define {new_guard}")
    else:  # .c
        text = text.replace(
            f'#include "{old_stem}.h"',
            f'#include "{new_stem}.h"',
        )

    write(path, text)

# ─────────────────────────────────────────────────────────────────────────────
# Main
# ─────────────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(
        formatter_class=argparse.RawDescriptionHelpFormatter,
        description=__doc__,
    )
    parser.add_argument("old_dir",  help="Current subdirectory name under src/")
    parser.add_argument("new_dir",
                        help="Target subdirectory name under src/  "
                             "(pass same as old_dir, or use --file-only, to rename files only)")
    parser.add_argument("old_stem", help="Current .c/.h base filename  (without extension)")
    parser.add_argument("new_stem", help="Target  .c/.h base filename  (without extension)")
    parser.add_argument("--file-only", action="store_true",
                        help="Rename files only; ignore new_dir and keep the directory name")
    parser.add_argument("--root", default=None,
                        help="Project root (default: current working directory)")
    args = parser.parse_args()

    old_dir  = args.old_dir
    new_dir  = args.old_dir if args.file_only else args.new_dir
    old_stem = args.old_stem
    new_stem = args.new_stem
    root     = os.path.abspath(args.root or os.getcwd())

    src_root     = os.path.join(root, "src")
    old_dir_path = os.path.join(src_root, old_dir)
    new_dir_path = os.path.join(src_root, new_dir)

    # ── Determine mode ───────────────────────────────────────────────────────
    same_dir      = (old_dir == new_dir)
    target_exists = os.path.isdir(new_dir_path)

    if same_dir:
        mode = 2  # rename files only
    elif not target_exists:
        mode = 1  # rename directory + files
    else:
        mode = 3  # move files to existing directory

    mode_labels = {
        1: "MODE 1 — Rename directory + files",
        2: "MODE 2 — Rename files only (same directory)",
        3: "MODE 3 — Move files to existing directory",
    }
    print(f"\nProject root : {root}")
    print(f"{mode_labels[mode]}")
    print(f"  src/{old_dir}/{old_stem}.[c|h]  →  src/{new_dir}/{new_stem}.[c|h]\n")

    # ── Sanity checks ────────────────────────────────────────────────────────
    if not os.path.isdir(old_dir_path):
        sys.exit(f"ERROR: Source directory not found: {old_dir_path}")
    if mode == 1 and target_exists:
        sys.exit(f"ERROR: Target directory already exists: {new_dir_path}\n"
                 "       (this would be MODE 3 — move — which is intentional if correct)")

    old_guard = old_stem.upper() + "_H_"
    new_guard = new_stem.upper() + "_H_"

    # ── Step 1: Update file contents ─────────────────────────────────────────
    print("── Step 1: Update file contents ────────────────────────────────────")
    for ext in (".c", ".h"):
        fpath = os.path.join(old_dir_path, old_stem + ext)
        if os.path.isfile(fpath):
            update_file_content(fpath, old_stem, new_stem, ext, old_guard, new_guard)
        else:
            print(f"  [skip]  {os.path.relpath(fpath, root)}  (not found)")

    # ── Step 2: Rename / move files ──────────────────────────────────────────
    print("\n── Step 2: Rename / move files ──────────────────────────────────────")
    for ext in (".c", ".h"):
        old_f = os.path.join(old_dir_path, old_stem + ext)
        new_f = os.path.join(new_dir_path, new_stem + ext)
        if os.path.isfile(old_f):
            os.makedirs(os.path.dirname(new_f), exist_ok=True)
            os.rename(old_f, new_f)
            print(f"  [moved]  src/{old_dir}/{old_stem}{ext}  →  src/{new_dir}/{new_stem}{ext}")
        else:
            print(f"  [skip]  {old_stem}{ext}  (not found)")

    # ── Step 3: Rename directory (MODE 1 only) ───────────────────────────────
    print("\n── Step 3: Rename directory ─────────────────────────────────────────")
    if mode == 1:
        os.rename(old_dir_path, new_dir_path)
        print(f"  [renamed]  src/{old_dir}/  →  src/{new_dir}/")
    else:
        print(f"  (skipped — {mode_labels[mode]})")

    # ── Step 4: Update #include across all src/ files ────────────────────────
    print("\n── Step 4: Update #include references in src/ ───────────────────────")
    old_inc = f'#include "{old_stem}.h"'
    new_inc = f'#include "{new_stem}.h"'
    changed = 0
    for dirpath, _, filenames in os.walk(src_root):
        for fname in filenames:
            if fname.endswith((".c", ".h")):
                if replace_if_changed(os.path.join(dirpath, fname), old_inc, new_inc):
                    changed += 1
    print(f"  {changed} file(s) updated." if changed else "  (no changes needed)")

    # ── Step 5: Update .cproject ─────────────────────────────────────────────
    print("\n── Step 5: Update .cproject ─────────────────────────────────────────")
    cproject = os.path.join(root, ".cproject")
    if same_dir:
        print("  (skipped — directory name unchanged)")
    elif mode == 3:
        # In MODE 3 the old directory is kept (other files still live there),
        # so its include-path entry in .cproject must NOT be removed/replaced.
        print("  (skipped — old directory still exists with other files)")
    elif os.path.isfile(cproject):
        if replace_if_changed(cproject, f"src/{old_dir}", f"src/{new_dir}"):
            print("  .cproject updated")
        else:
            print("  (no changes needed)")
    else:
        print("  [skip]  .cproject not found")

    # ── Step 6: Build artifacts ───────────────────────────────────────────────
    print("\n── Step 6: Build artifacts ──────────────────────────────────────────")
    build_src  = os.path.join(root, "PtxIotReaderApp_SPI_Debug", "src")
    stale_dir  = os.path.join(build_src, old_dir)
    sources_mk = os.path.join(root, "PtxIotReaderApp_SPI_Debug", "sources.mk")

    if mode == 1:
        # Delete old build subdir entirely (will be regenerated under new name)
        if os.path.isdir(stale_dir):
            shutil.rmtree(stale_dir)
            print(f"  [deleted]  PtxIotReaderApp_SPI_Debug/src/{old_dir}/")
        else:
            print(f"  (not found, skipping)  PtxIotReaderApp_SPI_Debug/src/{old_dir}/")
    elif mode == 3:
        # Delete only the specific .o / .d files for the moved source
        for ext in (".o", ".d", ".o.in"):
            artifact = os.path.join(stale_dir, old_stem + ext)
            if os.path.isfile(artifact):
                os.remove(artifact)
                print(f"  [deleted]  {os.path.relpath(artifact, root)}")
    else:
        print("  (no directory changes — skipping artifact cleanup)")

    # Update sources.mk
    if same_dir:
        print("  (sources.mk unchanged — directory name unchanged)")
    elif mode == 3:
        # Old directory still exists — keep its entry in sources.mk.
        print("  (sources.mk unchanged — old directory still exists with other files)")
    elif os.path.isfile(sources_mk):
        if replace_if_changed(sources_mk, f"src/{old_dir}", f"src/{new_dir}"):
            print("  sources.mk updated")
        else:
            print("  (no changes in sources.mk)")

    # ── Done ──────────────────────────────────────────────────────────────────
    print("\n✓ Done! In e2 studio: Project → Clean → Build\n")


if __name__ == "__main__":
    main()
