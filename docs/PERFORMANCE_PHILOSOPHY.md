# Sea-Claw Performance Philosophy

> The user feels like they are interacting with a piece of hardware, not a website.

---

## The Three Speeds

Sea-Claw performance is measured on three independent axes. Optimizing for the wrong one wastes effort.

| Axis | What It Measures | Sea-Claw Target | Python Agent Typical |
|------|-----------------|-----------------|---------------------|
| **Thinking** (Inference) | Token generation speed | 2–5 tok/s (CPU/mmap) | 30–100 tok/s (GPU) |
| **Reacting** (Latency) | Time from input to first action | < 1ms | 500–600ms |
| **Doing** (Tool Execution) | Time to complete a tool call | < 0.01ms (function pointer) | 50–200ms (subprocess) |

**Sea-Claw is slower at Thinking. It is faster at everything else.**

The user notices latency and tool speed far more than token throughput. A 200ms pause before the answer starts typing feels instant. A 600ms pause before anything happens feels broken.

---

## Why Python Agents Feel Slow

A typical Python agent (OpenClaw, LangChain) on every request:

```
Trigger:  User sends message
  +50ms   Python interpreter wakes up
  +200ms  Loads torch, pydantic, requests
  +100ms  OS allocates 500MB RAM
  +50ms   GPU inference: "I need to check logs"     ← Fast!
  +100ms  Spawns shell process for grep              ← Slow
  +50ms   Parses JSON output
  +20ms   Garbage collection pause
  ─────
  ~600ms  Total reaction time
```

The GPU inference is fast, but it's buried under 550ms of overhead that the user *feels*.

---

## Why Sea-Claw Feels Instant

Sea-Claw on the same request:

```
Trigger:  User sends message
  +0.01ms Binary already running in while(1) loop — catches packet instantly
  +0.01ms Zero-copy parser points into Arena (no malloc, no copy)
  +200ms  CPU/mmap inference: "Check logs"           ← Slower than GPU
  +0.001ms Function pointer jump to tool_read_logs   ← Instant
  +0.001ms Arena reset (one pointer move)
  ─────
  ~200ms  Total reaction time
```

The inference is slower, but the user gets the *result* 3x faster because there's zero overhead around it.

---

## The Compound Effect: Complex Tasks

Single tool calls show a 3x advantage. Multi-step tasks show orders of magnitude:

### "Read 1,000 files and find errors"

**Python Agent:**
```
Per file: spawn process (5ms) + read (1ms) + parse (2ms) + GC (1ms) = ~9ms
1,000 files × 9ms = 9,000ms (9 seconds)
```

**Sea-Claw:**
```
Per file: function pointer (0.001ms) + mmap read (0.1ms) + arena parse (0.01ms) = ~0.11ms
1,000 files × 0.11ms = 110ms (0.11 seconds)
```

**Sea-Claw: 80x faster on bulk operations.**

### "Call 5 tools in sequence"

**Python Agent:**
```
5 × (serialize args + context switch + subprocess + deserialize) = 5 × 120ms = 600ms
```

**Sea-Claw:**
```
5 × (validate grammar + function pointer jump + arena alloc) = 5 × 0.05ms = 0.25ms
```

**Sea-Claw: 2,400x faster on tool chains.**

---

## Implementation Rules

These principles translate directly into code decisions:

### 1. Never Leave the Event Loop

```c
// CORRECT: main.c runs forever, catches events instantly
int main() {
    sea_arena_create(&arena, 16 * 1024 * 1024);
    sea_model_mmap(&model, config.model_path);

    while (1) {
        int n = sea_wire_poll(fds, nfds, -1);  // epoll/kqueue — blocks until event
        for (int i = 0; i < n; i++) {
            sea_handle_event(&fds[i], &arena);
        }
    }
}

// WRONG: Spawning a new process per request
// fork() + exec() = 5-50ms overhead per request
```

### 2. Never Allocate on the Hot Path

```c
// CORRECT: Arena bump allocation — O(1), no syscall
void* ptr = sea_arena_alloc(arena, size);  // Just moves a pointer

// WRONG: malloc on every request
void* ptr = malloc(size);  // Syscall, fragmentation, eventual GC
```

### 3. Tools Are Function Pointers, Not Processes

```c
// CORRECT: Direct jump — 1 CPU instruction
SeaError result = tool_registry[tool_id].func(args, arena, output);

// WRONG: Spawning a subprocess
system("python3 tool_invoice.py --client 'Acme'");  // 50-200ms overhead
```

### 4. Parse by Pointing, Not Copying

```c
// CORRECT: Zero-copy — just point into the existing buffer
SeaSlice cmd = sea_json_find(buffer, "cmd");  // cmd.start points into buffer

// WRONG: Copy into new allocation
char* cmd = strdup(json_get_string(parsed, "cmd"));  // malloc + memcpy
```

### 5. Reset, Don't Free

```c
// CORRECT: One pointer move — instant, zero residue
sea_arena_reset(request_arena);

// WRONG: Walk the heap freeing individual allocations
free(parsed_json);
free(tool_args);
free(tool_output);
free(response);  // Miss one → memory leak
```

### 6. mmap, Don't Load

```c
// CORRECT: OS pages in 4KB chunks on demand
SeaModel* m = sea_model_mmap("qwen-7b-q4.clawmodel");
// 4GB model on 8GB RAM — OS handles paging

// WRONG: Read entire model into RAM
FILE* f = fopen("model.bin", "rb");
fread(buffer, 1, 4GB, f);  // OOM on 8GB machine
```

### 7. Inference Is the Only Acceptable Wait

The user should **never** wait for anything except the AI thinking. Every other operation (parsing, tool calls, arena reset, output delivery) must be imperceptible (< 1ms).

```
Acceptable:
  [BRAIN] Thinking... (200ms–2000ms)  ← User expects this

Not acceptable:
  [SENSES] Parsing... (50ms)          ← Should be < 0.1ms
  [HANDS] Loading tool... (100ms)     ← Should be < 0.01ms
  [CORE] Cleaning up... (20ms)        ← Should be < 0.01ms
```

---

## Performance Targets (v1.0)

| Metric | Target | How to Verify |
|--------|--------|---------------|
| Cold start (binary launch) | < 50ms | `time ./sea_claw --ping` |
| Event loop wake-up | < 0.1ms | Timestamp in `sea_wire_poll` return |
| JSON parse (1KB payload) | < 0.1ms | `test_json.c` benchmark |
| Tool call overhead | < 0.01ms | `test_tools.c` — measure registry lookup + dispatch |
| Arena alloc (any size) | < 0.001ms | `test_arena.c` — 1M allocations timed |
| Arena reset | < 0.001ms | `test_arena.c` — single pointer move |
| Shield validation | < 0.1ms | `test_shield.c` — charset check on 1KB input |
| Grammar constraint apply | < 1ms | Per-token logit masking during inference |
| Inference (7B Q4, CPU) | 2–5 tok/s | `clawbench` on target hardware |
| Full request cycle (excl. inference) | < 5ms | End-to-end minus brain time |
| Docker sidecar spawn | < 2000ms | Acceptable — user sees `[DOCKER] Spawning...` |

---

## The Vibe Test

If you can't measure it, feel it:

- **Hardware feel**: Typing a command and getting tool results should feel like pressing a button on a calculator — instant feedback, no loading state.
- **Inference feel**: The AI "typing" its response should feel like watching someone think — a natural, expected pause.
- **No spinners**: Real progress (millisecond timestamps), not fake loading bars.
- **No jank**: No GC pauses, no random stutters, no "please wait" messages for non-inference operations.

> Even if Sea-Claw takes a second longer to *type* the answer,
> it *finds* the answer instantly.

---

*Document Version: 1.0.0*
*Status: Design Principle — Governs all implementation decisions*
