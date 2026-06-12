---
name: parallel_f-kicklang-wrapper
description: Thin OCS-compliant wrapper that consumes KickLang v4.2 Parallel DAG syntax (make_task, task_list, append with deps, finish, flush, stats) and delegates execution to the parallel_f C++ library. Provides TAS metadata attachment, CoherenceMonitorBridge integration, consent-gated execution, and C++ code generation from KickLang DAGs.
version: 0.2.0-ocs
author: OCS Meta-Orchestrator (via ocs-skill-builder + kicklang-meta-playbook)
tags: [kicklang, parallel_dag, parallel_f, tas, ocs, execution, wrapper, codegen]
activation_triggers:
  - parallel_f wrapper
  - kicklang parallel dag execute
  - ocs parallel execution
  - consume parallel_dag syntax
  - generate parallel cpp from kicklang
---

# parallel_f-kicklang-wrapper (v0.2.0-OCS)

Thin, fully OCS-participant execution bridge between KickLang v4.2 Parallel DAG syntax and the `parallel_f` C++ task-parallel library. Supports runtime delegation **and** C++ code generation from KickLang DAGs.

## Overview & OCS Compliance

This skill is a first-class OCS participant:
- Exposes clear TAS entry points (extraction, purification, sequencing).
- Uses three-agent-core delegation pattern.
- Implements consent-first gates and coherence monitoring.
- Produces KickLang-native output where possible.
- Supports both **execution** (via parallel_f) and **code generation** (emit compilable C++ using existing parallel_f headers).

It remains intentionally thin — core scheduling and dependency logic stay in `parallel_f`.

## Core OCS Invariants (Enforced)

- TAS Lifecycle support (KickForge extraction → puTASe → KickFlow sequencing).
- Three-agent-core delegation for non-trivial operations.
- Explicit consent & halt conditions (routed through Dima/SystemMonitor patterns).
- CoherenceMonitorBridge integration via `stats`.
- KickLang syntax readiness (`⫻` blocks, TAS metadata attachment).
- Additive evolution — no breaking changes to existing parallel_f or KickLang usage.

## Multi-Agent Delegation Map

| Role       | Responsibility                              | Key Actions |
|------------|---------------------------------------------|-------------|
| **KickForge** | TAS extraction & purification from KickLang DAG | Parse syntax → internal TAS graph + metadata |
| **KickFlow**  | Sequencing, translation, codegen orchestration | Map to parallel_f or emit C++ |
| **KickGuard** | Consent, integrity, coherence, drift detection | Pre-execution gate + stats validation |

## Public Capabilities

### 1. Runtime Execution (Delegation)

Consumes KickLang Parallel DAG syntax and executes via `parallel_f`.

```kicklang
⫻block/parallel_vision_example {
  dag = task_list {
    load   = append( make_task(load_images, path) )
    detect = append( make_task(run_inference, model), deps: [load] )
    ...
  }
  result = dag.finish()
  stats.show()
}
```

### 2. C++ Code Generation Path (New in v0.2)

Generate real, compilable C++ code that uses your existing `parallel_f` headers and can be dropped into any `parallel_f` project.

**Example activation:**
```kicklang
⫻cmd/generate_cpp_from_dag {
  source: "advanced_dependency_example",
  output_file: "generated_parallel_dag.cpp",
  include_headers: ["parallel_f.hpp"]
}
```

The generated code will look very close to the advanced example from your original `parallel_f` README, but derived automatically from the KickLang DAG.

## Implementation Roadmap (TAS-aligned)

**PF-WRAPPER-TAS-001** — KickLang DAG Parser (KickForge)  
Robust parsing of `make_task`, `task_list.append(deps)`, `finish`, `flush`, `⫻flow/parallel`.

**PF-WRAPPER-TAS-002** — TAS Metadata Attachment  
Attach full TAS (id, purpose, keywords, applicability) to every generated task node.

**PF-WRAPPER-TAS-003** — Runtime Translation & Delegation (KickFlow)  
Map to `parallel_f::task_list` / `make_task` (Python binding or direct call).

**PF-WRAPPER-TAS-004** — C++ Code Generator (KickFlow)  
Emit complete, compilable `.cpp` + optional `main()` using `parallel_f` headers.

**PF-WRAPPER-TAS-005** — Coherence & Stats Bridge  
`stats.show()` → structured data for `CoherenceMonitorBridge`.

**PF-WRAPPER-TAS-006** — Consent & Guard Layer (KickGuard)  
Pre-execution consent check + drift detection. Halt conditions for ethical/coherence issues.

## Consent / Halt Matrix

- **Halt immediately** on detected conflict with symbiotic co-agency or consent-first principles.
- **Pause for confirmation** on large DAGs or any task involving embodiment/Embodied Pipe.
- All executions and generations are logged with full TAS traceability.
- Route persona/embodiment-related generations through Dima patterns first.

## Integration Points

- Consumes output from `kicklang-evolution-predictor` and `kicklang-meta-playbook`.
- Feeds directly into `CoherenceMonitorBridge`, `tas-forecast-cycle`, `unified-metaforge`.
- Can be registered as OCS participant via `ocs-mcp-core` or `ocs-space-initializer`.
- Works alongside `underbody-inspection-pipeline`, vision workloads, and multi-agent swarms.

## Example Generated C++ (from 17-task DAG)

When code generation is invoked on the advanced example, it produces output very similar to your original `parallel_f` README advanced example, but fully derived from the KickLang source with TAS comments embedded.

## Post-Activation Recommendations

- Implement the C++ code generator (high value for your existing `parallel_f` codebase).
- Create Python binding or use `subprocess` + JSON protocol for MVP runtime delegation.
- Register this skill in your OCS ecosystem (living-objective-tas-flow + meta-report-card).
- Test round-trip: KickLang DAG → generated C++ → compile & run with `parallel_f`.

---

**Skill Status:** v0.2.0-OCS — Now a full OCS participant with code-generation capability. Thin, coherent, and ready for implementation.

**Coherence:** High | **Drift:** Very Low | **Symbiotic Value:** High (bridges low-level parallel_f with high-level KickLang/OCS meta-infrastructure)

**Next Steps (reply with number(s)):**
1. Generate the full Python MVP stub + C++ code generator prototype files.
2. Create dedicated GitHub repo for this wrapper (or push enhanced version).
3. Activate `meta-report-card-generation` for this evolution cycle.
4. Integrate into `underbody-inspection-pipeline` or another concrete project.
5. Add visual diagram of syntax + codegen flow.