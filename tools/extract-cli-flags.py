#!/usr/bin/env python3
"""Extracts the command-line flag table of the CrispASR CLI.

Reads examples/cli/cli.cpp and LICENSE of a CrispASR source snapshot and writes the C++
table that src/core/cli_flags.cpp includes. Rerun it when the engine version changes:

    python3 -I tools/extract-cli-flags.py \\
        ../Learn/sources/github.com/CrispStrobe/CrispASR/v0.8.42 \\
        src/core/cli_flags_table.inc --version v0.8.42 \\
        --commit 3be8bcb4390b8080dc73f807b7abb34dc0aca9c3

The script only reads the snapshot; it never builds or runs CrispASR.
"""

import argparse
import pathlib
import re
import sys

PARSER_FUNCTIONS = re.compile(r"^static bool (whisper_params_parse\w*)\(", re.MULTILINE)
BRANCH = re.compile(
    r'(?:\bif|\belse\s+if)\s*\(\s*(arg\s*==\s*"[^"]+"(?:\s*\|\|\s*arg\s*==\s*"[^"]+")*)\s*\)\s*\{')
SPELLING = re.compile(r'arg\s*==\s*"([^"]+)"')
ASSIGNMENT = re.compile(r"\bparams\.(\w+)\s*=(?!=)\s*([^;]*);")
CONVERSION = re.compile(r"std::sto(i|l|ll|ul|ull|f|d)\s*\(")
LITERAL = re.compile(r'^(true|false|-?\d+(\.\d+)?f?|"[^"]*")$')

KINDS = {"i": "kInt", "l": "kInt", "ll": "kInt", "ul": "kUnsigned", "ull": "kUnsigned", "f": "kFloat",
         "d": "kFloat"}

# Flags whose first assignment does not name what they set: --sensitivity applies a preset to
# four thresholds, and the two file flags append to a list instead of assigning.
PARAM_OVERRIDES = {"--sensitivity": "sensitivity", "--file": "fname_inp", "--output-file": "fname_out"}


def closing_brace(text, open_index):
    """Index of the brace that closes the one at open_index, skipping literals and comments."""
    depth = 0
    i = open_index
    while i < len(text):
        c = text[i]
        if c in "\"'":
            i += 1
            while i < len(text) and text[i] != c:
                i += 2 if text[i] == "\\" else 1
        elif text.startswith("//", i):
            i = text.index("\n", i)
        elif text.startswith("/*", i):
            i = text.index("*/", i) + 1
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
            if depth == 0:
                return i
        i += 1
    raise ValueError(f"unbalanced brace at offset {open_index}")


def function_bodies(text):
    for match in PARSER_FUNCTIONS.finditer(text):
        open_index = text.index("{", match.end())
        yield match.group(1), text[open_index + 1:closing_brace(text, open_index)]


def branches(body):
    position = 0
    while True:
        match = BRANCH.search(body, position)
        if not match:
            return
        open_index = match.end() - 1
        close_index = closing_brace(body, open_index)
        yield SPELLING.findall(match.group(1)), body[open_index + 1:close_index]
        position = close_index


def is_conditional(body, position):
    """True when the statement at position sits in a nested block or follows a brace-less if."""
    depth = 0
    i = 0
    while i < position:
        c = body[i]
        if c in "\"'":
            i += 1
            while i < position and body[i] != c:
                i += 2 if body[i] == "\\" else 1
        elif body.startswith("//", i):
            i = body.find("\n", i)
            if i < 0:
                break
        elif c == "{":
            depth += 1
        elif c == "}":
            depth -= 1
        i += 1
    statement_start = max(body.rfind(";", 0, position), body.rfind("{", 0, position), body.rfind("}", 0, position))
    return depth > 0 or re.search(r"\bif\s*\(", body[statement_start + 1:position]) is not None


def classify(spellings, body):
    matches = list(ASSIGNMENT.finditer(body))
    assignments = [(m.group(1), m.group(2)) for m in matches]
    takes_value = "ARGV_NEXT" in body or (re.search(r"\+\+i\b", body) and "argv[i]" in body)
    param = PARAM_OVERRIDES.get(spellings[-1], assignments[0][0] if assignments else "")
    implied = ""
    if takes_value:
        conversion = CONVERSION.search(body)
        if conversion:
            kind = KINDS[conversion.group(1)]
        elif "whisper_param_turn_lowercase(ARGV_NEXT)" in body:
            kind = "kLowercase"
        else:
            kind = "kString"
    elif assignments:
        kind = "kSwitch"
        implied = assignments[0][1].strip()
    elif "exit(" in body:
        kind = "kExit"
    else:
        raise ValueError(f"cannot classify {spellings}")
    also = []
    for match in matches[1:]:
        name, value = match.group(1), match.group(2).strip()
        if name != param and LITERAL.match(value):
            prefix = "may set " if is_conditional(body, match.start()) else ""
            also.append(f"{prefix}{name}={value}")
    return param, kind, implied, also


def license_comment(license_text):
    return "\n".join(("// " + line).rstrip() for line in license_text.strip().splitlines())


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("snapshot", type=pathlib.Path, help="root of the CrispASR source snapshot")
    parser.add_argument("output", type=pathlib.Path, help="the .inc file to write")
    parser.add_argument("--version", required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()

    source = (args.snapshot / "examples" / "cli" / "cli.cpp").read_text(encoding="utf-8")
    license_text = (args.snapshot / "LICENSE").read_text(encoding="utf-8")

    rows = []
    seen = {}
    for function, body in function_bodies(source):
        for spellings, branch_body in branches(body):
            spellings = [s for s in spellings if s != "-"]  # "-" reads stdin; it is not a flag
            if not spellings:
                continue
            for spelling in spellings:
                if spelling in seen:
                    raise ValueError(f"{spelling} appears in {seen[spelling]} and {function}")
                seen[spelling] = function
            rows.append((spellings, *classify(spellings, branch_body)))

    lines = [
        "// Generated by tools/extract-cli-flags.py from CrispASR " + args.version + ", examples/cli/cli.cpp",
        "// (commit " + args.commit + "). Do not edit by hand; rerun the script.",
        "//",
        "// One row per flag of the CrispASR command line, in source order: the spellings, the",
        "// whisper_params field the flag sets first, how its value is read, and the value a switch",
        "// assigns. Flag names and parameter names come from CrispASR, whose license follows.",
        "//",
        license_comment(license_text),
        "",
    ]
    for spellings, param, kind, implied, also in rows:
        row = f'{{"{" ".join(spellings)}", "{param}", CliValueKind::{kind}, "{implied}"}},'
        if also:
            row += "  // also: " + ", ".join(also)
        lines.append(row)
    args.output.write_text("\n".join(lines) + "\n", encoding="utf-8")

    kinds = {}
    for row in rows:
        kinds[row[2]] = kinds.get(row[2], 0) + 1
    print(f"{len(rows)} flags, {len(seen)} spellings: " +
          ", ".join(f"{k} {v}" for k, v in sorted(kinds.items())), file=sys.stderr)


if __name__ == "__main__":
    main()
