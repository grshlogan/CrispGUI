"""Checks the project's Markdown files: relative links and anchors, CRLF, trailing spaces, TODO markers.
Usage: python3 -I mdcheck.py <repo-root>"""
import os, re, subprocess, sys, unicodedata

root = sys.argv[1]
files = [f for f in subprocess.run(["git", "-C", root, "ls-files", "-z", "*.md"], capture_output=True, text=True, check=True).stdout.split("\0") if f]
files = [f for f in files if not f.startswith("reference/") or f == "reference/README.md"]

def slug(text):
    text = re.sub(r"`([^`]*)`", r"\1", text)
    text = re.sub(r"\[([^\]]*)\]\([^)]*\)", r"\1", text)
    out = []
    for ch in text.lower():
        cat = unicodedata.category(ch)
        if ch in " -_" or cat[0] in "LN":
            out.append(ch)
    return "".join(out).replace(" ", "-")

anchor_cache = {}
def anchors(path):
    if path not in anchor_cache:
        seen, result, fence = {}, set(), False
        with open(path, encoding="utf-8") as f:
            for line in f:
                if line.startswith("```"):
                    fence = not fence
                    continue
                m = None if fence else re.match(r"^(#{1,6})\s+(.*?)\s*#*\s*$", line)
                if m:
                    s = slug(m.group(2))
                    n = seen.get(s, 0)
                    seen[s] = n + 1
                    result.add(s if n == 0 else f"{s}-{n}")
        anchor_cache[path] = result
    return anchor_cache[path]

problems = 0
def report(msg):
    global problems
    problems += 1
    print(msg)

for rel in files:
    path = os.path.join(root, rel)
    data = open(path, "rb").read()
    if b"\r" in data:
        report(f"{rel}: CR characters")
    text = data.decode("utf-8")
    fence = False
    for no, line in enumerate(text.split("\n"), 1):
        if line.startswith("```"):
            fence = not fence
        if line.rstrip(" \t") != line:
            report(f"{rel}:{no}: trailing whitespace")
        if re.search(r"\b(TODO|TBD|FIXME)\b", line):
            report(f"{rel}:{no}: marker")
        if fence:
            continue
        for target in re.findall(r"\]\(([^)\s]+)\)", line):
            if re.match(r"^[a-z]+:", target):
                continue
            file_part, _, anchor = target.partition("#")
            dest = os.path.normpath(os.path.join(os.path.dirname(path), file_part)) if file_part else path
            if not os.path.exists(dest):
                report(f"{rel}:{no}: missing target {target}")
                continue
            if anchor and dest.endswith(".md") and anchor not in anchors(dest):
                report(f"{rel}:{no}: missing anchor {target}")
print(f"{len(files)} files, {problems} problems")
sys.exit(1 if problems else 0)
