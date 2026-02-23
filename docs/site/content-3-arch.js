// Part 3: Architecture sections (enhanced with expanded analogies, attack walkthrough)

var CONTENT_ARCH = `
<section class="doc-section" id="five-pillars">
<h2>The Five Pillars</h2>
<p>Sea-Claw is built like a <strong>human body</strong>. Each pillar has a clear job, and they work together without stepping on each other&rsquo;s toes:</p>

<div class="diagram">+-------------+-----------+-----------+----------+------------+
| Substrate   |  Senses   |  Shield   |  Brain   |   Hands    |
| - Arena     | - JSON    | - Grammar | - Agent  | - 63 Tools |
| - Log       | - HTTP    | - Inject  | - LLM    | - File I/O |
| - DB        | - SSE     | - URL     | - Stream | - Shell    |
| - Config    |           | - PII     | - Think  | - Web/Data |
+-------------+-----------+-----------+----------+------------+
|        Channels: Telegram | TUI | API | WS | Slack         |
+-------------------------------------------------------------+
|        Infrastructure: Auth | Cron | Mesh | Bus             |
+-------------------------------------------------------------+
|        SeaZero: Bridge | Proxy | Workspace                  |
+-------------------------------------------------------------+</div>

<table>
<tr><th>Pillar</th><th>Analogy</th><th>What It Does</th></tr>
<tr><td><strong>Substrate</strong></td><td>The <strong>skeleton and bloodstream</strong>. Without bones, nothing stands up. Without blood, nothing gets nutrients. The Substrate provides memory (arena), record-keeping (database), logging (nervous system), and configuration (DNA).</td><td>Arena allocator, SQLite DB, structured logging, JSON config loader</td></tr>
<tr><td><strong>Senses</strong></td><td>The <strong>eyes and ears</strong>. How Sea-Claw perceives the outside world. It can read structured data (JSON parser) and communicate over the internet (HTTP client), including real-time streaming (SSE).</td><td>Zero-copy JSON parser, HTTP client with streaming</td></tr>
<tr><td><strong>Shield</strong></td><td>The <strong>immune system</strong>. Just like your body rejects viruses before they reach your organs, the Shield rejects malicious input before it reaches the Brain or Hands. It catches hackers the way white blood cells catch bacteria.</td><td>Grammar validation, injection detection, PII filtering</td></tr>
<tr><td><strong>Brain</strong></td><td>The <strong>brain</strong>. It thinks, plans, and decides. It talks to cloud LLMs (GPT-4, Claude, Gemini) to understand your request, then decides which tools to use. Like a human brain that can&rsquo;t lift heavy objects itself but tells the hands what to do.</td><td>LLM agent loop, tool dispatch, streaming, fallback chain</td></tr>
<tr><td><strong>Hands</strong></td><td>The <strong>hands</strong>. They do the actual work &mdash; reading files, running commands, searching the web, computing hashes. The Brain decides; the Hands execute. 63 tools, each a specialist.</td><td>63 compiled tool implementations</td></tr>
</table>

<div class="callout tip">
<div class="callout-title">Defense in Depth</div>
The Shield sits between <em>every</em> boundary &mdash; like airport security with multiple checkpoints. Input is scanned before the Brain sees it. Tool results are scanned before you see them. Even if one checkpoint fails, the next one catches the threat.
</div>
</section>

<section class="doc-section" id="directory-structure">
<h2>Directory Structure</h2>
<pre>seaclaw/
+-- include/seaclaw/          # 29 public headers
|   +-- sea_types.h           # Foundation types
|   +-- sea_arena.h           # Arena allocator
|   +-- sea_log.h / sea_json.h / sea_shield.h / sea_http.h
|   +-- sea_db.h / sea_config.h / sea_agent.h / sea_tools.h
|   +-- sea_telegram.h / sea_channel.h / sea_bus.h
|   +-- sea_api.h / sea_ws.h / sea_auth.h / sea_cron.h
|   +-- sea_heartbeat.h / sea_memory.h / sea_recall.h
|   +-- sea_skill.h / sea_graph.h / sea_mesh.h
|   +-- sea_session.h / sea_usage.h / sea_pii.h / sea_ext.h
|   +-- sea_a2a.h / sea_cli.h
+-- src/                      # 27 source directories
|   +-- core/ senses/ shield/ brain/ hands/ telegram/
|   +-- channels/ api/ ws/ bus/ auth/ cron/ heartbeat/
|   +-- memory/ recall/ skills/ graph/ mesh/ session/
|   +-- usage/ pii/ ext/ a2a/ cli/
|   +-- main.c               # Entry point (~2500 lines)
+-- seazero/bridge/           # Multi-agent (3 files)
+-- tests/                    # 26 test suites
+-- Makefile / Dockerfile / .env</pre>
</section>

<section class="doc-section" id="dependency-order">
<h2>Dependency Order</h2>
<p>Clean layering &mdash; lower layers never depend on higher layers. No circular includes.</p>
<div class="diagram">Level 0: sea_types.h          (no deps)
Level 1: sea_arena.h, sea_log.h
Level 2: sea_json.h, sea_http.h, sea_shield.h
Level 3: sea_db.h, sea_config.h
Level 4: sea_tools.h, sea_agent.h
Level 5: sea_telegram.h, sea_channel.h, sea_api.h
Level 6: sea_auth.h, sea_cron.h, sea_heartbeat.h, sea_mesh.h
Level 7: main.c (ALL headers)</div>
</section>

<section class="doc-section" id="memory-model">
<h2>Memory Model</h2>
<p>Sea-Claw uses <strong>zero malloc</strong>. All memory comes from arena allocators.</p>
<table>
<tr><th>Arena</th><th>Size</th><th>Purpose</th><th>Lifetime</th></tr>
<tr><td><code>s_session_arena</code></td><td>16 MB</td><td>Config, long-lived data</td><td>Entire program</td></tr>
<tr><td><code>s_request_arena</code></td><td>1 MB</td><td>Per-request: JSON, HTTP, tools</td><td>One message</td></tr>
</table>
<div class="diagram">Allocation (bump pointer):
  [data A][data B][data C][  free space  ]
                          ^-- offset

Reset (instant, one pointer move):
  [          all free space              ]
  ^-- offset = 0</div>
<table>
<tr><th>Problem</th><th>malloc/free</th><th>Arena</th></tr>
<tr><td>Memory leaks</td><td>Common</td><td><strong>Impossible</strong></td></tr>
<tr><td>Use-after-free</td><td>Common</td><td><strong>Impossible</strong></td></tr>
<tr><td>Fragmentation</td><td>Grows over time</td><td><strong>Zero</strong></td></tr>
<tr><td>Alloc speed</td><td>~50ns</td><td><strong>11ns</strong></td></tr>
</table>
<div class="callout tip">
<div class="callout-title">For Non-Technical Readers</div>
Think of malloc like a parking lot with random spots &mdash; gaps form (fragmentation) and cars get lost (leaks). An arena is a conveyor belt: everything goes on in order, rewind to start when done.
</div>
</section>

<section class="doc-section" id="security-model">
<h2>Security Model</h2>
<p>Six independent layers of defense:</p>
<h3>Layer 1: Grammar Shield</h3>
<p>Every input byte checked against a 256-bit bitmap. &lt;1&micro;s per check.</p>
<pre>if (!sea_shield_check(input, SEA_GRAMMAR_SAFE_TEXT)) {
    // REJECTED - invalid bytes
}</pre>
<h3>Layer 2: Injection Detection</h3>
<p>Pattern matching for <code>$(cmd)</code>, <code>\`cmd\`</code>, <code>; rm -rf</code>, SQL keywords, script tags.</p>
<h3>Layer 3: Tool-Level Validation</h3>
<p>Each tool validates its own arguments. File paths Shield-checked. SQL restricted to SELECT/PRAGMA.</p>
<h3>Layer 4: Static Tool Registry</h3>
<p>AI cannot invent new tools. All 63 are compiled function pointers. No <code>eval()</code>.</p>
<h3>Layer 5: Arena Memory Safety</h3>
<p>No malloc/free = no use-after-free, no double-free, no heap corruption.</p>
<h3>Layer 6: A2A Output Verification</h3>
<p>Remote agent responses Shield-validated before use.</p>

<h3>Real-World Attack Walkthrough</h3>
<p>Let&rsquo;s trace what happens when a hacker tries to sneak a command through Sea-Claw:</p>

<div class="flow-step"><div class="step-num">!</div><div class="step-text"><strong>Attacker sends:</strong> <code>What time is it? ; rm -rf /</code><br><em>The <code>; rm -rf /</code> part is a shell injection that would delete everything on a vulnerable system.</em></div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">1</div><div class="step-text"><strong>Layer 1 (Grammar Shield)</strong> scans every byte. The semicolon <code>;</code> followed by <code>rm</code> triggers the injection pattern detector.</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">&times;</div><div class="step-text"><strong>REJECTED.</strong> Sea-Claw returns: <em>&ldquo;Input rejected: potential injection detected.&rdquo;</em> The message <strong>never reaches the Brain, the LLM, or any tool</strong>. Total time: &lt;1 microsecond.</div></div>

<p style="margin-top:16px;">But what if a cleverer attacker encodes the injection to bypass Layer 1?</p>

<div class="flow-step"><div class="step-num">!</div><div class="step-text"><strong>Attacker sends:</strong> <code>Read the file at $(cat /etc/passwd)</code></div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">1</div><div class="step-text"><strong>Layer 1</strong> passes (no obvious shell metacharacters in the text).</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">2</div><div class="step-text"><strong>Layer 2 (Injection Detector)</strong> catches the <code>$(</code> pattern &mdash; this is a command substitution attempt. <strong>REJECTED.</strong></div></div>

<p style="margin-top:16px;">And if somehow both layers miss it?</p>

<div class="flow-step"><div class="step-num">3</div><div class="step-text"><strong>Layer 3 (Tool Validation)</strong> &mdash; The <code>file_read</code> tool validates the path through the Shield before opening. <code>$(cat /etc/passwd)</code> is not a valid filename.</div></div>
<div class="flow-step"><div class="step-num">4</div><div class="step-text"><strong>Layer 4 (Static Registry)</strong> &mdash; Even if the LLM hallucinates a tool name like <code>exec_shell_raw</code>, it doesn&rsquo;t exist in the 63-tool registry. Nothing happens.</div></div>

<div class="callout tip">
<div class="callout-title">The Key Takeaway</div>
<p>An attacker would need to bypass <strong>all six layers simultaneously</strong> to cause harm. Each layer is independent and uses a different detection method. This is like having a lock, an alarm, a guard dog, a moat, a drawbridge, AND a dragon &mdash; all protecting the same castle.</p>
</div>
</section>

<section class="doc-section" id="build-system">
<h2>Build System</h2>
<table>
<tr><th>Target</th><th>Command</th><th>Description</th></tr>
<tr><td>Debug</td><td><code>make all</code></td><td>With ASan + UBSan</td></tr>
<tr><td>Release</td><td><code>make release</code></td><td>-O3, LTO, stripped</td></tr>
<tr><td>Test</td><td><code>make test</code></td><td>26 test suites</td></tr>
<tr><td>Clean</td><td><code>make clean</code></td><td>Remove all artifacts</td></tr>
<tr><td>Install</td><td><code>make install</code></td><td>Copy to /usr/local/bin</td></tr>
</table>
<h3>Compiler Flags</h3>
<pre># Debug:   -std=c11 -Wall -Wextra -Werror -O0 -g -fsanitize=address,undefined
# Release: -std=c11 -Wall -Wextra -Werror -O3 -flto -fstack-protector-strong
# x86-64:  -mavx2 -mfma -DSEA_ARCH_X86
# ARM:     -DSEA_ARCH_ARM</pre>
<h3>Link Libraries</h3>
<pre>-lm -lcurl -lsqlite3 -lpthread -lreadline</pre>
</section>
`;
