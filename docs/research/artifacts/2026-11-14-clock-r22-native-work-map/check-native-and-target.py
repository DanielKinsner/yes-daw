"""Verify and join the disjoint native helper and target source envelopes."""

from pathlib import Path
import runpy


HERE = Path(__file__).resolve().parent


def main():
    helper = runpy.run_path(str(HERE / "check.py"))
    target = runpy.run_path(str(HERE.parent / "2026-11-14-clock-r22-target-census/check.py"))
    if helper["MANIFEST_PIN"] != target["R22_PIN"]:
        raise ValueError("helper and target manifest identities differ")
    helper_prefix, helper_tail = helper["main"]()
    produce, verify = target["main"]()
    subtotal = helper_prefix + helper_tail + produce + verify
    if subtotal != 2_241_007_953_245:
        raise ValueError("native and target subtotal differs")
    print(f"PASS: {subtotal:,} conditional helper and target actions")
    print("Controller composition, runtime resources, and binding remain open.")


if __name__ == "__main__":
    main()
