# KV Cache, Indexing & Weight Management: Cloud vs Local LLMs

## How This Applies to Sea-Claw

Sea-Claw currently sends a stateless HTTP POST to an OpenAI-compatible endpoint.
Every request re-sends the **full context** (system prompt + memory + history + user input)
as raw text. The provider's inference engine handles all tokenization, KV cache, and
weight indexing internally. Sea-Claw never touches model weights or KV tensors directly.

This document compares what happens **behind the API** for cloud providers vs local
runtimes, and recommends which local runtime best fits Sea-Claw's architecture.

---

## 1. What Sea-Claw Sends Today (All Providers)

```
POST /v1/chat/completions
{
  "model": "moonshotai/kimi-k2.5",
  "max_tokens": 4096,
  "temperature": 0.7,
  "messages": [
    {"role": "system", "content": "<system_prompt + SOUL.md + USER.md + recall facts + tool list>"},
    {"role": "user",   "content": "...history msg 1..."},
    {"role": "assistant", "content": "...history msg 2..."},
    {"role": "user",   "content": "<current user input>"}
  ]
}
```

**Key insight:** Sea-Claw sends **zero** weight data, **zero** KV tensors, **zero** token
offsets. It sends raw text. Everything below the API boundary is handled by the provider.

This is identical for OpenRouter, OpenAI, Anthropic, Gemini, and local (Ollama/LM Studio).
The OpenAI-compatible `/v1/chat/completions` contract is the same everywhere.

---

## 2. What Happens Behind the API — Cloud vs Local

### 2.1 Cloud Providers (OpenRouter, OpenAI, Anthropic, Gemini)

```
┌─────────────────────────────────────────────────────────────┐
│  YOUR REQUEST (JSON text)                                    │
│  "messages": [{system}, {user}, {assistant}, {user}]         │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  TOKENIZER (provider-side)                                   │
│  - BPE/SentencePiece converts text → token IDs               │
│  - Token IDs are integer indices into the model's vocabulary  │
│  - e.g. "Sea-Claw" → [14220, 12, 2149, 675]                 │
│  - These are NOT weight indices — they're vocabulary offsets  │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  EMBEDDING LOOKUP                                            │
│  - Token ID → row in embedding matrix (part of model weights)│
│  - embedding_matrix[token_id] → dense vector (e.g. 4096-dim) │
│  - This IS a weight lookup, but it's a table index, not a    │
│    "weight being sent" — the weights live on the GPU          │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  PREFILL PHASE (compute K and V for all input tokens)        │
│  - For each transformer layer:                               │
│    K[i] = input[i] × W_K    (key projection)                │
│    V[i] = input[i] × W_V    (value projection)              │
│  - These K,V tensors are stored in the KV CACHE              │
│  - W_K, W_V are the model's trained weight matrices          │
│  - They NEVER leave the GPU cluster                          │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  KV CACHE (the "working memory" of inference)                │
│                                                              │
│  Cloud providers use PAGED ATTENTION (vLLM-style):           │
│  - GPU memory pre-divided into fixed-size blocks (16 tokens) │
│  - Block table maps logical positions → physical GPU blocks  │
│  - Content-addressed hashing for prefix reuse:               │
│    hash(block_N) = sha256(hash(block_N-1), token_ids, salt)  │
│  - Multiple requests sharing a prefix REUSE the same blocks  │
│  - ref_count tracks sharing; LRU eviction when ref_count = 0 │
│                                                              │
│  PROMPT CACHING (automatic on most providers):               │
│  - OpenAI: auto, min 1024 tokens, reads at 0.25-0.50x cost  │
│  - Anthropic: explicit cache_control breakpoints, 0.1x reads │
│  - OpenRouter: passes through to underlying provider         │
│  - Gemini: implicit caching, 0.25x reads                     │
│                                                              │
│  Response tells you what was cached:                         │
│  "prompt_tokens_details": {                                  │
│    "cached_tokens": 10318,    ← prefix cache HIT             │
│    "cache_write_tokens": 0    ← nothing new written          │
│  }                                                           │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  DECODE PHASE (autoregressive generation)                    │
│  - Generate 1 token at a time                                │
│  - Each new token: compute K,V only for that token           │
│  - Attention reads from full KV cache (all prior tokens)     │
│  - Append new K,V to cache                                   │
│  - Repeat until EOS or max_tokens                            │
└─────────────────────────────────────────────────────────────┘
```

**What the API key "unlocks":**
- Access to the provider's GPU cluster where weights are **already loaded**
- The API key is an auth token, NOT a weight index or model offset
- Weights are shared across all users of the same model
- Your request gets a **slot** on a GPU that has the model loaded
- KV cache is per-request (or per-prefix for cached prompts)

### 2.2 Local LLMs (Ollama / LM Studio / llama.cpp)

```
┌─────────────────────────────────────────────────────────────┐
│  YOUR REQUEST (identical JSON text)                          │
│  Same /v1/chat/completions format                            │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  MODEL LOADING (one-time, at server startup)                 │
│                                                              │
│  GGUF file on disk → mmap into memory:                       │
│  - Model weights are MEMORY-MAPPED (not fully loaded to RAM) │
│  - OS pages in weight data on demand from NVMe/SSD           │
│  - 7B Q4_K_M model ≈ 4.4 GB on disk, ~4.4 GB virtual memory │
│  - Actual RSS depends on which layers are accessed            │
│                                                              │
│  GPU offloading (if available):                              │
│  - -ngl N: offload N transformer layers to GPU VRAM          │
│  - Remaining layers run on CPU using mmap'd weights          │
│  - Split: embedding on CPU, middle layers on GPU, head on CPU│
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  TOKENIZER (local, bundled in GGUF)                          │
│  - Same BPE/SentencePiece as cloud, but runs on YOUR CPU     │
│  - Token vocabulary is embedded in the GGUF file             │
│  - Identical token IDs as the original model                 │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  KV CACHE (allocated in RAM or VRAM at startup)              │
│                                                              │
│  SLOT-BASED (llama.cpp / Ollama / LM Studio):                │
│  - Server starts with N slots: -c 8192 -np 2 → 2 slots      │
│  - Each slot = context_size/N tokens of KV cache             │
│  - KV cache memory = 2 × n_layers × n_heads × head_dim      │
│                       × context_length × bytes_per_element   │
│                                                              │
│  Example: Llama 3 8B, 8192 context, F16 KV:                 │
│    2 × 32 × 32 × 128 × 8192 × 2 bytes = ~4 GB              │
│  With Q8_0 KV quantization: ~2 GB (50% saving)              │
│  With Q4_0 KV quantization: ~1.3 GB (66% saving)            │
│                                                              │
│  PROMPT CACHING (slot reuse):                                │
│  - llama.cpp matches incoming prompt against existing slots  │
│  - -sps 0.5: reuse slot if ≥50% of prompt prefix matches    │
│  - Matched tokens skip prefill entirely (instant)            │
│  - Only new/changed tokens get computed                      │
│  - Slot persistence: save/restore KV cache to disk           │
│                                                              │
│  vs PAGED ATTENTION (vLLM-style, NOT in llama.cpp):          │
│  - llama.cpp uses contiguous KV buffers per slot             │
│  - No block-level sharing between requests                   │
│  - No content-addressed hashing                              │
│  - Simpler but less memory-efficient for multi-tenant        │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
┌─────────────────────────────────────────────────────────────┐
│  PREFILL + DECODE (same math as cloud, but on YOUR hardware) │
│  - Prefill: batch-compute K,V for all input tokens           │
│  - Decode: one token at a time, append to KV cache           │
│  - Speed depends on: CPU cores, RAM bandwidth, GPU VRAM      │
└─────────────────────────────────────────────────────────────┘
```

---

## 3. The Critical Difference: What Gets "Indexed"

### Cloud: You Index Nothing

| What you send | What the provider does | What you get back |
|---|---|---|
| Raw text in JSON | Tokenize → embed → prefill → decode | Raw text + token counts |
| API key (auth) | Route to GPU with model loaded | `cached_tokens` count |
| Model name string | Select which weight matrix to use | `prompt_tokens`, `completion_tokens` |

**You never see:** token IDs, embedding vectors, KV tensors, attention scores, weight matrices.
The API is a **text-in, text-out black box**. The "index" is just the model name string.

### Local: You Own Everything

| What you control | How it works | Sea-Claw relevance |
|---|---|---|
| GGUF file on disk | mmap'd weights, OS pages on demand | Could mmap directly in C |
| KV cache in RAM/VRAM | Slot-based, configurable quantization | Could manage slots from C |
| Tokenizer | Bundled in GGUF, runs locally | Could call llama.cpp tokenizer |
| Context window | `-c` flag, split across slots | Maps to `max_tokens` in config |
| KV quantization | `OLLAMA_KV_CACHE_TYPE=q8_0` | Halves KV memory usage |
| Layer offloading | `-ngl N` for GPU layers | Configurable per hardware |

**The weights are the same mathematical matrices** whether cloud or local.
The difference is **who owns the GPU/RAM** and **who manages the KV cache**.

---

## 4. Ollama vs LM Studio — Deep Comparison for Sea-Claw

### 4.1 Architecture

| Aspect | Ollama | LM Studio |
|---|---|---|
| **Core engine** | llama.cpp (Go wrapper) | llama.cpp (Electron + native) |
| **Design philosophy** | API-first, headless daemon | GUI-first, desktop app |
| **Process model** | `ollama serve` → background daemon | Desktop app with embedded server |
| **Model format** | GGUF (via Modelfile abstraction) | GGUF (direct HuggingFace download) |
| **Multi-model** | Concurrent serving, auto-load/unload | One model at a time (typically) |
| **Platform** | Linux, macOS, Windows, Docker | macOS, Windows, Linux |
| **License** | MIT (open source) | Proprietary (free for personal use) |
| **Server startup** | `ollama serve` or systemd | Launch app → enable server |

### 4.2 API Compatibility with Sea-Claw

| Feature | Ollama | LM Studio |
|---|---|---|
| **OpenAI-compatible endpoint** | `http://localhost:11434/v1/chat/completions` | `http://localhost:1234/v1/chat/completions` |
| **Auth required** | No (local only) | Optional Bearer token |
| **Request format** | Identical to OpenAI | Identical to OpenAI |
| **Response format** | Standard `choices[0].message.content` | Standard + extra `stats`, `model_info`, `runtime` fields |
| **Tool/function calling** | Yes (Mistral, Llama 3.1+, Qwen 2.5) | Yes (via llama.cpp) |
| **Streaming** | Yes (`stream: true`) | Yes (`stream: true`) |
| **`cache_prompt`** | Implicit (always on) | Implicit (always on) |
| **Token usage in response** | `prompt_tokens`, `completion_tokens` | Same + `tokens_per_second`, `time_to_first_token` |

**Both work with Sea-Claw's current code with ZERO changes** to `sea_agent.c` or `sea_http.c`.
The only change needed is the `api_url` in `config.json`.

### 4.3 KV Cache Management

| Feature | Ollama | LM Studio |
|---|---|---|
| **KV cache quantization** | Yes: `OLLAMA_KV_CACHE_TYPE=q8_0/q4_0/f16` | Yes: via llama.cpp settings |
| **Flash Attention** | `OLLAMA_FLASH_ATTENTION=1` | Enabled by default |
| **Slot management** | Automatic (hidden from user) | Automatic (hidden from user) |
| **Prompt prefix caching** | Automatic (llama.cpp slot reuse) | Automatic (llama.cpp slot reuse) |
| **Slot persistence (save/restore)** | Not exposed via API | Not exposed via API |
| **Manual slot control (`id_slot`)** | Not exposed | Not exposed |
| **Context window config** | `num_ctx` in Modelfile or API | GUI slider or API parameter |
| **KV cache per-model config** | Via Modelfile `PARAMETER` | Via GUI settings |

**Neither exposes raw KV cache manipulation via API.** Both handle it internally via llama.cpp.
To get slot-level control, you'd need to run `llama-server` directly (bypassing both).

### 4.4 Model Weight Handling

| Feature | Ollama | LM Studio |
|---|---|---|
| **Weight loading** | mmap (llama.cpp default) | mmap (llama.cpp default) |
| **Quantization support** | Q2_K through Q8_0, F16, F32 | Q2_K through Q8_0, F16, F32 |
| **GPU offloading** | Auto-detect, configurable `num_gpu` | Auto-detect, GUI slider |
| **Multi-GPU** | Yes (split across GPUs) | Limited |
| **Model hot-swap** | Auto-unload idle models, load on demand | Manual model switching |
| **Concurrent models** | Yes (memory permitting) | One at a time |
| **Model storage** | `~/.ollama/models/` (blob store) | `~/.cache/lm-studio/models/` |

### 4.5 Performance on 8GB RAM (Hetzner Server)

| Scenario | Ollama | LM Studio |
|---|---|---|
| **7B Q4_K_M + 4K context** | ~5.5 GB total (model 4.4 + KV 1.1) ✅ | Same (same engine) |
| **7B Q4_K_M + 8K context** | ~6.6 GB total ✅ (tight) | Same |
| **7B Q4_K_M + 8K + Q8_0 KV** | ~5.5 GB total ✅ (comfortable) | Same |
| **13B Q4_K_M + 4K context** | ~8.2 GB total ❌ (swap) | Same |
| **3B Q4_K_M + 32K context** | ~4.5 GB total ✅ | Same |
| **Headless overhead** | ~30 MB (Go binary) | ~300 MB (Electron) ❌ |
| **Startup time** | <1s (daemon already running) | 3-5s (app launch) |

---

## 5. Recommendation: Ollama for Sea-Claw

### Why Ollama Wins

1. **API-first design** — Sea-Claw is a headless C binary. Ollama is a headless Go daemon. No GUI overhead. LM Studio wastes ~300 MB on Electron for a server we'll never look at.

2. **Identical API contract** — `http://localhost:11434/v1/chat/completions` is byte-for-byte compatible with Sea-Claw's existing `build_request_json()` and `parse_llm_response()`. Zero code changes needed.

3. **Daemon model** — `ollama serve` runs as a systemd service alongside Sea-Claw. Auto-starts, auto-recovers, auto-loads models on first request, auto-unloads when idle. Perfect for our Hetzner server.

4. **KV cache quantization** — `OLLAMA_KV_CACHE_TYPE=q8_0` halves KV memory with negligible quality loss. Critical for fitting 7B + 8K context in 8 GB RAM.

5. **Concurrent model serving** — Can serve multiple model sizes (3B for fast tasks, 7B for complex ones) and auto-swap based on demand.

6. **MIT license** — Open source, no licensing concerns for redistribution.

7. **Docker-native** — Already have `docker-compose.yml` in the project. Ollama has official Docker images.

8. **Model management** — `ollama pull llama3` is simpler than manually downloading GGUF files. Modelfile system allows custom system prompts, parameters, and quantization per model.

### What Sea-Claw Needs to Change for Local LLM

**Answer: Almost nothing.** The current code already works:

```c
// sea_agent.c line 55 — already configured:
case SEA_LLM_LOCAL: cfg->api_url = "http://localhost:11434/v1/chat/completions"; break;

// sea_agent.c line 64 — already configured:
case SEA_LLM_LOCAL: cfg->model = "llama3"; break;

// sea_agent.c line 373 — already skips API key for local:
if (!cfg->api_key && cfg->provider != SEA_LLM_LOCAL) { ... }
```

The only additions for **optimal** local performance:

| Change | Why | Effort |
|---|---|---|
| Add `cache_prompt: true` to request JSON | Explicit prompt caching hint | 1 line in `build_request_json()` |
| Parse `tokens_per_second` from response | Monitor local inference speed | 5 lines in `parse_llm_response()` |
| Add `num_ctx` to request JSON | Control context window per-request | 3 lines in `build_request_json()` |
| Onboard wizard: detect Ollama | `curl localhost:11434/api/tags` | 10 lines in `main.c` |

---

## 6. The "Different Weights and Index" Question — Answered

### Are cloud providers sending different weights than local?

**No.** The weights are mathematically identical. Here's the chain:

```
Meta releases Llama 3 8B weights (F16, ~16 GB)
         │
         ├──→ OpenRouter hosts on A100 GPUs (F16 or BF16, full precision)
         │     └── KV cache: F16, paged attention, multi-tenant
         │
         ├──→ Ollama downloads GGUF (Q4_K_M, ~4.4 GB, quantized)
         │     └── KV cache: configurable (F16/Q8_0/Q4_0), slot-based
         │
         └──→ LM Studio downloads same GGUF
               └── KV cache: same as Ollama (same llama.cpp engine)
```

**The difference is PRECISION, not different weights:**

| Aspect | Cloud (OpenRouter) | Local (Ollama Q4_K_M) |
|---|---|---|
| Weight precision | F16/BF16 (16-bit) | Q4_K_M (4-bit mixed) |
| Weight size | ~16 GB per 8B model | ~4.4 GB per 8B model |
| Quality loss | None (full precision) | ~0.5% perplexity increase |
| KV cache precision | F16 (typically) | Configurable (F16/Q8_0/Q4_0) |
| KV cache memory | Managed by provider | Your RAM/VRAM |
| Inference speed | 50-100+ tok/s (A100) | 10-30 tok/s (CPU), 30-80 tok/s (GPU) |

**The API key does NOT send "different weights."** It authenticates you to a GPU cluster
where the full-precision weights are already loaded. You send text, they send text back.
The weights never cross the network.

### What about "index" parameters?

The only "index" in the entire flow is the **token ID** — an integer that maps to a row
in the embedding matrix. This is determined by the **tokenizer**, which is:
- Bundled inside the GGUF file (local)
- Running on the provider's server (cloud)
- **Identical** for the same model family (Llama 3 uses the same tokenizer everywhere)

There is no "weight index" or "offset parameter" sent over the API. The model name
string (`"llama3"` or `"meta-llama/llama-3-8b-instruct"`) is the only identifier.

---

## 7. Sea-Claw's Recall System vs Cloud Prompt Caching

Sea-Claw's `sea_recall` module is actually solving the **same problem** as cloud prompt
caching, but at a different layer:

| Layer | Cloud Solution | Sea-Claw Solution |
|---|---|---|
| **KV cache reuse** | Paged attention, prefix caching | N/A (provider handles) |
| **Context efficiency** | Send same prefix → auto-cached | `sea_recall`: only send relevant facts |
| **Token cost reduction** | `cached_tokens` at 0.1-0.5x price | Fewer tokens sent = fewer tokens billed |
| **Memory across sessions** | None (stateless API) | SQLite recall DB persists facts |

**Sea-Claw's approach is complementary:**
- Cloud caching reduces cost of **re-processing** the same tokens
- Sea-Claw recall reduces the **number of tokens sent** in the first place
- Together: send fewer tokens AND pay less for the ones that repeat

For local LLMs, Sea-Claw's recall is even more valuable because:
- Local inference is **CPU-bound** — fewer input tokens = faster prefill
- KV cache is **RAM-bound** — shorter context = more room for generation
- No billing, but **latency** is the cost — recall directly reduces it

---

## 8. Phase 2 Vision: What Changes for Direct llama.cpp Integration

If Sea-Claw ever bypasses Ollama and calls llama.cpp directly (Phase 2 local model):

| Component | Current (HTTP API) | Phase 2 (Direct C integration) |
|---|---|---|
| Model loading | Ollama handles | `llama_load_model_from_file()` with mmap |
| Tokenization | Provider handles | `llama_tokenize()` in C |
| KV cache | Provider handles | `llama_kv_cache_*()` functions |
| Inference | HTTP round-trip | Direct `llama_decode()` calls |
| Memory | Arena for JSON strings | Arena + mmap for weights + KV cache |
| Slot management | Provider handles | Manual slot allocation in C |

This would give Sea-Claw:
- **Zero HTTP overhead** (no JSON serialization, no curl, no network)
- **Direct KV cache control** (save/restore slots, quantize KV)
- **mmap weight sharing** (multiple Sea-Claw instances share one model in RAM)
- **Token-level streaming** (callback per token, not per HTTP chunk)

But this is a **major undertaking** (~3000+ lines of C) and Ollama already provides
95% of the benefit with 0% of the effort. **Recommended: use Ollama for v2, consider
direct integration for v3.**

---

## 9. Quick Start: Sea-Claw + Ollama on Hetzner 8GB

```bash
# Install Ollama
curl -fsSL https://ollama.com/install.sh | sh

# Configure for 8GB RAM
export OLLAMA_FLASH_ATTENTION=1
export OLLAMA_KV_CACHE_TYPE=q8_0

# Pull a model that fits
ollama pull llama3          # 7B Q4_K_M, ~4.4 GB
# or for tighter RAM:
ollama pull phi3:mini       # 3.8B Q4_K_M, ~2.3 GB

# Sea-Claw config.json
{
  "llm_provider": "local",
  "llm_model": "llama3",
  "api_url": "http://localhost:11434/v1/chat/completions"
}

# Run Sea-Claw — it just works
./sea_claw --telegram
```

---

## 10. Summary Table

| Question | Answer |
|---|---|
| Do cloud APIs send different weights? | **No.** Same model, higher precision (F16 vs Q4). Weights never cross the network. |
| Does the API key unlock different indices? | **No.** It's auth only. Routes you to a GPU with the model loaded. |
| Is KV cache different cloud vs local? | **Yes.** Cloud uses paged attention (vLLM). Local uses slot-based (llama.cpp). Same math, different memory management. |
| Ollama or LM Studio for Sea-Claw? | **Ollama.** API-first, headless, MIT license, 30 MB overhead vs 300 MB, systemd-native, Docker-native. |
| Code changes needed for local? | **Near zero.** `SEA_LLM_LOCAL` already configured. Just set `config.json`. |
| Should we integrate llama.cpp directly? | **Not yet.** Ollama gives 95% of the benefit. Direct integration is a v3 project (~3000 lines). |
| Does Sea-Claw's recall help with local? | **Yes, even more than cloud.** Fewer tokens = faster prefill on CPU. |
