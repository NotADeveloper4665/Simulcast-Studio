"""Diagnostic public symbol comparison, not a full ABI proof."""
import subprocess
import sys

def symbols(path):
    output = subprocess.check_output(["nm", "-D", "--defined-only", "--format=posix", path], text=True)
    return {line.split()[0].split("@")[0] for line in output.splitlines()
            if line.split() and line.split()[0].startswith(("obs_", "gs_", "video_", "audio_", "bfree", "bmalloc", "bzalloc", "blog"))}

stock, candidate = sys.argv[1:]
expected, actual = symbols(stock), symbols(candidate)
missing = sorted(expected - actual)
print(f"{len(expected)} stock symbols; {len(missing)} missing from {candidate}")
print("\n".join(missing))
