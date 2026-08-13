#!/usr/bin/env python3
"""KickLang v4.2 Parallel DAG parser.

Consumes the thin Parallel DAG subset used by parallel_f-kicklang-wrapper:
  make_task, task_list, task_queue, append(deps), push, finish, flush,
  exec, stats.show, join, cancel_task, ⫻block / ⫻flow/parallel /
  ⫻parallel_f_v2 / ⫻cmd/generate_cpp_from_dag.
"""

from __future__ import annotations

from dataclasses import dataclass, field
from enum import Enum
from typing import Any


# KickLang ⫻ sigil appears in the wild as U+2AFC and U+2AFB.
SIGIL = "\u2afc"
SIGILS = frozenset("\u2afc\u2afb")


class ParseError(Exception):
    def __init__(self, message: str, line: int = 0, col: int = 0) -> None:
        where = f"{line}:{col}" if line else "?"
        super().__init__(f"{where}: {message}")
        self.line = line
        self.col = col


class T(str, Enum):
    SIGIL = "sigil"
    IDENT = "ident"
    STRING = "string"
    NUMBER = "number"
    UNIT = "unit"
    LBRACE = "{"
    RBRACE = "}"
    LPAREN = "("
    RPAREN = ")"
    LBRACK = "["
    RBRACK = "]"
    EQ = "="
    COMMA = ","
    COLON = ":"
    DOT = "."
    COLONCOLON = "::"
    ARROW = "=>"
    EOF = "eof"


@dataclass(frozen=True)
class Token:
    kind: T
    value: Any
    line: int
    col: int


# ---------------------------------------------------------------------------
# IR
# ---------------------------------------------------------------------------


@dataclass
class MakeTask:
    callable: str | None
    args: list[Any] = field(default_factory=list)
    body: list[dict[str, Any]] = field(default_factory=list)


@dataclass
class AppendOp:
    name: str
    task: MakeTask | str
    deps: list[str] = field(default_factory=list)


@dataclass
class PushOp:
    name: str | None
    task: MakeTask | str


@dataclass
class FlushOp:
    bind: str | None = None


@dataclass
class FinishOp:
    target: str
    bind: str | None = None
    detached: bool = False


@dataclass
class ExecOp:
    target: str
    detached: bool = False


@dataclass
class RunOp:
    name: str


@dataclass
class JoinOp:
    tasks: list[str]
    options: dict[str, Any] = field(default_factory=dict)


@dataclass
class CancelOp:
    name: str
    reason: str = ""


@dataclass
class StatsOp:
    pass


@dataclass
class FuncDef:
    name: str
    params: list[str]
    body: list[dict[str, Any]]


@dataclass
class Binding:
    name: str
    value: Any


@dataclass
class Container:
    kind: str  # task_list | task_queue
    name: str
    ops: list[Any] = field(default_factory=list)


@dataclass
class Block:
    name: str
    kind: str  # block | flow | parallel_f_v2
    tas: dict[str, Any] = field(default_factory=dict)
    meta: dict[str, Any] = field(default_factory=dict)
    funcs: list[FuncDef] = field(default_factory=list)
    bindings: list[Binding] = field(default_factory=list)
    containers: list[Container] = field(default_factory=list)
    ops: list[Any] = field(default_factory=list)  # top-level ops (join, finish…)


@dataclass
class GenerateCmd:
    source: str | None = None
    output_file: str = "generated_parallel_dag.cpp"
    include_headers: list[str] = field(default_factory=lambda: ["parallel_f.hpp"])


@dataclass
class Program:
    source_name: str
    meta: dict[str, Any] = field(default_factory=dict)
    blocks: list[Block] = field(default_factory=list)
    generate: list[GenerateCmd] = field(default_factory=list)


# ---------------------------------------------------------------------------
# Lexer
# ---------------------------------------------------------------------------


_UNITS = {"ms", "s", "us", "ns"}


class Lexer:
    def __init__(self, text: str) -> None:
        self.text = text
        self.i = 0
        self.line = 1
        self.col = 1
        self.n = len(text)

    def _peek(self, k: int = 0) -> str:
        j = self.i + k
        return self.text[j] if j < self.n else ""

    def _adv(self, n: int = 1) -> str:
        out = []
        for _ in range(n):
            if self.i >= self.n:
                break
            ch = self.text[self.i]
            self.i += 1
            if ch == "\n":
                self.line += 1
                self.col = 1
            else:
                self.col += 1
            out.append(ch)
        return "".join(out)

    def tokens(self) -> list[Token]:
        out: list[Token] = []
        guard = 0
        limit = max(8, self.n * 8 + 32)
        while True:
            before = self.i
            tok = self._next()
            out.append(tok)
            if tok.kind is T.EOF:
                break
            if self.i <= before:
                raise ParseError(
                    f"lexer did not advance at {self.i} {self._peek()!r}",
                    tok.line,
                    tok.col,
                )
            guard += 1
            if guard > limit:
                raise ParseError(
                    f"lexer token limit ({limit}) at i={self.i} ch={self._peek()!r}",
                    tok.line,
                    tok.col,
                )
        return out

    def _next(self) -> Token:
        while True:
            ch = self._peek()
            if not ch:
                break
            if ch in " \t\r\n":
                self._adv()
                continue
            if ch == "#":
                self._adv()
                while self._peek() and self._peek() != "\n":
                    self._adv()
                continue
            if ch == "/" and self._peek(1) == "/":
                self._adv(2)
                while self._peek() and self._peek() != "\n":
                    self._adv()
                continue
            break

        line, col = self.line, self.col
        ch = self._peek()
        if not ch:
            return Token(T.EOF, None, line, col)

        if ch in SIGILS:
            self._adv()
            name = self._read_sigil_name()
            return Token(T.SIGIL, name, line, col)

        if ch == '"':
            return Token(T.STRING, self._read_string(), line, col)

        if ch.isdigit():
            return self._read_number(line, col)

        if ch.isalpha() or ch == "_":
            ident = self._read_ident()
            if ident in _UNITS:
                return Token(T.UNIT, ident, line, col)
            return Token(T.IDENT, ident, line, col)

        two = ch + self._peek(1)
        if two == "=>":
            self._adv(2)
            return Token(T.ARROW, two, line, col)
        if two == "::":
            self._adv(2)
            return Token(T.COLONCOLON, two, line, col)

        singles = {
            "{": T.LBRACE,
            "}": T.RBRACE,
            "(": T.LPAREN,
            ")": T.RPAREN,
            "[": T.LBRACK,
            "]": T.RBRACK,
            "=": T.EQ,
            ",": T.COMMA,
            ":": T.COLON,
            ".": T.DOT,
        }
        if ch in singles:
            self._adv()
            return Token(singles[ch], ch, line, col)

        raise ParseError(f"unexpected character {ch!r}", line, col)

    def _read_ident(self) -> str:
        s = []
        while True:
            ch = self._peek()
            if ch.isalnum() or ch == "_":
                s.append(self._adv())
            else:
                break
        return "".join(s)

    def _read_sigil_name(self) -> str:
        # block/name, kicklang:header, parallel_f_v2 (optional /name is later)
        s = []
        while True:
            ch = self._peek()
            if ch.isalnum() or ch in "_/:":
                s.append(self._adv())
            else:
                break
        return "".join(s).strip("/")

    def _read_string(self) -> str:
        self._adv()  # opening "
        s = []
        while True:
            ch = self._peek()
            if not ch:
                raise ParseError("unterminated string", self.line, self.col)
            if ch == '"':
                self._adv()
                return "".join(s)
            if ch == "\\" and self._peek(1):
                self._adv()
                esc = self._adv()
                s.append({"n": "\n", "t": "\t", '"': '"', "\\": "\\"}.get(esc, esc))
            else:
                s.append(self._adv())

    def _read_number(self, line: int, col: int) -> Token:
        s = []
        while self._peek().isdigit():
            s.append(self._adv())
        if self._peek() == "." and self._peek(1).isdigit():
            s.append(self._adv())
            while self._peek().isdigit():
                s.append(self._adv())
            return Token(T.NUMBER, float("".join(s)), line, col)
        return Token(T.NUMBER, int("".join(s)), line, col)


# ---------------------------------------------------------------------------
# Parser
# ---------------------------------------------------------------------------


class Parser:
    def __init__(self, tokens: list[Token], source_name: str = "<input>") -> None:
        self.toks = tokens
        self.i = 0
        self.source_name = source_name

    def _cur(self) -> Token:
        return self.toks[self.i]

    def _at(self, *kinds: T, value: Any = None) -> bool:
        tok = self._cur()
        if tok.kind not in kinds:
            return False
        if value is not None and tok.value != value:
            return False
        return True

    def _ident_is(self, *names: str) -> bool:
        tok = self._cur()
        return tok.kind is T.IDENT and tok.value in names

    def _eat(self, kind: T, value: Any = None) -> Token:
        tok = self._cur()
        if tok.kind is not kind or (value is not None and tok.value != value):
            want = f"{kind.value}{'' if value is None else ' ' + repr(value)}"
            raise ParseError(
                f"expected {want}, got {tok.kind.value} {tok.value!r}",
                tok.line,
                tok.col,
            )
        self.i += 1
        return tok

    def _accept(self, kind: T, value: Any = None) -> Token | None:
        if self._at(kind, value=value):
            tok = self._cur()
            self.i += 1
            return tok
        return None

    def parse(self) -> Program:
        prog = Program(source_name=self.source_name)
        while not self._at(T.EOF):
            if self._at(T.SIGIL):
                self._parse_sigil(prog)
            elif self._ident_is("meta"):
                self.i += 1
                self._eat(T.COLON)
                prog.meta = self._parse_object()
            else:
                # Bare statements at file scope become an implicit block.
                implicit = Block(name=prog.source_name, kind="block")
                self._parse_block_body(implicit, braced=False)
                if (
                    implicit.funcs
                    or implicit.containers
                    or implicit.ops
                    or implicit.bindings
                ):
                    prog.blocks.append(implicit)
                else:
                    tok = self._cur()
                    raise ParseError(
                        f"unexpected token {tok.kind.value} {tok.value!r}",
                        tok.line,
                        tok.col,
                    )
        return prog

    def _parse_sigil(self, prog: Program) -> None:
        tok = self._eat(T.SIGIL)
        name = tok.value or ""
        head, _, tail = name.partition("/")
        if head.split(":", 1)[0] == "kicklang":
            return
        if head == "end":
            return
        if head == "cmd" and tail == "generate_cpp_from_dag":
            prog.generate.append(self._parse_generate_cmd())
            return
        if head == "cmd":
            # ignore other commands (consume optional object)
            if self._at(T.LBRACE):
                self._parse_object()
            return
        if head in {"block", "flow", "parallel_f_v2"}:
            block_name = tail
            if not block_name and self._at(T.IDENT):
                # ⫻parallel_f_v2 /weaver_generated_harness
                if self._cur().value.startswith("/") or True:
                    ident = self._eat(T.IDENT)
                    block_name = ident.value.lstrip("/")
            if not block_name and self._at(T.SIGIL):
                block_name = head
            if not block_name:
                block_name = head
            block = Block(name=block_name, kind=head)
            if self._at(T.LBRACE):
                self._eat(T.LBRACE)
                self._parse_block_body(block, braced=True)
                self._eat(T.RBRACE)
            else:
                self._parse_block_body(block, braced=False)
            prog.blocks.append(block)
            return
        # Unknown sigil: skip optional payload.
        if self._at(T.LBRACE):
            self._parse_object()

    def _parse_generate_cmd(self) -> GenerateCmd:
        cmd = GenerateCmd()
        if not self._at(T.LBRACE):
            return cmd
        obj = self._parse_object()
        if "source" in obj:
            cmd.source = str(obj["source"])
        if "output_file" in obj:
            cmd.output_file = str(obj["output_file"])
        if "include_headers" in obj:
            raw = obj["include_headers"]
            if isinstance(raw, list):
                cmd.include_headers = [str(x) for x in raw]
            else:
                cmd.include_headers = [str(raw)]
        return cmd

    def _parse_block_body(self, block: Block, braced: bool) -> None:
        while not self._at(T.EOF):
            if braced and self._at(T.RBRACE):
                return
            if not braced and self._at(T.SIGIL):
                return
            self._parse_stmt(block)

    def _parse_stmt(self, block: Block) -> None:
        if self._ident_is("tas", "meta"):
            key = self._eat(T.IDENT).value
            self._eat(T.COLON)
            obj = self._parse_object() if self._at(T.LBRACE) else self._parse_value()
            if key == "tas" and isinstance(obj, dict):
                block.tas.update(obj)
            elif key == "meta" and isinstance(obj, dict):
                block.meta.update(obj)
            return

        if self._ident_is("func"):
            block.funcs.append(self._parse_func())
            return

        if self._ident_is("stats"):
            self._eat(T.IDENT)
            self._eat(T.DOT)
            method = self._eat(T.IDENT).value
            if self._at(T.LPAREN):
                self._eat(T.LPAREN)
                self._eat(T.RPAREN)
            if method == "show":
                block.ops.append(StatsOp())
            return

        if self._ident_is("join"):
            block.ops.append(self._parse_join())
            return

        if self._ident_is("cancel_task"):
            block.ops.append(self._parse_cancel())
            return

        if self._ident_is("coherence", "rta_mode", "seal", "on"):
            self._skip_decorative()
            return

        if not self._at(T.IDENT):
            tok = self._cur()
            raise ParseError(
                f"expected statement, got {tok.kind.value} {tok.value!r}",
                tok.line,
                tok.col,
            )

        name = self._eat(T.IDENT).value

        if self._at(T.COLONCOLON):
            self._eat(T.COLONCOLON)
            method = self._eat(T.IDENT).value
            if self._at(T.LPAREN):
                self._parse_call_args()
            if method == "run":
                block.ops.append(RunOp(name=name))
            return

        if self._at(T.DOT):
            self._eat(T.DOT)
            method = self._eat(T.IDENT).value
            detached = False
            if self._at(T.LPAREN):
                args = self._parse_call_args()
                if args and args[0] is True:
                    detached = True
            if method == "finish":
                block.ops.append(FinishOp(target=name, detached=detached))
            elif method == "flush":
                block.ops.append(FlushOp())
            elif method == "exec":
                block.ops.append(ExecOp(target=name, detached=detached))
            elif method == "show" and name == "stats":
                block.ops.append(StatsOp())
            return

        if self._at(T.COLON) and not self._at(T.COLONCOLON):
            # decorative `coherence: a | b` style line
            self._skip_decorative()
            return

        self._eat(T.EQ)

        if self._ident_is("task_list", "task_queue"):
            kind = self._eat(T.IDENT).value
            self._eat(T.LBRACE)
            container = Container(kind=kind, name=name)
            self._parse_container_body(container)
            self._eat(T.RBRACE)
            block.containers.append(container)
            return

        if self._ident_is("append"):
            block.ops.append(self._parse_append(name))
            return

        if self._ident_is("push"):
            block.ops.append(self._parse_push(name))
            return

        if self._ident_is("make_task"):
            block.ops.append(AppendOp(name=name, task=self._parse_make_task()))
            return

        if self._ident_is("flush"):
            self._eat(T.IDENT)
            if self._at(T.LPAREN):
                self._eat(T.LPAREN)
                self._eat(T.RPAREN)
            block.ops.append(FlushOp(bind=name))
            return

        # result = dag.finish()
        if self._at(T.IDENT) and self.toks[self.i + 1].kind is T.DOT:
            target = self._eat(T.IDENT).value
            self._eat(T.DOT)
            method = self._eat(T.IDENT).value
            detached = False
            if self._at(T.LPAREN):
                args = self._parse_call_args()
                if args and args[0] is True:
                    detached = True
            if method == "finish":
                block.ops.append(FinishOp(target=target, bind=name, detached=detached))
            elif method == "flush":
                block.ops.append(FlushOp(bind=name))
            elif method == "exec":
                block.ops.append(ExecOp(target=target, detached=detached))
            return

        block.bindings.append(Binding(name=name, value=self._parse_value()))

    def _parse_container_body(self, container: Container) -> None:
        while not self._at(T.RBRACE, T.EOF):
            if self._ident_is("flush"):
                self._eat(T.IDENT)
                bind = None
                if self._at(T.LPAREN):
                    self._eat(T.LPAREN)
                    self._eat(T.RPAREN)
                container.ops.append(FlushOp(bind=bind))
                continue
            if self._ident_is("append"):
                container.ops.append(self._parse_append(name=f"t{len(container.ops)}"))
                continue
            if self._ident_is("push"):
                container.ops.append(self._parse_push(name=None))
                continue
            if not self._at(T.IDENT):
                tok = self._cur()
                raise ParseError(
                    f"expected container item, got {tok.kind.value} {tok.value!r}",
                    tok.line,
                    tok.col,
                )
            name = self._eat(T.IDENT).value
            self._eat(T.EQ)
            if self._ident_is("append"):
                container.ops.append(self._parse_append(name))
            elif self._ident_is("push"):
                container.ops.append(self._parse_push(name))
            elif self._ident_is("make_task"):
                container.ops.append(AppendOp(name=name, task=self._parse_make_task()))
            elif self._ident_is("flush"):
                self._eat(T.IDENT)
                if self._at(T.LPAREN):
                    self._eat(T.LPAREN)
                    self._eat(T.RPAREN)
                container.ops.append(FlushOp(bind=name))
            else:
                raise ParseError(
                    "expected append/push/make_task inside container",
                    self._cur().line,
                    self._cur().col,
                )

    def _parse_append(self, name: str) -> AppendOp:
        self._eat(T.IDENT)  # append
        self._eat(T.LPAREN)
        task: MakeTask | str
        if self._ident_is("make_task"):
            task = self._parse_make_task()
        else:
            task = str(self._parse_value())
        deps: list[str] = []
        if self._accept(T.COMMA):
            if self._ident_is("deps"):
                self._eat(T.IDENT)
                self._eat(T.COLON)
                deps = self._parse_ident_list()
            else:
                # append(task, dep1, dep2) positional form
                while not self._at(T.RPAREN):
                    deps.append(str(self._parse_value()))
                    if not self._accept(T.COMMA):
                        break
        self._eat(T.RPAREN)
        return AppendOp(name=name, task=task, deps=deps)

    def _parse_push(self, name: str | None) -> PushOp:
        self._eat(T.IDENT)  # push
        self._eat(T.LPAREN)
        if self._ident_is("make_task"):
            task: MakeTask | str = self._parse_make_task()
        else:
            task = str(self._parse_value())
        self._eat(T.RPAREN)
        return PushOp(name=name, task=task)

    def _parse_make_task(self) -> MakeTask:
        self._eat(T.IDENT)  # make_task
        if self._at(T.LBRACE):
            self._eat(T.LBRACE)
            body = self._parse_func_body()
            self._eat(T.RBRACE)
            call = None
            if body and body[0].get("op") == "call":
                call = body[0].get("name")
            return MakeTask(callable=call, body=body)
        self._eat(T.LPAREN)
        callable_name = None
        args: list[Any] = []
        if not self._at(T.RPAREN):
            first = self._parse_value()
            if isinstance(first, str):
                callable_name = first
            else:
                args.append(first)
            while self._accept(T.COMMA):
                args.append(self._parse_value())
        self._eat(T.RPAREN)
        return MakeTask(callable=callable_name, args=args)

    def _parse_func(self) -> FuncDef:
        self._eat(T.IDENT)  # func
        name = self._eat(T.IDENT).value
        self._eat(T.EQ)
        self._eat(T.LPAREN)
        params: list[str] = []
        if not self._at(T.RPAREN):
            params.append(self._eat(T.IDENT).value)
            while self._accept(T.COMMA):
                params.append(self._eat(T.IDENT).value)
        self._eat(T.RPAREN)
        self._eat(T.ARROW)
        self._eat(T.LBRACE)
        body = self._parse_func_body()
        self._eat(T.RBRACE)
        return FuncDef(name=name, params=params, body=body)

    def _parse_func_body(self) -> list[dict[str, Any]]:
        body: list[dict[str, Any]] = []
        while not self._at(T.RBRACE, T.EOF):
            if self._ident_is("log"):
                self._eat(T.IDENT)
                msg = self._eat(T.STRING).value
                args: list[Any] = []
                for _ in range(_format_argc(msg)):
                    if self._at(T.IDENT, T.STRING, T.NUMBER):
                        args.append(self._parse_value())
                    else:
                        break
                body.append({"op": "log", "msg": msg, "args": args})
            elif self._ident_is("sleep"):
                self._eat(T.IDENT)
                ms = self._eat(T.NUMBER).value
                unit = "ms"
                if self._at(T.UNIT):
                    unit = self._eat(T.UNIT).value
                elif self._ident_is("ms", "s", "us", "ns"):
                    unit = self._eat(T.IDENT).value
                if unit == "s":
                    ms = int(float(ms) * 1000)
                body.append({"op": "sleep", "ms": int(ms)})
            elif self._at(T.IDENT):
                name = self._eat(T.IDENT).value
                args = self._parse_call_args() if self._at(T.LPAREN) else []
                body.append({"op": "call", "name": name, "args": args})
            else:
                tok = self._cur()
                raise ParseError(
                    f"unexpected function body token {tok.kind.value} {tok.value!r}",
                    tok.line,
                    tok.col,
                )
        return body

    def _parse_join(self) -> JoinOp:
        self._eat(T.IDENT)
        self._eat(T.LPAREN)
        tasks = self._parse_ident_list()
        options: dict[str, Any] = {}
        if self._accept(T.COMMA):
            if self._at(T.LBRACE):
                options = self._parse_object()
        self._eat(T.RPAREN)
        return JoinOp(tasks=tasks, options=options)

    def _parse_cancel(self) -> CancelOp:
        self._eat(T.IDENT)
        self._eat(T.LPAREN)
        name = str(self._parse_value())
        reason = ""
        if self._accept(T.COMMA):
            reason = str(self._parse_value())
        self._eat(T.RPAREN)
        return CancelOp(name=name, reason=reason)

    def _parse_ident_list(self) -> list[str]:
        self._eat(T.LBRACK)
        names: list[str] = []
        if not self._at(T.RBRACK):
            names.append(str(self._parse_value()))
            while self._accept(T.COMMA):
                names.append(str(self._parse_value()))
        self._eat(T.RBRACK)
        return names

    def _parse_call_args(self) -> list[Any]:
        self._eat(T.LPAREN)
        args: list[Any] = []
        if not self._at(T.RPAREN):
            args.append(self._parse_value())
            while self._accept(T.COMMA):
                args.append(self._parse_value())
        self._eat(T.RPAREN)
        return args

    def _parse_object(self) -> dict[str, Any]:
        self._eat(T.LBRACE)
        obj: dict[str, Any] = {}
        while not self._at(T.RBRACE, T.EOF):
            if not self._at(T.IDENT, T.STRING):
                # skip decorative tokens like | in `coherence: a | b`
                if self._at(T.IDENT) or self._accept(T.COMMA):
                    continue
                # try key: value
                tok = self._cur()
                raise ParseError(
                    f"expected object key, got {tok.kind.value} {tok.value!r}",
                    tok.line,
                    tok.col,
                )
            key_tok = self._cur()
            key = str(key_tok.value)
            self.i += 1
            if self._accept(T.COLON):
                obj[key] = self._parse_value()
            else:
                obj[key] = True
            self._accept(T.COMMA)
        self._eat(T.RBRACE)
        return obj

    def _parse_value(self) -> Any:
        tok = self._cur()
        if tok.kind is T.STRING:
            self.i += 1
            return tok.value
        if tok.kind is T.NUMBER:
            self.i += 1
            if self._at(T.UNIT):
                unit = self._eat(T.UNIT).value
                return (tok.value, unit)
            return tok.value
        if tok.kind is T.IDENT:
            self.i += 1
            if tok.value == "true":
                return True
            if tok.value == "false":
                return False
            if tok.value == "null":
                return None
            return tok.value
        if tok.kind is T.LBRACK:
            self.i += 1
            items: list[Any] = []
            if not self._at(T.RBRACK):
                items.append(self._parse_value())
                while self._accept(T.COMMA):
                    items.append(self._parse_value())
            self._eat(T.RBRACK)
            return items
        if tok.kind is T.LBRACE:
            return self._parse_object()
        raise ParseError(
            f"expected value, got {tok.kind.value} {tok.value!r}", tok.line, tok.col
        )

    def _skip_decorative(self) -> None:
        """Skip `on ... :`, `coherence: a | b`, `seal: X { ... }`."""
        # consume rest of a "statement" until we hit a likely next statement start
        depth = 0
        started = False
        while not self._at(T.EOF):
            tok = self._cur()
            if tok.kind is T.LBRACE:
                depth += 1
                started = True
                self.i += 1
                continue
            if tok.kind is T.RBRACE:
                if depth:
                    depth -= 1
                    self.i += 1
                    if depth == 0:
                        return
                    continue
                return
            if depth == 0 and started:
                return
            if (
                depth == 0
                and started is False
                and tok.kind is T.SIGIL
            ):
                return
            # newline-less stream: stop before a clear new statement
            nxt = self.toks[self.i + 1] if self.i + 1 < len(self.toks) else None
            if (
                depth == 0
                and tok.kind is T.IDENT
                and nxt is not None
                and nxt.kind in {T.EQ, T.DOT, T.COLONCOLON}
                and started
            ):
                return
            self.i += 1
            started = True
            # stop at likely statement boundary after consuming a simple value list
            if depth == 0 and tok.kind in {T.IDENT, T.STRING, T.NUMBER} and self._at(
                T.IDENT, T.SIGIL, T.RBRACE
            ):
                # if next looks like a new statement keyword
                if self._ident_is(
                    "func",
                    "tas",
                    "meta",
                    "join",
                    "stats",
                    "cancel_task",
                    "coherence",
                    "rta_mode",
                    "seal",
                    "on",
                ) or self._at(T.SIGIL, T.RBRACE):
                    return


def _format_argc(msg: str) -> int:
    """Count printf-style conversions in a log format string."""
    n = 0
    i = 0
    while i < len(msg):
        if msg[i] == "%":
            if i + 1 < len(msg) and msg[i + 1] == "%":
                i += 2
                continue
            n += 1
        i += 1
    return n


def parse_kicklang(text: str, source_name: str = "<input>") -> Program:
    tokens = Lexer(text).tokens()
    return Parser(tokens, source_name=source_name).parse()


def block_task_names(block: Block) -> list[str]:
    names: list[str] = []
    for c in block.containers:
        for op in c.ops:
            if isinstance(op, (AppendOp, PushOp)) and op.name:
                names.append(op.name)
    for op in block.ops:
        if isinstance(op, (AppendOp, PushOp, RunOp)) and getattr(op, "name", None):
            names.append(op.name)
    return names
