// Part 5: Channels, SeaZero, Infrastructure, Use Cases, Edge Cases, Dev Guide, Appendix

var CONTENT_CHANNELS = `
<section class="doc-section" id="mod-telegram">
<h2>Telegram Bot</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_telegram.h</code></p>
<p>Long-polling Telegram bot. Receives messages, dispatches through handler, sends responses. Supports <code>deleteWebhook</code> to prevent HTTP 409 conflicts.</p>
<pre>SeaError sea_telegram_init(SeaTelegram* tg, const char* token, i64 chat_id,
                            SeaTelegramHandler handler, SeaArena* arena);
SeaError sea_telegram_poll(SeaTelegram* tg);
SeaError sea_telegram_send(SeaTelegram* tg, i64 chat_id, const char* text);
SeaError sea_telegram_delete_webhook(SeaTelegram* tg);</pre>
</section>

<section class="doc-section" id="mod-channel">
<h2>Channel Abstraction</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_channel.h</code></p>
<p>Abstract interface for messaging channels. Every channel implements the same vtable: <code>init</code>, <code>start</code>, <code>poll</code>, <code>send</code>, <code>stop</code>, <code>destroy</code>. The channel manager starts/stops all channels and routes messages through the bus.</p>
<pre>typedef struct {
    SeaError (*init)(SeaChannel* ch, SeaBus* bus, SeaArena* arena);
    SeaError (*start)(SeaChannel* ch);
    SeaError (*poll)(SeaChannel* ch);
    SeaError (*send)(SeaChannel* ch, i64 chat_id, const char* text, u32 len);
    void     (*stop)(SeaChannel* ch);
    void     (*destroy)(SeaChannel* ch);
} SeaChannelVTable;</pre>
</section>

<section class="doc-section" id="mod-slack">
<h2>Slack Webhook (E16)</h2>
<p>Outbound-only channel via Slack Incoming Webhooks. Set <code>SLACK_WEBHOOK_URL</code> in <code>.env</code>.</p>
<pre># .env
SLACK_WEBHOOK_URL=https://hooks.slack.com/services/T.../B.../xxx</pre>
<p>Sea-Claw will forward agent responses to your Slack channel automatically when running in gateway mode.</p>
</section>

<section class="doc-section" id="mod-ws">
<h2>WebSocket Server</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_ws.h</code></p>
<p>Built-in WebSocket server for real-time browser clients. Enables custom web dashboards that communicate with Sea-Claw bidirectionally.</p>
</section>

<section class="doc-section" id="mod-api">
<h2>HTTP API Server (E17)</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_api.h</code></p>
<p>Lightweight REST endpoint for external integrations. Listens on localhost only.</p>
<table>
<tr><th>Method</th><th>Path</th><th>Description</th></tr>
<tr><td>POST</td><td><code>/api/chat</code></td><td>Send message, get agent response</td></tr>
<tr><td>GET</td><td><code>/api/health</code></td><td>Health check + version</td></tr>
<tr><td>OPTIONS</td><td><code>*</code></td><td>CORS preflight</td></tr>
</table>
<h3>Example</h3>
<pre>curl -X POST http://localhost:8899/api/chat \\
  -H "Content-Type: application/json" \\
  -d '{"message":"What is the system uptime?"}'

# Response:
{"response":"Up for 3 days, 14 hours.","tool_calls":1}</pre>
<p>Enable with <code>SEA_API_PORT=8899</code> in <code>.env</code> or automatically in gateway mode.</p>
</section>

<section class="doc-section" id="mod-bus">
<h2>Message Bus</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_bus.h</code></p>
<p>Internal pub/sub bus. Channels publish inbound messages; agent consumes, processes, publishes outbound. Dispatch thread routes outbound to correct channel.</p>
</section>

<section class="doc-section" id="tui-commands">
<h2>TUI Commands (30+)</h2>
<p>All commands start with <code>/</code>. Type <code>/help</code> for the full list.</p>
<table>
<tr><th>Command</th><th>Description</th></tr>
<tr><td><code>/help</code></td><td>Full command reference</td></tr>
<tr><td><code>/status</code></td><td>System status &amp; memory</td></tr>
<tr><td><code>/tools</code></td><td>List all 63 tools</td></tr>
<tr><td><code>/tasks</code></td><td>List tasks</td></tr>
<tr><td><code>/config</code></td><td>Show configuration</td></tr>
<tr><td><code>/model [name]</code></td><td>View/change LLM model</td></tr>
<tr><td><code>/provider [name]</code></td><td>View/change provider</td></tr>
<tr><td><code>/stream on|off</code></td><td>Toggle SSE streaming</td></tr>
<tr><td><code>/think &lt;level&gt;</code></td><td>Think level (off/low/medium/high)</td></tr>
<tr><td><code>/agents</code></td><td>List Agent Zero instances</td></tr>
<tr><td><code>/delegate &lt;task&gt;</code></td><td>Delegate to Agent Zero</td></tr>
<tr><td><code>/seazero</code></td><td>Multi-agent dashboard</td></tr>
<tr><td><code>/usage</code></td><td>Token usage breakdown</td></tr>
<tr><td><code>/audit</code></td><td>Security events</td></tr>
<tr><td><code>/skills</code></td><td>Learned skills</td></tr>
<tr><td><code>/auth</code></td><td>Token auth management</td></tr>
<tr><td><code>/heartbeat</code></td><td>Health monitor</td></tr>
<tr><td><code>/graph</code></td><td>Knowledge graph ops</td></tr>
<tr><td><code>/cron</code></td><td>Cron scheduler</td></tr>
<tr><td><code>/recall</code></td><td>Fact memory ops</td></tr>
<tr><td><code>/mesh</code></td><td>Mesh network status</td></tr>
<tr><td><code>/utests [sprint]</code></td><td>Usability test results</td></tr>
<tr><td><code>/clear</code></td><td>Clear screen</td></tr>
<tr><td><code>/quit</code></td><td>Exit Sea-Claw</td></tr>
</table>
</section>

<section class="doc-section" id="seazero-overview">
<h2>What is SeaZero?</h2>
<p>SeaZero is Sea-Claw's <strong>multi-agent orchestration layer</strong>. It lets Sea-Claw delegate complex tasks to Agent Zero (a Python-based autonomous agent) running in a Docker container, while keeping Sea-Claw as the sovereign controller.</p>
<div class="diagram">Sea-Claw (C11, sovereign)
    |
    +-- SeaZero Bridge (HTTP IPC)
    |       |
    |       +-- Agent Zero (Python, Docker)
    |       |       |
    |       |       +-- LLM Proxy (port 7432)
    |       |       |   (Sea-Claw controls budget)
    |       |       |
    |       |       +-- Workspace (isolated /tmp)
    |       |
    |       +-- Agent Zero #2 (optional)
    |
    +-- Other Sea-Claw instances (A2A protocol)</div>
<h3>Key Design Principles</h3>
<ul>
  <li><strong>Sea-Claw stays sovereign</strong> &mdash; No new dependencies, uses existing sea_http</li>
  <li><strong>Agent Zero stays isolated</strong> &mdash; Docker container, capability-dropped</li>
  <li><strong>Bridge is stateless</strong> &mdash; Each call is an independent HTTP request</li>
  <li><strong>All responses Shield-validated</strong> &mdash; Before reaching the user</li>
</ul>
</section>

<section class="doc-section" id="seazero-bridge">
<h2>SeaZero Bridge API</h2>
<p><span class="badge badge-blue">Header</span> <code>seazero/bridge/sea_zero.h</code></p>
<pre>// Initialize
bool sea_zero_init(SeaZeroConfig* cfg, const char* agent_url);

// Delegate a task
SeaZeroResult sea_zero_delegate(const SeaZeroConfig* cfg,
                                 const SeaZeroTask* task, SeaArena* arena);

// Health check
SeaZeroHealth sea_zero_health(const SeaZeroConfig* cfg, SeaArena* arena);

// Register as LLM tool
int sea_zero_register_tool(const SeaZeroConfig* cfg);</pre>
</section>

<section class="doc-section" id="seazero-proxy">
<h2>SeaZero LLM Proxy</h2>
<p><span class="badge badge-blue">Header</span> <code>seazero/bridge/sea_proxy.h</code></p>
<p>Lightweight HTTP server on port 7432 that proxies Agent Zero's LLM requests through Sea-Claw. Validates internal token, checks daily budget, forwards to real LLM, logs usage.</p>
<pre>int  sea_proxy_start(const SeaProxyConfig* cfg);
void sea_proxy_stop(void);
bool sea_proxy_running(void);</pre>
</section>

<section class="doc-section" id="seazero-workspace">
<h2>Workspace Isolation</h2>
<p><span class="badge badge-blue">Header</span> <code>seazero/bridge/sea_workspace.h</code></p>
<p>Each Agent Zero instance gets an isolated workspace directory. Sea-Claw manages creation, cleanup, and file exchange between workspaces.</p>
</section>

<section class="doc-section" id="seazero-docker">
<h2>Docker Orchestration</h2>
<p>SeaZero uses Docker to run Agent Zero instances. Each agent gets its own container with:</p>
<ul>
  <li>Isolated filesystem (bind-mounted workspace)</li>
  <li>Network access only to Sea-Claw's LLM proxy</li>
  <li>Capability-dropped (no privileged operations)</li>
  <li>Resource limits (CPU, memory)</li>
</ul>
<pre># Spawn an Agent Zero instance
/delegate "Research the latest Rust async features and summarize"

# Check status
/seazero</pre>
</section>

<section class="doc-section" id="mod-auth">
<h2>Authentication &amp; Tokens</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_auth.h</code></p>
<p>SQLite-backed token authentication. Generate, validate, revoke API tokens. Used for HTTP API and WebSocket authentication.</p>
</section>

<section class="doc-section" id="mod-cron">
<h2>Cron Scheduler</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_cron.h</code></p>
<p>Built-in cron scheduler. Schedule recurring tasks with cron expressions. Jobs persist in DB and survive restarts.</p>
<pre>/cron add "0 9 * * *" "Check disk usage and alert if above 80%"</pre>
</section>

<section class="doc-section" id="mod-heartbeat">
<h2>Heartbeat Monitor</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_heartbeat.h</code></p>
<p>Periodic health checks. Logs system state to DB. Can trigger proactive agent actions (e.g., "disk is 90% full, should I clean up?").</p>
</section>

<section class="doc-section" id="mod-mesh">
<h2>Mesh Networking</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_mesh.h</code></p>
<p>Connect multiple Sea-Claw instances into a mesh network. Nodes discover each other, share capabilities, and can delegate tasks across the mesh.</p>
</section>

<section class="doc-section" id="mod-session">
<h2>Session Management</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_session.h</code></p>
<p>Per-chat session state. Tracks conversation context, active tools, and user preferences within a session.</p>
</section>

<section class="doc-section" id="mod-usage">
<h2>Usage Tracking</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_usage.h</code></p>
<p>Tracks LLM token usage per provider/model. View with <code>/usage</code> command. Persists in DB for cost analysis.</p>
</section>

<section class="doc-section" id="mod-pii">
<h2>PII Detection</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_pii.h</code></p>
<p>Detects personally identifiable information (emails, phone numbers, SSNs) in text. Can be used to redact PII before sending to cloud LLMs.</p>
</section>

<section class="doc-section" id="mod-ext">
<h2>Extension System</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_ext.h</code></p>
<p>Plugin-style extension registry. Extensions can register custom tools, commands, and event handlers at startup.</p>
</section>

<section class="doc-section" id="mod-a2a">
<h2>Agent-to-Agent (A2A)</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_a2a.h</code></p>
<p>Delegate tasks to remote agents via HTTP JSON-RPC. Shield-verifies results before returning.</p>
<pre>SeaA2aResult sea_a2a_delegate(const SeaA2aPeer* peer, const SeaA2aRequest* req, SeaArena* arena);
bool         sea_a2a_heartbeat(const SeaA2aPeer* peer, SeaArena* arena);
i32          sea_a2a_discover(const char* url, SeaA2aPeer* out, i32 max, SeaArena* arena);</pre>
</section>

<section class="doc-section" id="uc-personal">
<h2>Use Case: Personal AI Assistant</h2>
<p>Run Sea-Claw on your laptop or desktop. Talk to it via the terminal. Ask it to manage files, search the web, schedule reminders, and answer questions &mdash; all while keeping your data local.</p>
<pre>&gt; Read my TODO.md and tell me what's overdue
&gt; Search the web for "Rust async best practices 2025" and summarize
&gt; Schedule a reminder every Monday at 9am to check server logs</pre>
</section>

<section class="doc-section" id="uc-devops">
<h2>Use Case: DevOps Automation</h2>
<p>Deploy Sea-Claw on your server. Monitor via Telegram. Get alerts, run diagnostics, and fix issues from your phone.</p>
<pre>/status              # System health from your phone
/shell df -h         # Check disk usage remotely
/delegate "Check all SSL certificates and report expiring ones"</pre>
</section>

<section class="doc-section" id="uc-iot">
<h2>Use Case: IoT &amp; Raspberry Pi</h2>
<p>Run Sea-Claw on a Pi as a 24/7 home automation hub. Control smart devices, monitor sensors, and get alerts via Telegram.</p>
<pre># Example: temperature monitoring
&gt; Read /sys/class/thermal/thermal_zone0/temp and convert to Celsius
&gt; If above 70C, send me a Telegram alert</pre>
</section>

<section class="doc-section" id="uc-enterprise">
<h2>Use Case: Enterprise Deployment</h2>
<p>On-premise AI with full audit trail. Every action logged to SQLite. Token authentication for API access. PII detection before cloud LLM calls. Mesh networking for multi-node deployments.</p>
</section>

<section class="doc-section" id="uc-education">
<h2>Use Case: Education &amp; Research</h2>
<p>Clean C11 codebase for learning systems programming. Each module is self-contained with clear APIs. 26 test suites demonstrate usage patterns. Zero external dependencies beyond libc, libcurl, SQLite.</p>
</section>

<section class="doc-section" id="uc-multi-agent">
<h2>Use Case: Multi-Agent Workflows</h2>
<p>Use SeaZero to orchestrate complex tasks across multiple agents. Sea-Claw delegates research to Agent Zero, code review to another instance, and synthesizes results.</p>
<pre>/delegate "Research competitor pricing and create a comparison table"
/seazero   # Monitor agent status
/sztasks   # View task history</pre>
</section>

<section class="doc-section" id="edge-memory">
<h2>Edge Case: Memory Limits</h2>
<p><strong>Q: What happens when the arena is full?</strong></p>
<p>A: <code>sea_arena_alloc</code> returns <code>NULL</code>. The caller checks and returns <code>SEA_ERR_ARENA_FULL</code>. The request arena resets after each message, so this only happens with extremely large single responses (&gt;1 MB). Increase <code>arena_size_mb</code> in config if needed.</p>
<p><strong>Q: Can I run with less than 16 MB?</strong></p>
<p>A: Yes. Set <code>arena_size_mb</code> to 4 for constrained devices (Pi Zero). Most requests use &lt;100 KB.</p>
</section>

<section class="doc-section" id="edge-security">
<h2>Edge Case: Security</h2>
<p><strong>Q: What if the LLM tries to inject shell commands?</strong></p>
<p>A: The Grammar Shield catches injection patterns before they reach any tool. Even if a pattern slips through Layer 2, Layer 4 (static registry) means the AI can only call pre-compiled tools &mdash; it cannot execute arbitrary code.</p>
<p><strong>Q: What about prompt injection?</strong></p>
<p>A: Sea-Claw's tool calls are parsed from structured JSON, not from free text. The LLM must produce valid JSON with a tool name that exists in the registry. Prompt injection that produces invalid JSON or unknown tool names is simply ignored.</p>
</section>

<section class="doc-section" id="edge-network">
<h2>Edge Case: Network Failures</h2>
<p><strong>Q: What if the LLM API is down?</strong></p>
<p>A: The fallback chain kicks in. If OpenRouter fails, Sea-Claw tries OpenAI, then Gemini. If all fail, the user gets a clear error message. Local tools (file_read, shell_exec, etc.) still work without any network.</p>
<p><strong>Q: What about Telegram disconnections?</strong></p>
<p>A: The polling loop has automatic retry with 5-second backoff. <code>deleteWebhook</code> is called at startup to prevent 409 conflicts.</p>
</section>

<section class="doc-section" id="edge-concurrency">
<h2>Edge Case: Concurrency</h2>
<p><strong>Q: Is Sea-Claw thread-safe?</strong></p>
<p>A: Each thread gets its own arena. The message bus uses mutex-protected queues. The API server handles one request at a time (sequential accept loop). For high-throughput scenarios, use gateway mode with the bus architecture.</p>
</section>

<section class="doc-section" id="faq">
<h2>Frequently Asked Questions</h2>
<h3>General</h3>
<p><strong>Q: Do I need to know C to use Sea-Claw?</strong><br>A: No. You interact via Telegram, terminal, or HTTP API. C knowledge is only needed to modify or extend Sea-Claw.</p>
<p><strong>Q: Is Sea-Claw free?</strong><br>A: Yes. The code is open source. You only pay for LLM API usage (many providers have free tiers).</p>
<p><strong>Q: Can I use it without internet?</strong><br>A: Partially. Local tools work offline. The LLM brain requires internet access to the API provider.</p>
<h3>Technical</h3>
<p><strong>Q: Why C instead of Rust?</strong><br>A: C11 compiles everywhere, has zero runtime overhead, and produces the smallest binaries. Rust's borrow checker adds complexity without benefit when using arena allocation (which already prevents memory bugs).</p>
<p><strong>Q: Can I add my own tools?</strong><br>A: Yes. Create a .c file in <code>src/hands/impl/</code>, register it in <code>sea_tools.c</code>, add to Makefile. See the Developer Guide.</p>
<p><strong>Q: How do I update?</strong><br>A: <code>git pull &amp;&amp; make release</code>. Your database and config are preserved.</p>
</section>

<section class="doc-section" id="dev-add-tool">
<h2>Developer Guide: Adding a New Tool</h2>
<h3>Step 1: Create the Implementation</h3>
<pre>// src/hands/impl/tool_hello.c
#include "seaclaw/sea_types.h"
#include "seaclaw/sea_arena.h"
#include &lt;stdio.h&gt;
#include &lt;string.h&gt;

SeaError tool_hello(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "Hello, %.*s!",
                       (int)args.len, (const char*)args.data);
    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst;
    output->len = (u32)len;
    return SEA_OK;
}</pre>
<h3>Step 2: Register in sea_tools.c</h3>
<pre>extern SeaError tool_hello(SeaSlice args, SeaArena* arena, SeaSlice* output);

// In the tools array:
{64, "hello", "Greet someone. Args: name", tool_hello},</pre>
<h3>Step 3: Add to Makefile</h3>
<pre>HANDS_SRC := \\
    ...
    src/hands/impl/tool_hello.c</pre>
<h3>Step 4: Build and Test</h3>
<pre>make all &amp;&amp; make test</pre>
</section>

<section class="doc-section" id="dev-add-channel">
<h2>Developer Guide: Adding a New Channel</h2>
<p>Implement the <code>SeaChannelVTable</code> interface. See <code>src/channels/channel_slack.c</code> for a minimal example (outbound-only) or <code>src/channels/channel_telegram.c</code> for a full bidirectional channel.</p>
</section>

<section class="doc-section" id="dev-add-provider">
<h2>Developer Guide: Adding an LLM Provider</h2>
<p>Add a new <code>SEA_LLM_*</code> enum value in <code>sea_agent.h</code>, implement the request JSON builder in <code>sea_agent.c</code>, and add the API URL/auth header logic. Most OpenAI-compatible providers work with the existing <code>build_request_json</code> function.</p>
</section>

<section class="doc-section" id="dev-testing">
<h2>Developer Guide: Writing Tests</h2>
<p>Tests use a minimal framework defined in each test file. Pattern:</p>
<pre>static int test_something(void) {
    // Arrange
    SeaArena arena;
    sea_arena_create(&amp;arena, 4096);

    // Act
    void* p = sea_arena_alloc(&amp;arena, 100, 8);

    // Assert
    ASSERT(p != NULL);
    ASSERT(sea_arena_used(&amp;arena) >= 100);

    sea_arena_destroy(&amp;arena);
    return 0;  // 0 = pass
}</pre>
</section>

<section class="doc-section" id="dev-debugging">
<h2>Developer Guide: Debugging Tips</h2>
<ul>
  <li><strong>AddressSanitizer</strong>: Debug builds include ASan. Run normally &mdash; it reports memory errors automatically.</li>
  <li><strong>Log levels</strong>: Set <code>"log_level": "debug"</code> in config.json for verbose output.</li>
  <li><strong>Arena usage</strong>: Check <code>sea_arena_usage_pct()</code> to detect near-full arenas.</li>
  <li><strong>SQLite inspection</strong>: Open <code>seaclaw.db</code> with any SQLite tool to inspect state.</li>
</ul>
</section>

<section class="doc-section" id="dev-contributing">
<h2>Developer Guide: Contributing</h2>
<ol>
  <li>Fork the repository</li>
  <li>Create a feature branch: <code>git checkout -b feature/my-feature</code></li>
  <li>Make changes, ensure <code>make test</code> passes with 0 failures</li>
  <li>Ensure <code>make release</code> builds with 0 warnings</li>
  <li>Submit a pull request to <code>develop</code></li>
</ol>
</section>

<section class="doc-section" id="error-codes">
<h2>Appendix: Error Codes Reference</h2>
<table>
<tr><th>Code</th><th>Name</th><th>Description</th></tr>
<tr><td>0</td><td><code>SEA_OK</code></td><td>Success</td></tr>
<tr><td>1</td><td><code>SEA_ERR_OOM</code></td><td>Out of memory</td></tr>
<tr><td>2</td><td><code>SEA_ERR_ARENA_FULL</code></td><td>Arena allocator full</td></tr>
<tr><td>3</td><td><code>SEA_ERR_IO</code></td><td>I/O error</td></tr>
<tr><td>4</td><td><code>SEA_ERR_EOF</code></td><td>End of file/stream</td></tr>
<tr><td>5</td><td><code>SEA_ERR_TIMEOUT</code></td><td>Operation timed out</td></tr>
<tr><td>6</td><td><code>SEA_ERR_PARSE</code></td><td>Parse error</td></tr>
<tr><td>7</td><td><code>SEA_ERR_INVALID_JSON</code></td><td>Invalid JSON</td></tr>
<tr><td>8</td><td><code>SEA_ERR_INVALID_INPUT</code></td><td>Invalid user input</td></tr>
<tr><td>9</td><td><code>SEA_ERR_GRAMMAR_REJECT</code></td><td>Grammar Shield rejected</td></tr>
<tr><td>10</td><td><code>SEA_ERR_TOOL_NOT_FOUND</code></td><td>Tool not in registry</td></tr>
<tr><td>11</td><td><code>SEA_ERR_TOOL_FAILED</code></td><td>Tool execution failed</td></tr>
<tr><td>12</td><td><code>SEA_ERR_CONFIG</code></td><td>Configuration error</td></tr>
<tr><td>13</td><td><code>SEA_ERR_DB</code></td><td>Database error</td></tr>
<tr><td>14</td><td><code>SEA_ERR_AUTH</code></td><td>Authentication failed</td></tr>
<tr><td>15</td><td><code>SEA_ERR_RATE_LIMIT</code></td><td>Rate limited</td></tr>
</table>
</section>

<section class="doc-section" id="grammar-types">
<h2>Appendix: Grammar Types Reference</h2>
<table>
<tr><th>Type</th><th>Allowed Characters</th><th>Use Case</th></tr>
<tr><td><code>SAFE_TEXT</code></td><td>Printable ASCII (32-126), newline, tab</td><td>User messages</td></tr>
<tr><td><code>NUMERIC</code></td><td>0-9, ., -, +, e, E</td><td>Numbers</td></tr>
<tr><td><code>ALPHA</code></td><td>a-z, A-Z</td><td>Names</td></tr>
<tr><td><code>ALPHANUM</code></td><td>a-z, A-Z, 0-9</td><td>Identifiers</td></tr>
<tr><td><code>FILENAME</code></td><td>Alphanumeric, ., -, _, /</td><td>File paths</td></tr>
<tr><td><code>URL</code></td><td>URL-safe characters</td><td>URLs</td></tr>
<tr><td><code>JSON</code></td><td>Valid JSON characters</td><td>JSON data</td></tr>
<tr><td><code>COMMAND</code></td><td>/, alphanumeric, space</td><td>Commands</td></tr>
<tr><td><code>HEX</code></td><td>0-9, a-f, A-F</td><td>Hex strings</td></tr>
<tr><td><code>BASE64</code></td><td>A-Z, a-z, 0-9, +, /, =</td><td>Encoded data</td></tr>
</table>
</section>

<section class="doc-section" id="db-schema">
<h2>Appendix: Database Schema</h2>
<pre>-- Core tables (auto-created)
CREATE TABLE trajectory (
    id INTEGER PRIMARY KEY, entry_type TEXT, title TEXT,
    content TEXT, created_at TEXT DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE config (key TEXT PRIMARY KEY, value TEXT);
CREATE TABLE tasks (
    id INTEGER PRIMARY KEY, title TEXT, status TEXT DEFAULT 'pending',
    priority TEXT DEFAULT 'medium', content TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP
);
CREATE TABLE chat_history (
    id INTEGER PRIMARY KEY, chat_id INTEGER, role TEXT,
    content TEXT, created_at TEXT DEFAULT CURRENT_TIMESTAMP
);

-- Usability testing (E13-E17)
CREATE TABLE usability_tests (
    id INTEGER PRIMARY KEY, sprint TEXT, test_name TEXT,
    category TEXT, status TEXT, input TEXT, expected TEXT,
    actual TEXT, latency_ms INTEGER, error TEXT, env TEXT,
    created_at TEXT DEFAULT CURRENT_TIMESTAMP,
    finished_at TEXT
);

-- SeaZero tables
CREATE TABLE seazero_agents (...);
CREATE TABLE seazero_tasks (...);
CREATE TABLE seazero_llm_usage (...);</pre>
</section>

<section class="doc-section" id="api-reference">
<h2>Appendix: API Endpoint Reference</h2>
<h3>HTTP API (port 8899)</h3>
<table>
<tr><th>Endpoint</th><th>Method</th><th>Request</th><th>Response</th></tr>
<tr><td><code>/api/chat</code></td><td>POST</td><td><code>{"message":"..."}</code></td><td><code>{"response":"...","tool_calls":N}</code></td></tr>
<tr><td><code>/api/health</code></td><td>GET</td><td>&mdash;</td><td><code>{"status":"ok","version":"..."}</code></td></tr>
</table>
<h3>SeaZero LLM Proxy (port 7432)</h3>
<table>
<tr><th>Endpoint</th><th>Method</th><th>Description</th></tr>
<tr><td><code>/v1/chat/completions</code></td><td>POST</td><td>OpenAI-compatible proxy</td></tr>
<tr><td><code>/health</code></td><td>GET</td><td>Proxy health check</td></tr>
</table>
</section>

<section class="doc-section" id="changelog">
<h2>Appendix: Changelog</h2>
<h3>v1.0.0 (Current)</h3>
<ul>
  <li><strong>E1-E4</strong>: Core substrate (arena, log, JSON, shield, HTTP, DB, config)</li>
  <li><strong>E5-E8</strong>: Intelligence (agent, tools, Telegram, A2A)</li>
  <li><strong>E9-E12</strong>: Infrastructure (auth, cron, heartbeat, mesh, graph, WS, extensions, CLI, usage, PII, recall, skills, memory, channels, bus, session)</li>
  <li><strong>E13</strong>: SSE streaming parser (live tested)</li>
  <li><strong>E14</strong>: Telegram deleteWebhook + anti-409 fix</li>
  <li><strong>E15</strong>: /seazero multi-agent dashboard</li>
  <li><strong>E16</strong>: Slack webhook channel</li>
  <li><strong>E17</strong>: HTTP API server</li>
  <li><strong>E18-E20</strong>: Documentation site</li>
</ul>
</section>

<section class="doc-section" id="license">
<h2>Appendix: License</h2>
<p>Sea-Claw is open source software. See the LICENSE file in the repository for full terms.</p>
<p>Copyright &copy; 2024-2026 t4tarzan. All rights reserved.</p>
</section>
`;
