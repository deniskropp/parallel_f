#!/usr/bin/env python3
"""KickLang Parallel DAG → parallel_f C++ code generator.

Usage:
  python3 generate_cpp.py examples/advanced_dependency_example.kl
  python3 generate_cpp.py examples/*.kl --out-dir ../../test_kicklang_gen
  python3 generate_cpp.py --self-test
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from emit_cpp import default_output_name, emit_program, write_cpp
from kicklang_dag import ParseError, block_task_names, parse_kicklang


ROOT = Path(__file__).resolve().parent


def generate_file(
    source: Path,
    output: Path | None = None,
    out_dir: Path | None = None,
    dump_ir: bool = False,
) -> Path:
    text = source.read_text(encoding="utf-8")
    program = parse_kicklang(text, source_name=source.name)
    if dump_ir:
        print(_ir_summary(program), file=sys.stderr)

    cmd = program.generate[0] if program.generate else None
    headers = cmd.include_headers if cmd else ["parallel_f.hpp"]
    cpp = emit_program(program, headers=headers, source_label=str(source))

    if output is None:
        name = default_output_name(program, cmd)
        output = (out_dir or source.parent) / Path(name).name
    write_cpp(output, cpp)
    return output


def _ir_summary(program) -> str:
    blocks = []
    for b in program.blocks:
        blocks.append(
            {
                "name": b.name,
                "kind": b.kind,
                "tas": b.tas,
                "funcs": [f.name for f in b.funcs],
                "bindings": [x.name for x in b.bindings],
                "containers": [
                    {
                        "name": c.name,
                        "kind": c.kind,
                        "ops": [type(op).__name__ for op in c.ops],
                    }
                    for c in b.containers
                ],
                "tasks": block_task_names(b),
                "top_ops": [type(op).__name__ for op in b.ops],
            }
        )
    return json.dumps(
        {
            "source": program.source_name,
            "blocks": blocks,
            "generate": [g.output_file for g in program.generate],
        },
        indent=2,
    )


def self_test() -> int:
    examples = ROOT / "examples"
    cases = [
        ("advanced_dependency_example.kl", 17, "task_list"),
        ("parallel_vision_example.kl", 5, "task_list"),
        ("hello_queue.kl", 1, "task_queue"),
        ("speculative_fast_path.kl", 2, None),
    ]
    failed = 0
    for name, n_tasks, kind in cases:
        path = examples / name
        if not path.exists():
            print(f"FAIL {name}: missing example", file=sys.stderr)
            failed += 1
            continue
        try:
            program = parse_kicklang(path.read_text(encoding="utf-8"), source_name=name)
        except ParseError as exc:
            print(f"FAIL {name}: parse {exc}", file=sys.stderr)
            failed += 1
            continue
        if not program.blocks:
            print(f"FAIL {name}: no blocks", file=sys.stderr)
            failed += 1
            continue
        block = program.blocks[0]
        tasks = block_task_names(block)
        if len(tasks) < n_tasks:
            print(
                f"FAIL {name}: expected >= {n_tasks} tasks, got {tasks}",
                file=sys.stderr,
            )
            failed += 1
            continue
        if kind and not any(c.kind == kind for c in block.containers):
            print(f"FAIL {name}: missing container {kind}", file=sys.stderr)
            failed += 1
            continue
        cpp = emit_program(program, source_label=name)
        if "int main()" not in cpp or "parallel_f::" not in cpp:
            print(f"FAIL {name}: emitter produced no main/parallel_f", file=sys.stderr)
            failed += 1
            continue
        print(f"ok   {name}: {len(tasks)} tasks, {len(cpp)} bytes C++")
    return 1 if failed else 0


def main(argv: list[str] | None = None) -> int:
    p = argparse.ArgumentParser(
        description="Generate parallel_f C++ from KickLang Parallel DAG syntax"
    )
    p.add_argument("sources", nargs="*", type=Path, help="KickLang .kl files")
    p.add_argument("-o", "--output", type=Path, help="Output .cpp (single source only)")
    p.add_argument(
        "--out-dir",
        type=Path,
        help="Directory for generated .cpp files",
    )
    p.add_argument("--dump-ir", action="store_true", help="Print parsed IR as JSON")
    p.add_argument("--self-test", action="store_true", help="Parse bundled examples")
    args = p.parse_args(argv)

    if args.self_test:
        return self_test()

    if not args.sources:
        p.print_help()
        return 2

    if args.output and len(args.sources) != 1:
        print("error: -o/--output requires exactly one source file", file=sys.stderr)
        return 2

    for src in args.sources:
        if not src.is_file():
            print(f"error: not a file: {src}", file=sys.stderr)
            return 2
        try:
            out = generate_file(
                src,
                output=args.output,
                out_dir=args.out_dir,
                dump_ir=args.dump_ir,
            )
        except ParseError as exc:
            print(f"error: {src}: {exc}", file=sys.stderr)
            return 1
        print(f"wrote {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
