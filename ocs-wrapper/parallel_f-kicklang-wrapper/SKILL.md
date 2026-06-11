---
name: parallel_f-kicklang-wrapper
description: Thin OCS-compliant wrapper that consumes KickLang v4.2 Parallel DAG syntax (make_task, task_list, append with deps, finish, flush, stats) and delegates execution to the parallel_f C++ library. Provides TAS metadata attachment, CoherenceMonitorBridge integration, and consent-gated execution.
version: 0.1.0-prototype
author: OCS Meta-Orchestrator (generated from kicklang-evolution-predictor + kicklang-meta-playbook)
tags: [kicklang, parallel_dag, parallel_f, tas, ocs, execution, wrapper]
activation_triggers:
  - parallel_f wrapper
  - kicklang parallel dag execute
  - ocs parallel execution
  - consume parallel_dag syntax
---

# parallel_f-kicklang-wrapper

Thin execution bridge between KickLang v4.2 Parallel DAG syntax and the mature `parallel_f` C++ task-parallel library.

## Overview

This skill acts as a **translation + delegation layer**:
- Parses KickLang `task_list` / `make_task` / `⫻flow/parallel` blocks.
- Builds an internal DAG representation.
- Delegates to `parallel_f` (initially via planned Python bindings / code generation / or direct C++ bridge).
- Returns execution results + structured stats that feed directly into `CoherenceMonitorBridge`.
- Attaches full TAS metadata to every task for traceability across OCS workflows.

It is intentionally **thin** — heavy lifting (scheduling, vthread management, dependency resolution) stays in `parallel_f`.

## When to Activate

- User or higher-level skill emits a `task_list` or `⫻flow/parallel` block.
- Need for high-performance parallel execution of TAS graphs (vision pipelines, multi-agent swarms, RTA traversals, thumbnailer-style workloads).
- Desire to reuse existing `parallel_f` investment while gaining KickLang-native syntax and OCS observability.

## Core OCS Invariants (Enforced)

- Full TAS lifecycle support (extraction, purification, sequencing).
- Three-agent-core delegation for non-trivial runs.
- Consent-first gates before execution.
- Coherence monitoring via `stats.show()` → `CoherenceMonitorBridge`.
- Additive & backward-compatible with existing KickLang codex.
- No override of human intent or embodiment grounding.

## Skill Architecture

### 1. Input Parsing Layer (KickLang → Internal Graph)

```python
# Conceptual pseudocode (to be implemented)
def parse_kicklang_dag(block: str) -> DAG:
    # Recognizes:
    #   make_task(...)
    #   task_list { id = append(task, deps=...) ... finish() }
    #   ⫻flow/parallel { ... }
    # Returns structured DAG with nodes, edges, TAS metadata
```

### 2. Translation Layer (Internal Graph → parallel_f)

- `make_task(callable, args)` → `parallel_f::make_task(...)`
- `append(task, *deps)` → `task_list.append(...)` with predecessor wiring
- `finish(detached)` / `flush()` → corresponding `parallel_f` calls
- Optional: code generation path that emits compilable C++ using the existing `parallel_f` headers

### 3. Execution & Delegation

- For prototype: Python stub that mimics `parallel_f` behavior or calls a future `pyparallel_f` binding.
- For production: Thin C++ bridge (Pybind11 / ctypes / subprocess with JSON protocol) or direct compilation of generated code.

### 4. Observability & Coherence Hook

```python
stats = executor.run(dag)
coherence_report = CoherenceMonitorBridge.ingest(stats)  # flux, drift, valence
```

### 5. Consent & Guard Layer

- Pre-execution consent gate (explicit or policy-based).
- Halt on detected drift from user meta-DNA or OCS invariants.
- Route embodiment-related tasks through Embodied Pipe patterns.

## Public API (Consumed by KickLang / OCS)

```kicklang
# Example usage inside any KickLang block or Meta-Playbook

⫻block/parallel_vision_pipeline {
  dag = task_list {
    load   = append( make_task(load_images, path) )
    detect = append( make_task(run_inference, model), deps: [load] )
    filter = append( make_task(filter_defects), deps: [detect] )
    report = append( make_task(generate_report), deps: [filter] )
  }
  result = dag.finish()
  stats.show()
}
```

The wrapper skill exposes:
- `execute_kicklang_dag(kicklang_block: str) -> ExecutionResult`
- `execute_parallel_f_dag(dag_spec: dict) -> ExecutionResult`
- `get_stats() -> CoherenceReport`

## Implementation Roadmap (TAS-aligned)

**PF-WRAPPER-TAS-001** — Syntax Parser  
Parse `make_task`, `task_list.append(deps)`, `finish`, `⫻flow/parallel` into internal DAG + TAS metadata.

**PF-WRAPPER-TAS-002** — Translation to parallel_f  
Map internal DAG to `parallel_f::task_list` / `make_task` calls (or generate compilable C++).

**PF-WRAPPER-TAS-003** — Execution Delegation  
Run via `parallel_f` (binding or codegen) with vthread scheduling.

**PF-WRAPPER-TAS-004** — Stats → Coherence Bridge  
Convert `stats.show()` output into `CoherenceMonitorBridge` format.

**PF-WRAPPER-TAS-005** — Consent & Guard Integration  
Pre-execution consent gate + drift detection.

**PF-WRAPPER-TAS-006** — Thin Python Stub (MVP)  
Working prototype that accepts the syntax and simulates or delegates execution.

## Example Thin Python Stub (MVP Starter)

```python
# parallel_f_kicklang_wrapper.py (prototype)
from typing import Any, Dict, List
import parallel_f  # future binding or stub

class ParallelFKicklangWrapper:
    def __init__(self):
        self.coherence_bridge = None  # inject CoherenceMonitorBridge

    def execute(self, kicklang_dag_block: str) -> Dict[str, Any]:
        dag = self._parse(kicklang_dag_block)
        pf_dag = self._translate_to_parallel_f(dag)
        result = pf_dag.finish()
        stats = parallel_f.stats.instance.get().show_stats()
        if self.coherence_bridge:
            self.coherence_bridge.ingest(stats)
        return {"result": result, "stats": stats}

    def _parse(self, block: str):
        # TODO: real KickLang parser or regex + AST for prototype
        pass

    def _translate_to_parallel_f(self, dag):
        tl = parallel_f.task_list()
        # build append calls with deps...
        return tl
```

## Consent / Halt Matrix

- **Halt** if execution would violate consent or cause excessive drift from established meta-DNA.
- **Pause** for explicit user confirmation on large or embodiment-linked DAGs.
- All runs are logged with full TAS traceability.

## Integration Points

- Consumes syntax produced by `kicklang-meta-playbook` and `kicklang-evolution-predictor`.
- Feeds stats directly into `CoherenceMonitorBridge`.
- Can be used inside `tas-forecast-cycle`, `unified-metaforge`, `underbody-inspection-pipeline`, or any TAS-heavy workflow.
- Future: MCP tool exposure for external orchestration.

## Post-Generation Recommendations

- Implement the Python stub in the sandbox and test with the 17-task example.
- Generate actual C++ code from a KickLang DAG (codegen path).
- Register as OCS participant via `ocs-skill-builder`.
- Add to your GitHub as a new focused repo or extend `parallel_f` itself.
