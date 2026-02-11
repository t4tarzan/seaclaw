# Sea-Claw Terminal UX Reference

> Visual guide for the Sea-Claw interactive terminal experience.
> Covers startup sequence, status line, command execution, and TUI layout.

---

## 1. Startup Sequence

```
dev@sovereign-box:~$ ./sea_claw --interactive
```

The screen clears. A cyan-colored ASCII header appears:

```
  ____  ______  ___      ________  ___  ___      __
 / ___// ____/ / /  |   / ____/ / / /  | |     / /
 \__ \/ __/   / /|  |  / /   / / / /   | | /| / /
 ___/ / /___  / ___ | / /___/ /_/ /    | |/ |/ /
/____/_____/ /_/  |_| \____/\____/     |_|__|__/

 [SYSTEM] Substrate Initialized. Arena: 16MB (Fixed).
 [BRAIN]  Model 'Qwen-14B-Quantized' Mapped (Read-Only).
 [SHIELD] Grammar Filter: ACTIVE.
 [STATUS] Waiting for command... (Type /help)
```

### Startup Messages

Each subsystem reports readiness with a bracketed tag:

| Tag        | Meaning                                    |
|------------|--------------------------------------------|
| `[SYSTEM]` | Core arena allocator and runtime ready     |
| `[BRAIN]`  | Model mmap'd and tensor index loaded       |
| `[SHIELD]` | Grammar constraints and byte filters armed |
| `[STATUS]` | Idle — awaiting user input                 |

---

## 2. The Status Line (Live Ticker)

When the user hits **ENTER**, the input line locks and a **Live Status Ticker** appears at the bottom of the screen. It updates in milliseconds as the C-Engine cycles through its loop.

### Example: Invoice Generation

```
> Generate invoice for Acme Corp, $500

T+0ms:    [SENSES] Parsing Input... (Zero-Copy Mode)
T+2ms:    [SHIELD] Validating Input Grammar... OK.
T+5ms:    [BRAIN]  Indexing Weights... (Layer 1/32 mapped)
T+200ms:  [BRAIN]  Thinking... Extraction: {Client: "Acme Corp", Amount: 500}
T+210ms:  [HANDS]  Selecting Tool: TOOL_INVOICE_GEN
T+212ms:  [HANDS]  Executing Tool... PDF Generated (14kb).
T+215ms:  [HANDS]  Selecting Tool: TOOL_EMAIL_SEND
T+250ms:  [HANDS]  Connecting to SMTP... Sent.
T+255ms:  [HANDS]  Selecting Tool: TOOL_DB_LOG
T+260ms:  [CORE]   Resetting Memory Arena...
T+261ms:  [STATUS] Done. (261ms total)
```

### Status Line Tags

| Tag        | Color  | Subsystem                          |
|------------|--------|------------------------------------|
| `[SENSES]` | Blue   | Input parsing, wire protocol       |
| `[SHIELD]` | Yellow | Validation, grammar checks         |
| `[BRAIN]`  | Cyan   | Model inference, token generation  |
| `[HANDS]`  | Green  | Tool selection and execution       |
| `[DOCKER]` | Purple | Sidecar container lifecycle        |
| `[CORE]`   | White  | Arena reset, memory management     |
| `[STATUS]` | Bold   | Final result / idle state          |

---

## 3. Full TUI Layout

```
┌─────────────────────────────────────────────────────────────────────┐
│  Sea-Claw v1.0.0                    [Qwen-14B-Q4]  [Arena: 2.4MB]  │
├─────────────────────────────────────────────────────────────────────┤
│                                                                     │
│  User: Generate invoice for Acme Corp, $500                        │
│                                                                     │
│  AI: I'll create that invoice for you.                             │
│      [Thinking...]                                                 │
│      + Extracted: client="Acme Corp", amount=500.00                │
│      + Generated: Invoice #001 (14kb PDF)                          │
│      + Emailed to: billing@acme.com                                │
│      + Logged to ledger                                            │
│                                                                     │
│      Invoice sent! View: https://drive.google.com/d/abc123         │
│                                                                     │
├──────────────────────────┬──────────────────────────────────────────┤
│  Tools (active)          │  System                                  │
│  > invoice_gen    14kb   │  Memory:  142MB / 8GB   (1.8%)          │
│    email_send     OK     │  Arena:   2.4MB / 16MB  (15%)           │
│    db_log         OK     │  Model:   Qwen-14B-Q4   (mmap)         │
│                          │  Uptime:  3d 14h 22m                    │
├──────────────────────────┴──────────────────────────────────────────┤
│  T+261ms [STATUS] Done. 3 tools called. Arena reset.               │
├─────────────────────────────────────────────────────────────────────┤
│  > _                                                                │
└─────────────────────────────────────────────────────────────────────┘
```

### Layout Regions

| Region              | Position    | Content                                |
|---------------------|-------------|----------------------------------------|
| **Title bar**       | Top         | Version, active model, arena usage     |
| **Conversation**    | Center      | Chat history (scrollable)              |
| **Tools panel**     | Bottom-left | Active tools and their last output     |
| **System panel**    | Bottom-right| Memory, arena, model, uptime           |
| **Status line**     | Below panels| Live ticker during execution           |
| **Input prompt**    | Bottom      | User input with `> ` prefix            |

---

## 4. Command Examples

### Help

```
> /help

  Commands:
    /analyze <url|file>     Analyze a document (PDF, DOCX, etc.)
    /generate <prompt>      Generate content (code, text, etc.)
    /build <description>    Build an application via sidecar
    /status                 Show system status
    /model [name]           Switch active model
    /tools                  List available tools
    /clear                  Clear conversation
    /quit                   Exit Sea-Claw
```

### Status Check

```
> /status

  ┌─────────────────────────────────────────────┐
  │  SYSTEM STATUS                               │
  ├─────────────────────────────────────────────┤
  │  Engine:    Sea-Claw v1.0.0 (C11)           │
  │  Uptime:    3d 14h 22m                      │
  │  PID:       4821                             │
  │                                              │
  │  Memory:    142MB / 8192MB  (1.8%)           │
  │  Arena:     2.4MB / 16MB   (15.0%)          │
  │  Peak:      4.1MB          (25.6%)          │
  │                                              │
  │  Model:     Qwen-14B-Q4 (mmap, read-only)   │
  │  Tensors:   291 indexed                      │
  │  Pages:     1,024 resident / 8,192 mapped    │
  │                                              │
  │  Tools:     12 registered, 3 used this sess. │
  │  Sidecars:  0 running, 47 completed today    │
  │  Shield:    ACTIVE (grammar + byte filter)   │
  │                                              │
  │  Telegram:  Connected (@seaclaw_bot)         │
  │  Drive:     Google Drive (authenticated)     │
  │  APIs:      Kimi (ok), OpenAI (ok)           │
  └─────────────────────────────────────────────┘
```

### Docker Sidecar Execution

```
> /analyze https://example.com/contract.pdf

T+0ms:    [SENSES] Parsing Input... (Zero-Copy Mode)
T+2ms:    [SHIELD] Validating URL... OK. (HTTPS, allowed domain)
T+50ms:   [SENSES] Downloading PDF... 2.3MB received.
T+55ms:   [SHIELD] File Magic: %PDF-1.4 ... OK.
T+60ms:   [DOCKER] Spawning sidecar: seaclaw/pdf-parser:v1.0
T+62ms:   [DOCKER]   --network none  --memory 512m  --timeout 30s
T+1200ms: [DOCKER] Sidecar exited (0). Output: 48KB text extracted.
T+1205ms: [DOCKER] Container destroyed. Zero residue.
T+1210ms: [BRAIN]  Sending to Kimi API... (context: 12K tokens)
T+3400ms: [BRAIN]  Analysis complete. 2,100 tokens generated.
T+3410ms: [HANDS]  Uploading to Google Drive...
T+3800ms: [HANDS]  Link generated (expires: 7d).
T+3805ms: [CORE]   Resetting Memory Arena...
T+3806ms: [STATUS] Done. (3.8s total)

  AI: Contract analysis complete!
      Key findings:
      - Liability clause in Section 4.2 (high risk)
      - Non-compete expires 2027-06-01
      - Payment terms: Net 30

      Full report: https://drive.google.com/d/xyz789
```

### Error Handling

```
> /analyze http://malicious-site.ru/payload.exe

T+0ms:    [SENSES] Parsing Input... (Zero-Copy Mode)
T+2ms:    [SHIELD] Validating URL... REJECTED.
T+2ms:    [SHIELD]   Reason: HTTP (not HTTPS), untrusted domain.
T+3ms:    [STATUS] Blocked. (3ms total)

  AI: Request blocked by Shield.
      - URL must use HTTPS
      - Domain not in allowlist
      Type /help for usage.
```

---

## 5. Color Scheme

| Element            | Color         | ANSI Code  |
|--------------------|---------------|------------|
| ASCII banner       | Cyan          | `\033[36m` |
| `[SYSTEM]` tag     | White bold    | `\033[1m`  |
| `[SENSES]` tag     | Blue          | `\033[34m` |
| `[SHIELD]` tag     | Yellow        | `\033[33m` |
| `[BRAIN]` tag      | Cyan          | `\033[36m` |
| `[HANDS]` tag      | Green         | `\033[32m` |
| `[DOCKER]` tag     | Magenta       | `\033[35m` |
| `[CORE]` tag       | White         | `\033[37m` |
| `[STATUS]` tag     | White bold    | `\033[1m`  |
| Timestamps (T+)    | Dim           | `\033[2m`  |
| Errors / REJECTED  | Red bold      | `\033[1;31m` |
| User input `>`     | Green bold    | `\033[1;32m` |
| AI response        | Default       | `\033[0m`  |
| Panel borders      | Dim white     | `\033[2;37m` |

---

## 6. Design Principles

- **Millisecond timestamps** — every action is timed, showing the engine's speed
- **Subsystem tags** — user always knows which pillar is active
- **Zero ambiguity** — blocked requests show exact reason immediately
- **Arena reset visible** — user sees memory cleanup happen in real time
- **No spinners** — real progress, not fake loading bars
- **Minimal chrome** — borders and panels use dim colors, content is bright

---

*Document Version: 1.0.0*
*Status: UX Reference — Ready for ncurses implementation*
