// Part 3: Architecture sections

var CONTENT_ARCH = `
<section class="doc-section" id="five-pillars">
<h2>The Five Pillars</h2>
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
<tr><th>Pillar</th><th>Directory</th><th>Purpose</th><th>Analogy</th></tr>
<tr><td><strong>Substrate</strong></td><td><code>src/core/</code></td><td>Memory, logging, database, config</td><td>Skeleton &amp; nervous system</td></tr>
<tr><td><strong>Senses</strong></td><td><code>src/senses/</code></td><td>JSON parsing, HTTP client, SSE</td><td>Eyes &amp; ears</td></tr>
<tr><td><strong>Shield</strong></td><td><code>src/shield/</code></td><td>Grammar validation, injection detection</td><td>Immune system</td></tr>
<tr><td><strong>Brain</strong></td><td><code>src/brain/</code></td><td>LLM agent loop, tool dispatch</td><td>The brain</td></tr>
<tr><td><strong>Hands</strong></td><td><code>src/hands/</code></td><td>63 tool implementations</td><td>The hands</td></tr>
</table>
<h3>Data Flow</h3>
<pre>User Input -> Channel -> Shield (validate) -> Brain (think)
  -> Brain calls Tool -> Hands (execute) -> Brain (interpret)
  -> Shield (validate output) -> Channel -> User</pre>
<div class="callout tip">
<div class="callout-title">Defense in Depth</div>
The Shield sits between <em>every</em> boundary. Input validated before Brain sees it. Output validated before user sees it. Even if one layer fails, the next catches it.
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
