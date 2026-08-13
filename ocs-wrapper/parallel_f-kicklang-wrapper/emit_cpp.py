#!/usr/bin/env python3
"""Emit compilable parallel_f C++ from a parsed KickLang Parallel DAG."""

from __future__ import annotations

import re
from pathlib import Path
from typing import Any, Iterable

from kicklang_dag import (
    AppendOp,
    Binding,
    Block,
    CancelOp,
    Container,
    ExecOp,
    FinishOp,
    FlushOp,
    FuncDef,
    GenerateCmd,
    JoinOp,
    MakeTask,
    Program,
    PushOp,
    RunOp,
    StatsOp,
)


def cpp_ident(name: str) -> str:
    ident = re.sub(r"[^A-Za-z0-9_]", "_", name)
    if not ident or ident[0].isdigit():
        ident = "n_" + ident
    return ident


def _lit(value: Any) -> str:
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, int):
        return str(value)
    if isinstance(value, float):
        return str(value)
    if isinstance(value, tuple) and len(value) == 2:
        return str(value[0])
    if value is None:
        return "0"
    text = str(value)
    return f'std::string("{_escape(text)}")'


def _escape(text: str) -> str:
    return (
        text.replace("\\", "\\\\")
        .replace('"', '\\"')
        .replace("\n", "\\n")
        .replace("\t", "\\t")
    )


def _is_stringy(value: Any) -> bool:
    return isinstance(value, str) or (
        isinstance(value, tuple) and isinstance(value[0], str)
    )


def emit_func_body(body: list[dict[str, Any]], indent: str = "    ") -> list[str]:
    lines: list[str] = []
    for stmt in body:
        op = stmt.get("op")
        if op == "log":
            msg = stmt.get("msg", "")
            args = stmt.get("args") or []
            if not msg.endswith("\n"):
                msg = msg + "\n"
            if args:
                arg_s = ", ".join(_log_arg(a) for a in args)
                lines.append(
                    f'{indent}parallel_f::log_info("{_escape(msg)}", {arg_s});'
                )
            else:
                lines.append(f'{indent}parallel_f::log_info("{_escape(msg)}");')
        elif op == "sleep":
            ms = int(stmt.get("ms") or 0)
            lines.append(
                f"{indent}std::this_thread::sleep_for(std::chrono::milliseconds({ms}));"
            )
        elif op == "call":
            name = stmt.get("name") or "work"
            args = stmt.get("args") or []
            if args:
                shown = ", ".join(_fmt_preview(a) for a in args)
                lines.append(
                    f'{indent}parallel_f::log_info("{_escape(name)}({_escape(shown)})\\n");'
                )
            else:
                lines.append(
                    f'{indent}parallel_f::log_info("{_escape(name)}()\\n");'
                )
            lines.append(
                f"{indent}std::this_thread::sleep_for(std::chrono::milliseconds(50));"
            )
    if not lines:
        lines.append(f'{indent}parallel_f::log_info("task\\n");')
    return lines


def _log_arg(value: Any) -> str:
    if isinstance(value, str) and re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", value):
        # identifier — assume std::string-like unless it looks numeric
        return f"{cpp_ident(value)}.c_str()"
    if isinstance(value, str):
        return f'"{_escape(value)}"'
    return _lit(value)


def _fmt_preview(value: Any) -> str:
    return str(value)


def _callable_params(fn: FuncDef | None, args: list[Any]) -> str:
    if fn and fn.params:
        parts = []
        for i, p in enumerate(fn.params):
            sample = args[i] if i < len(args) else None
            if sample is None or _is_stringy(sample) or isinstance(sample, str):
                parts.append(f"auto {cpp_ident(p)}")
            elif isinstance(sample, (int, float)):
                parts.append(f"auto {cpp_ident(p)}")
            else:
                parts.append(f"auto {cpp_ident(p)}")
        return ", ".join(parts)
    if not args:
        return ""
    names = []
    for i, a in enumerate(args):
        names.append(f"auto a{i}" if not isinstance(a, str) else f"auto {cpp_ident(str(a))}")
    # args to make_task are values, not param names — use a0, a1
    return ", ".join(f"auto a{i}" for i in range(len(args)))


def _make_task_expr(
    task: MakeTask | str,
    declared: set[str],
    inline_name: str | None = None,
) -> str:
    if isinstance(task, str):
        return cpp_ident(task)
    args = ", ".join(_task_arg(a, declared) for a in task.args)
    if task.callable and not task.body:
        call = cpp_ident(task.callable)
        return f"parallel_f::make_task({call}{', ' + args if args else ''})"
    # inline body / unnamed callable
    return None  # type: ignore[return-value]


def _task_arg(value: Any, declared: set[str]) -> str:
    if isinstance(value, str) and value in declared:
        return cpp_ident(value)
    if isinstance(value, str) and re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", value):
        return cpp_ident(value)
    if isinstance(value, str):
        return f'std::string("{_escape(value)}")'
    return _lit(value)


def _collect_callables(block: Block) -> tuple[set[str], dict[str, list[Any]]]:
    known = {f.name for f in block.funcs}
    arities: dict[str, list[Any]] = {}

    def walk_task(task: MakeTask | str) -> None:
        if isinstance(task, MakeTask) and task.callable and task.callable not in known:
            arities.setdefault(task.callable, task.args)

    for c in block.containers:
        for op in c.ops:
            if isinstance(op, (AppendOp, PushOp)):
                walk_task(op.task)
    for op in block.ops:
        if isinstance(op, (AppendOp, PushOp)):
            walk_task(op.task)
    return known, arities


def _collect_free_bindings(block: Block) -> list[str]:
    bound = {b.name for b in block.bindings} | {f.name for f in block.funcs}
    free: list[str] = []

    def consider(value: Any) -> None:
        if (
            isinstance(value, str)
            and re.match(r"^[A-Za-z_][A-Za-z0-9_]*$", value)
            and value not in bound
            and value not in free
            and value not in {"true", "false", "null"}
        ):
            free.append(value)

    for c in block.containers:
        for op in c.ops:
            if isinstance(op, (AppendOp, PushOp)) and isinstance(op.task, MakeTask):
                for a in op.task.args:
                    consider(a)
    for op in block.ops:
        if isinstance(op, (AppendOp, PushOp)) and isinstance(op.task, MakeTask):
            for a in op.task.args:
                consider(a)
    return free


def emit_program(
    program: Program,
    *,
    headers: Iterable[str] | None = None,
    source_label: str | None = None,
) -> str:
    headers = list(headers or ["parallel_f.hpp"])
    blocks = program.blocks or [
        Block(name=program.source_name, kind="block"),
    ]
    label = source_label or program.source_name
    lines: list[str] = []
    lines.append(
        f"// Generated by parallel_f-kicklang-wrapper from {label}"
    )
    lines.append("// TAS-traced KickLang Parallel DAG → parallel_f C++")
    lines.append("// Regenerate: python3 ocs-wrapper/parallel_f-kicklang-wrapper/generate_cpp.py <source.kl>")
    lines.append("")
    for h in headers:
        if h.startswith("<") or h.startswith('"'):
            lines.append(f"#include {h}")
        else:
            lines.append(f'#include "{h}"')
    lines.append("")
    lines.append("#include <chrono>")
    lines.append("#include <string>")
    lines.append("#include <thread>")
    lines.append("")
    lines.append("int main() {")
    lines.append("  parallel_f::set_debug_level(0);")
    lines.append("  parallel_f::system::instance().set_auto_flush(")
    lines.append("      parallel_f::system::AutoFlush::EndOfLine);")
    lines.append("")

    for i, block in enumerate(blocks):
        if i:
            lines.append("")
        lines.extend(_emit_block(block))

    # default finish/stats if the program never asked
    if not _has_terminal(blocks):
        # if any task_list exists, finish the last one
        last_list = None
        last_queue = None
        for b in blocks:
            for c in b.containers:
                if c.kind == "task_list":
                    last_list = c.name
                elif c.kind == "task_queue":
                    last_queue = c.name
        if last_list:
            lines.append(f"  {cpp_ident(last_list)}.finish();")
        elif last_queue:
            lines.append(f"  {cpp_ident(last_queue)}.exec();")
        lines.append("  parallel_f::stats::instance::get().show_stats();")

    lines.append("  return 0;")
    lines.append("}")
    lines.append("")
    return "\n".join(lines)


def _has_terminal(blocks: list[Block]) -> bool:
    for b in blocks:
        for op in b.ops:
            if isinstance(op, (FinishOp, ExecOp, StatsOp, JoinOp)):
                return True
    return False


def _emit_block(block: Block) -> list[str]:
    lines: list[str] = []
    tas_id = block.tas.get("id", block.name)
    purpose = block.tas.get("purpose", block.meta.get("purpose", ""))
    lines.append(f"  // --- ⫻{block.kind}/{block.name} ---")
    lines.append(f"  // TAS id={tas_id}" + (f' purpose="{purpose}"' if purpose else ""))

    declared: set[str] = set()

    for fn in block.funcs:
        lines.extend(_emit_func(fn))
        declared.add(fn.name)

    known, stubs = _collect_callables(block)
    for name, args in stubs.items():
        if name in known:
            continue
        params = ", ".join(f"auto a{i}" for i in range(len(args)))
        lines.append(f"  auto {cpp_ident(name)} = []({params}) {{")
        for i in range(len(args)):
            lines.append(f"    (void)a{i};")
        lines.append(f'    parallel_f::log_info("{_escape(name)}()\\n");')
        lines.append(
            "    std::this_thread::sleep_for(std::chrono::milliseconds(50));"
        )
        lines.append("  };")
        declared.add(name)

    for name in _collect_free_bindings(block):
        if name in declared or any(f.name == name for f in block.funcs):
            continue
        # skip names that are task nodes
        if name in {op.name for c in block.containers for op in c.ops if getattr(op, "name", None)}:
            continue
        lines.append(f'  auto {cpp_ident(name)} = std::string("{_escape(name)}");')
        declared.add(name)

    for b in block.bindings:
        lines.append(f"  auto {cpp_ident(b.name)} = {_lit(b.value)};")
        declared.add(b.name)

    # v2-style free make_task assignments live in block.ops as AppendOp
    implicit_tasks = [op for op in block.ops if isinstance(op, (AppendOp, PushOp))]
    if implicit_tasks and not block.containers:
        synth = Container(kind="task_list", name="dag", ops=list(implicit_tasks))
        # keep other ops (run/join/finish)
        lines.extend(_emit_container(synth, block, declared))
    else:
        if implicit_tasks and block.containers:
            # merge implicit appends into first matching container later — emit as tasks first
            pass
        for c in block.containers:
            lines.extend(_emit_container(c, block, declared))

    for op in block.ops:
        if isinstance(op, (AppendOp, PushOp)):
            continue  # already emitted via container or implicit dag
        lines.extend(_emit_top_op(op, block, declared))

    return lines


def _emit_func(fn: FuncDef) -> list[str]:
    params = ", ".join(f"auto {cpp_ident(p)}" for p in fn.params)
    lines = [f"  auto {cpp_ident(fn.name)} = []({params}) {{"]
    lines.extend(emit_func_body(fn.body, indent="    "))
    lines.append("  };")
    return lines


def _emit_make_task(
    task: MakeTask | str, declared: set[str], tmp_name: str
) -> tuple[list[str], str]:
    """Return (prelude_lines, expression)."""
    if isinstance(task, str):
        return [], cpp_ident(task)
    if task.body and (not task.callable or task.body[0].get("op") != "call" or len(task.body) > 1):
        lines = [f"  auto {tmp_name} = parallel_f::make_task([]() {{"]
        lines.extend(emit_func_body(task.body, indent="    "))
        lines.append("  });")
        return lines, tmp_name
    if task.body and task.callable:
        # make_task { compute_fast_path() } → named stub already emitted? use lambda call
        args = ", ".join(_task_arg(a, declared) for a in task.args)
        extra = f", {args}" if args else ""
        if task.callable in declared:
            return [], f"parallel_f::make_task({cpp_ident(task.callable)}{extra})"
        lines = [f"  auto {tmp_name} = parallel_f::make_task([]() {{"]
        lines.extend(emit_func_body(task.body, indent="    "))
        lines.append("  });")
        return lines, tmp_name
    args = ", ".join(_task_arg(a, declared) for a in task.args)
    call = cpp_ident(task.callable or tmp_name)
    extra = f", {args}" if args else ""
    return [], f"parallel_f::make_task({call}{extra})"


def _emit_container(container: Container, block: Block, declared: set[str]) -> list[str]:
    lines: list[str] = []
    cname = cpp_ident(container.name)
    if container.kind == "task_queue":
        lines.append(f"  parallel_f::task_queue {cname};")
    else:
        lines.append(f"  parallel_f::task_list {cname};")
    declared.add(container.name)

    id_of: dict[str, str] = {}

    for i, op in enumerate(container.ops):
        if isinstance(op, FlushOp):
            if op.bind:
                lines.append(f"  auto {cpp_ident(op.bind)} = {cname}.flush();")
                id_of[op.bind] = cpp_ident(op.bind)
                declared.add(op.bind)
            else:
                lines.append(f"  {cname}.flush();")
            continue

        if isinstance(op, PushOp):
            tname = cpp_ident(op.name or f"qtask_{i}")
            prelude, expr = _emit_make_task(op.task, declared, tname)
            if prelude:
                lines.extend(prelude)
                lines.append(f"  {cname}.push({tname});")
            else:
                lines.append(f"  auto {tname} = {expr};")
                lines.append(f"  {cname}.push({tname});")
            if op.name:
                declared.add(op.name)
            continue

        if isinstance(op, AppendOp):
            tname = cpp_ident(op.name)
            idname = f"{tname}_id"
            prelude, expr = _emit_make_task(op.task, declared, tname)
            tas_note = _tas_note(block, op)
            if tas_note:
                lines.append(f"  {tas_note}")
            if prelude:
                lines.extend(prelude)
                task_expr = tname
            else:
                lines.append(f"  auto {tname} = {expr};")
                task_expr = tname
            deps = []
            for d in op.deps:
                if d in id_of:
                    deps.append(id_of[d])
                else:
                    deps.append(f"{cpp_ident(d)}_id")
            dep_s = (", " + ", ".join(deps)) if deps else ""
            lines.append(f"  auto {idname} = {cname}.append({task_expr}{dep_s});")
            id_of[op.name] = idname
            declared.add(op.name)
            continue

    return lines


def _tas_note(block: Block, op: AppendOp) -> str:
    nid = block.tas.get("id", block.name)
    deps = f" deps=[{', '.join(op.deps)}]" if op.deps else " deps=[]"
    return f"// TAS {nid} node={op.name}{deps}"


def _emit_top_op(op: Any, block: Block, declared: set[str]) -> list[str]:
    lines: list[str] = []
    if isinstance(op, FinishOp):
        target = cpp_ident(op.target)
        call = f"{target}.finish({'true' if op.detached else ''})"
        if op.bind:
            lines.append(f"  auto {cpp_ident(op.bind)} = {call};")
        else:
            lines.append(f"  {call};")
    elif isinstance(op, ExecOp):
        extra = "true" if op.detached else ""
        lines.append(f"  {cpp_ident(op.target)}.exec({extra});")
    elif isinstance(op, FlushOp):
        if op.bind:
            lines.append(f"  auto {cpp_ident(op.bind)} = dag.flush();")
        else:
            # flush last task_list if any
            target = block.containers[-1].name if block.containers else "dag"
            lines.append(f"  {cpp_ident(target)}.flush();")
    elif isinstance(op, StatsOp):
        lines.append("  parallel_f::stats::instance::get().show_stats();")
    elif isinstance(op, RunOp):
        lines.append(f"  // {op.name}::run() — scheduled via task_list.append")
    elif isinstance(op, JoinOp):
        q = op.options.get("quorum")
        pred = op.options.get("predicate")
        extra = []
        if q is not None:
            extra.append(f"quorum={q}")
        if pred is not None:
            extra.append(f"predicate={pred}")
        note = (" // " + ", ".join(str(x) for x in extra)) if extra else ""
        # parallel_f join == task_list.finish()
        target = block.containers[-1].name if block.containers else "dag"
        lines.append(f"  // join([{', '.join(op.tasks)}]){note}")
        lines.append(f"  {cpp_ident(target)}.finish();")
    elif isinstance(op, CancelOp):
        lines.append(
            f'  // cancel_task({op.name}, "{_escape(op.reason)}")'
            " — parallel_f has no speculative cancel; task remains in the DAG"
        )
    return lines


def default_output_name(program: Program, cmd: GenerateCmd | None = None) -> str:
    if cmd and cmd.output_file:
        return cmd.output_file
    if program.generate:
        return program.generate[0].output_file
    if program.blocks:
        return f"generated_{cpp_ident(program.blocks[0].name)}.cpp"
    return "generated_parallel_dag.cpp"


def write_cpp(path: Path, source: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(source, encoding="utf-8")
