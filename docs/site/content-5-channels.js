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
<p>Deploy Sea-Claw on your server. Monitor via Telegram. Get alerts, run diagnostics, and fix issues &mdash; all from your phone, anywhere in the world.</p>

<h3>Scenario: 2 AM Server Alert</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">My monitoring says server-3 is slow. What&rsquo;s going on?</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Checking server-3...<br><br>CPU: 94% (high)<br>Top process: <code>postgres</code> using 89% CPU<br>Disk: 92% full &mdash; <code>/var/log</code> is 4.2 GB<br><br>The database is thrashing because the disk is almost full. I recommend:<br>1. Rotate and compress old logs<br>2. Run <code>VACUUM</code> on PostgreSQL<br><br>Want me to clean up the logs now?</div></div>
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">Yes, clean up logs older than 7 days</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Done. Removed 3.8 GB of old logs. Disk now at 54%. CPU already dropping &mdash; currently 23%.</div></div>
</div>

<h3>Scenario: SSL Certificate Check</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">Check SSL certs for all my domains and tell me if any expire within 30 days</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Checked 5 domains:<br><br>example.com &mdash; expires in 142 days<br>api.example.com &mdash; expires in 142 days<br>staging.example.com &mdash; <strong>expires in 12 days</strong><br>docs.example.com &mdash; expires in 89 days<br>blog.example.com &mdash; expires in 89 days<br><br>Action needed: <code>staging.example.com</code> expires in 12 days. Want me to set up a cron job to remind you in 5 days?</div></div>
</div>
</section>

<section class="doc-section" id="uc-iot">
<h2>Use Case: IoT &amp; Raspberry Pi</h2>
<p>Run Sea-Claw on a Pi as a 24/7 home automation hub. Control smart devices, monitor sensors, and get alerts via Telegram &mdash; all for ~$5/year in electricity.</p>

<h3>Scenario: Home Temperature Monitoring</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">What&rsquo;s the CPU temperature on the Pi?</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">CPU temperature: 52.3&deg;C (normal range). Ambient: ~24&deg;C based on offset. The Pi is running cool.</div></div>
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">Set up a cron job to alert me if it goes above 70C</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Created cron job: every 15 minutes, check CPU temp. If &gt;70&deg;C, send Telegram alert. Job ID: cron-7.<br><br>You can manage it with <code>/cron list</code> or <code>/cron delete 7</code>.</div></div>
</div>

<h3>Scenario: Smart Home Control</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">Turn on the garden lights</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Sent HIGH signal to GPIO pin 17. Garden lights are ON.<br>Want me to set a timer to turn them off at 11 PM?</div></div>
</div>
</section>

<section class="doc-section" id="uc-enterprise">
<h2>Use Case: Enterprise Deployment</h2>
<p>On-premise AI with full audit trail. Every action logged to SQLite. Token authentication for API access. PII detection before cloud LLM calls.</p>

<h3>Scenario: Compliance Audit</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">Show me all AI actions from the last 24 hours</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Last 24 hours: 47 actions logged.<br><br>Tools used: file_read (12), shell_exec (8), web_search (6), task_manage (5), db_query (4), others (12)<br>LLM tokens: 24,830 (OpenRouter: 18,200, Gemini fallback: 6,630)<br>Security events: 0 rejections, 0 injection attempts<br>PII detections: 2 (redacted before LLM call)<br><br>Full audit trail available at: <code>SELECT * FROM trajectory WHERE timestamp &gt; datetime('now', '-1 day')</code></div></div>
</div>

<div class="callout info">
<div class="callout-title">Enterprise Features</div>
<ul>
  <li><strong>Audit trail</strong> &mdash; Every action logged with timestamp, tool name, input/output, and user ID</li>
  <li><strong>PII filter</strong> &mdash; Detects emails, phone numbers, SSNs before sending to cloud LLMs</li>
  <li><strong>Token auth</strong> &mdash; HTTP API requires bearer token for access</li>
  <li><strong>Mesh networking</strong> &mdash; Connect multiple Sea-Claw instances across departments</li>
  <li><strong>Usage tracking</strong> &mdash; Per-provider, per-day token usage with cost estimates</li>
</ul>
</div>
</section>

<section class="doc-section" id="uc-education">
<h2>Use Case: Education &amp; Research</h2>
<p>Sea-Claw is an excellent learning resource for systems programming. The codebase is clean, well-structured, and demonstrates real-world patterns.</p>

<h3>What Students Can Learn</h3>
<table>
<tr><th>Topic</th><th>Sea-Claw Module</th><th>What It Teaches</th></tr>
<tr><td>Memory management</td><td><code>sea_arena</code></td><td>Arena allocation vs. malloc/free, bump pointers, memory-mapped files</td></tr>
<tr><td>Parsing</td><td><code>sea_json</code></td><td>Zero-copy parsing, recursive descent, SeaSlice views</td></tr>
<tr><td>Networking</td><td><code>sea_http</code></td><td>HTTP client with libcurl, SSL, streaming (SSE)</td></tr>
<tr><td>Databases</td><td><code>sea_db</code></td><td>SQLite integration, schema migration, WAL mode</td></tr>
<tr><td>Security</td><td><code>sea_shield</code></td><td>Input validation, injection detection, grammar-based filtering</td></tr>
<tr><td>Concurrency</td><td><code>sea_bus</code></td><td>Thread-safe message passing, mutex-protected queues</td></tr>
<tr><td>Testing</td><td><code>tests/</code></td><td>26 test suites with assertion macros, edge case coverage</td></tr>
</table>

<h3>Scenario: Data Processing for Research</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">I have 200 CSV files in /data/experiment/. Parse each one, compute the mean and standard deviation of column 3, and create a summary CSV.</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Processing 200 files...<br><br>Done. Summary saved to <code>/data/experiment/summary.csv</code>.<br>Overall mean: 42.7 &plusmn; 3.2<br>Range: 31.4 to 58.9<br>3 files had missing values in column 3 (skipped those rows).</div></div>
</div>
</section>

<section class="doc-section" id="uc-multi-agent">
<h2>Use Case: Multi-Agent Workflows</h2>
<p>Use <span class="glossary" data-def="Sea-Claw&rsquo;s multi-agent orchestration layer. Delegates tasks to Agent Zero (Python) running in Docker.">SeaZero</span> to orchestrate complex tasks. Sea-Claw stays responsive while Agent Zero does heavy lifting in an isolated Docker container.</p>

<h3>Scenario: Competitive Research</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">/delegate Research the top 5 AI coding assistants in 2025, compare features, pricing, and user reviews. Create a detailed report.</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Delegated to Agent Zero. Task ID: SZ-051.<br>Agent Zero will browse the web, collect data, and compile the report in its Docker workspace.<br><br>I&rsquo;ll notify you when it&rsquo;s done. Meanwhile, I&rsquo;m still available for other tasks.</div></div>
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">/sztasks</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">SeaZero Tasks:<br>SZ-051 | IN_PROGRESS | &ldquo;Research AI coding assistants&rdquo; | 4 min elapsed<br>SZ-050 | COMPLETED | &ldquo;Rust frameworks comparison&rdquo; | 12 min<br>SZ-049 | COMPLETED | &ldquo;Security audit report&rdquo; | 8 min</div></div>
</div>

<div class="callout tip">
<div class="callout-title">Why Multi-Agent?</div>
<p>Some tasks take minutes (web research, code analysis, report generation). Without multi-agent, Sea-Claw would be blocked during that time. With SeaZero, it delegates the heavy work to Agent Zero and stays responsive for your other questions. It&rsquo;s like a manager who assigns tasks to team members instead of doing everything themselves.</p>
</div>
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

<section class="doc-section" id="edge-power">
<h2>Edge Case: Power Loss &amp; Crashes</h2>
<p><strong>Q: What if my Pi loses power mid-conversation?</strong></p>
<p>A: SQLite uses <span class="glossary" data-def="Write-Ahead Logging &mdash; a SQLite feature that writes changes to a separate log file first, then applies them to the database. If power is lost, the log is replayed on restart.">WAL mode</span>, which is crash-safe. Your chat history, tasks, and config survive power loss. The arena memory is volatile (RAM only), so the current in-flight request is lost &mdash; but that&rsquo;s just one message. On restart, Sea-Claw picks up right where it left off.</p>
<p><strong>Q: What if the database file gets corrupted?</strong></p>
<p>A: SQLite is extremely resilient (used in aircraft, phones, and browsers). In the rare event of corruption, <code>sqlite3 seaclaw.db ".recover"</code> can salvage data. Sea-Claw also auto-creates a fresh database if the file is missing, so you can always start clean.</p>
</section>

<section class="doc-section" id="edge-llm-garbage">
<h2>Edge Case: LLM Returns Garbage</h2>
<p><strong>Q: What if the LLM returns invalid JSON?</strong></p>
<p>A: The JSON parser returns <code>SEA_ERR_INVALID_JSON</code>. The agent retries once with a &ldquo;please respond with valid JSON&rdquo; prompt. If it fails again, you get a clear error: &ldquo;LLM returned unparseable response.&rdquo; No crash, no undefined behavior.</p>
<p><strong>Q: What if the LLM hallucinates a tool that doesn&rsquo;t exist?</strong></p>
<p>A: The static registry lookup returns <code>SEA_ERR_TOOL_NOT_FOUND</code>. The agent tells the LLM: &ldquo;Tool X does not exist. Available tools are: [list].&rdquo; The LLM usually self-corrects on the next attempt.</p>
<p><strong>Q: What if the LLM tries to call a tool with wrong arguments?</strong></p>
<p>A: Each tool validates its own arguments. Invalid args return <code>SEA_ERR_INVALID_INPUT</code> with a descriptive message. The agent feeds this error back to the LLM, which adjusts its call.</p>
</section>

<section class="doc-section" id="edge-large-files">
<h2>Edge Case: Large Files &amp; Long Operations</h2>
<p><strong>Q: Can Sea-Claw read a 500 MB log file?</strong></p>
<p>A: The <code>file_read</code> tool reads in chunks bounded by the request arena (default 1 MB). For large files, use <code>head_tail</code> (first/last N lines), <code>grep_text</code> (search for patterns), or <code>shell_exec</code> with <code>awk</code>/<code>sed</code>. The AI is smart enough to choose the right approach.</p>
<p><strong>Q: What if a shell command runs forever?</strong></p>
<p>A: <code>shell_exec</code> has a configurable timeout (default 30 seconds). If exceeded, the process is killed and <code>SEA_ERR_TIMEOUT</code> is returned. The AI reports: &ldquo;Command timed out after 30 seconds.&rdquo;</p>
<p><strong>Q: What if I send a very long message?</strong></p>
<p>A: Messages are bounded by the request arena (1 MB = ~250,000 words). In practice, Telegram limits messages to 4,096 characters. The TUI and HTTP API accept larger inputs but the arena is the hard limit.</p>
</section>

<section class="doc-section" id="faq">
<h2>Frequently Asked Questions</h2>

<h3>Getting Started</h3>
<p><strong>Q: Do I need to know C to use Sea-Claw?</strong><br>A: No. You interact via Telegram, terminal, or HTTP API using natural language. C knowledge is only needed if you want to modify or extend Sea-Claw itself.</p>
<p><strong>Q: Is Sea-Claw free?</strong><br>A: Yes. The code is open source. You only pay for LLM API usage (many providers have free tiers &mdash; OpenRouter offers several free models).</p>
<p><strong>Q: Can I use it without internet?</strong><br>A: Partially. All 63 local tools (file operations, shell commands, data processing) work offline. The AI &ldquo;brain&rdquo; needs internet to reach the LLM API. If you run a local LLM (like Ollama), everything works fully offline.</p>
<p><strong>Q: How long does installation take?</strong><br>A: About 2 minutes. Install dependencies (30 sec), clone repo (10 sec), build (3 sec), configure (1 min). Sea-Claw compiles in ~3 seconds on a modern machine.</p>
<p><strong>Q: What if I mess up the configuration?</strong><br>A: Sea-Claw has sensible defaults for everything. The only required setting is an LLM API key. If config.json is missing, it uses built-in defaults.</p>

<h3>Security &amp; Privacy</h3>
<p><strong>Q: Does my data go to the cloud?</strong><br>A: Only your <em>questions</em> and <em>tool results</em> are sent to the LLM API. Your files, database, and system are never directly accessible to the cloud. Think of it as calling a consultant on the phone &mdash; you describe the problem, they suggest a solution, but they never enter your office.</p>
<p><strong>Q: Can the AI delete my files?</strong><br>A: Only if you explicitly ask it to, and only through the <code>shell_exec</code> or <code>file_write</code> tools. The Grammar Shield blocks injection attempts. You can also restrict which tools are available.</p>
<p><strong>Q: What about prompt injection attacks?</strong><br>A: Sea-Claw&rsquo;s tool calls are parsed from structured JSON, not free text. The LLM must produce valid JSON with a tool name that exists in the 63-tool registry. Hallucinated tool names are simply ignored. The Grammar Shield catches injection patterns before they reach the LLM.</p>
<p><strong>Q: Is it safe to run on a public server?</strong><br>A: Yes, with precautions. Use the <code>telegram_chat_id</code> setting to restrict the bot to your chat only. The HTTP API listens on localhost by default. Use token authentication for API access.</p>

<h3>Technical</h3>
<p><strong>Q: Why C instead of Rust?</strong><br>A: C11 compiles everywhere, has zero runtime overhead, and produces the smallest binaries. Rust&rsquo;s borrow checker adds complexity without benefit when using arena allocation (which already prevents memory bugs by design). C also has a 50-year track record &mdash; Linux, SQLite, curl, Redis, Nginx, and Git are all C.</p>
<p><strong>Q: Can I add my own tools?</strong><br>A: Yes! Create a .c file in <code>src/hands/impl/</code>, register it in <code>sea_tools.c</code>, add to Makefile. See the Developer Guide. It takes about 10 minutes.</p>
<p><strong>Q: How do I update Sea-Claw?</strong><br>A: <code>git pull &amp;&amp; make release</code>. Your database and config are preserved. The SQLite schema auto-migrates.</p>
<p><strong>Q: Can I use a local LLM instead of cloud APIs?</strong><br>A: Yes. Set <code>llm_provider</code> to <code>"local"</code> and point <code>llm_api_url</code> to your Ollama, llama.cpp, or vLLM server. Any OpenAI-compatible API works.</p>
<p><strong>Q: What happens if the LLM API is down?</strong><br>A: The fallback chain kicks in automatically. If OpenRouter fails, Sea-Claw tries OpenAI, then Gemini. If all fail, you get a clear error message. Local tools still work.</p>
<p><strong>Q: Can I run multiple Sea-Claw instances?</strong><br>A: Yes. Each instance uses its own database file and config. You can run one for Telegram, one for the HTTP API, and one for cron jobs &mdash; all on the same machine.</p>
<p><strong>Q: What&rsquo;s the maximum message size?</strong><br>A: Limited by the request arena (default 1 MB). That&rsquo;s about 250,000 words &mdash; far more than any practical message. For very large file operations, the tool reads/writes in chunks.</p>

<h3>Raspberry Pi &amp; Edge</h3>
<p><strong>Q: Which Pi models are supported?</strong><br>A: Pi 4 (all RAM variants), Pi 5, and Pi Zero 2 W. The Pi 3B+ works but is slower. Pi Zero (original, ARMv6) is not supported.</p>
<p><strong>Q: How much does it cost to run 24/7 on a Pi?</strong><br>A: A Pi 4 draws ~5 watts. At typical electricity rates, that&rsquo;s about $5/year. Add ~$2-10/month for LLM API usage depending on how much you chat.</p>
<p><strong>Q: Can Sea-Claw control GPIO pins on the Pi?</strong><br>A: Yes, through the <code>shell_exec</code> tool. You can read sensors, control LEDs, or trigger relays by running shell commands that interact with GPIO.</p>
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

<section class="doc-section" id="glossary">
<h2>Glossary (30+ Terms)</h2>
<p>Hover over <span class="glossary" data-def="This is an example tooltip! Hover over any dotted-underlined term in the docs to see its definition.">dotted terms</span> anywhere in the docs for quick definitions. Here&rsquo;s the full list:</p>
<table>
<tr><th>Term</th><th>Plain-English Definition</th></tr>
<tr><td><strong>API</strong></td><td>Application Programming Interface &mdash; a way for programs to talk to each other. Like a waiter taking your order to the kitchen and bringing back food.</td></tr>
<tr><td><strong>Arena Allocator</strong></td><td>A memory strategy where you grab one big block upfront, hand out pieces sequentially, and free everything at once. Like a whiteboard: write, erase all, repeat. No memory leaks possible.</td></tr>
<tr><td><strong>A2A</strong></td><td>Agent-to-Agent protocol. How Sea-Claw talks to other AI agents (like Agent Zero) to delegate tasks.</td></tr>
<tr><td><strong>Binary</strong></td><td>A compiled program file that your computer can run directly. Sea-Claw compiles into a single binary &mdash; one file, ready to go.</td></tr>
<tr><td><strong>C11</strong></td><td>The 2011 version of the C programming language. C is the language that Linux, SQLite, and most operating systems are written in.</td></tr>
<tr><td><strong>Compiler</strong></td><td>A program that translates human-readable code into machine instructions. Like a translator turning a recipe into actual cooking steps.</td></tr>
<tr><td><strong>Cron</strong></td><td>A scheduler that runs tasks at specific times. Named after Chronos (Greek god of time). &ldquo;Every Monday at 9 AM, check disk usage.&rdquo;</td></tr>
<tr><td><strong>Docker</strong></td><td>A tool that packages software into &ldquo;containers&rdquo; &mdash; like shipping containers for code. Everything the program needs is inside, so it runs the same everywhere.</td></tr>
<tr><td><strong>DNS</strong></td><td>Domain Name System &mdash; the internet&rsquo;s phone book. Translates &ldquo;google.com&rdquo; into an IP address (like 142.250.80.46).</td></tr>
<tr><td><strong>Fallback Chain</strong></td><td>A backup plan. If the primary LLM fails, Sea-Claw automatically tries the next one. Like having a backup generator when the power goes out.</td></tr>
<tr><td><strong>Grammar Shield</strong></td><td>Sea-Claw&rsquo;s security system. Checks every byte of input against allowed character sets. Catches injection attacks in under 1 microsecond.</td></tr>
<tr><td><strong>HTTP/HTTPS</strong></td><td>The language web browsers and servers use to communicate. HTTPS adds encryption (the &ldquo;S&rdquo; stands for Secure).</td></tr>
<tr><td><strong>Injection Attack</strong></td><td>A hacking technique where malicious commands are hidden inside normal-looking input. Like hiding a bomb inside a gift box. The Grammar Shield catches these.</td></tr>
<tr><td><strong>JSON</strong></td><td>JavaScript Object Notation &mdash; a way to structure data as labeled fields. Like a form: <code>{"name": "Alice", "age": 30}</code>.</td></tr>
<tr><td><strong>libcurl</strong></td><td>A C library for making HTTP requests. Used by Sea-Claw to talk to Telegram, LLM APIs, and web services. Powers billions of devices worldwide.</td></tr>
<tr><td><strong>LLM</strong></td><td>Large Language Model &mdash; an AI that understands and generates human language. Examples: GPT-4, Claude, Gemini. Sea-Claw uses these as its &ldquo;brain.&rdquo;</td></tr>
<tr><td><strong>malloc/free</strong></td><td>Traditional C memory management. You request memory (malloc) and return it (free). Forgetting to free causes leaks. Sea-Claw avoids this entirely with arenas.</td></tr>
<tr><td><strong>Mesh Network</strong></td><td>Multiple Sea-Claw instances connected together, sharing capabilities and delegating tasks across nodes.</td></tr>
<tr><td><strong>mmap</strong></td><td>Memory-mapped file &mdash; a way to reserve a block of memory from the operating system. Sea-Claw uses this to create its arenas.</td></tr>
<tr><td><strong>PII</strong></td><td>Personally Identifiable Information &mdash; data that can identify a person (email, phone, SSN). Sea-Claw can detect and redact PII before sending to cloud LLMs.</td></tr>
<tr><td><strong>SeaSlice</strong></td><td>Sea-Claw&rsquo;s zero-copy string type. A pointer + length that views existing memory without copying it. Like reading a book without photocopying every page.</td></tr>
<tr><td><strong>SeaZero</strong></td><td>Sea-Claw&rsquo;s multi-agent orchestration layer. Delegates tasks to Agent Zero (Python) running in Docker. Sea-Claw stays sovereign; Agent Zero does the heavy lifting.</td></tr>
<tr><td><strong>SQLite</strong></td><td>A tiny database engine that stores everything in a single file. No server needed. Used by Firefox, Android, and billions of devices. Sea-Claw uses it for all persistent data.</td></tr>
<tr><td><strong>SSE</strong></td><td>Server-Sent Events &mdash; a way for a server to push data to a client in real-time. Sea-Claw uses this to stream LLM tokens as they arrive, so you see the response being typed out.</td></tr>
<tr><td><strong>Static Tool Registry</strong></td><td>All 63 tools are compiled into the binary at build time. The AI cannot create new tools at runtime. Like a vending machine with fixed buttons &mdash; you can&rsquo;t add new ones without rebuilding the machine.</td></tr>
<tr><td><strong>Telegram</strong></td><td>A messaging app with 900+ million users. Sea-Claw runs as a Telegram bot you can message from your phone, anywhere in the world.</td></tr>
<tr><td><strong>Token (LLM)</strong></td><td>A word-piece used by LLMs. 1 token &asymp; 3/4 of a word. &ldquo;Hello world&rdquo; is 2 tokens. 1024 tokens &asymp; 750 words &asymp; 1.5 pages.</td></tr>
<tr><td><strong>Token (Auth)</strong></td><td>A secret string used to prove identity. Like a password, but generated by the system and used for API access.</td></tr>
<tr><td><strong>TUI</strong></td><td>Text User Interface &mdash; an interactive terminal. You type commands and see text output. Sea-Claw&rsquo;s TUI supports 30+ slash commands and natural language input.</td></tr>
<tr><td><strong>WAL Mode</strong></td><td>Write-Ahead Logging &mdash; a SQLite feature that allows reading while writing. Makes the database faster and more reliable.</td></tr>
<tr><td><strong>WebSocket</strong></td><td>A protocol for real-time two-way communication between a browser and a server. Like a phone call (always connected) vs. HTTP which is like sending letters.</td></tr>
<tr><td><strong>Zero-Copy</strong></td><td>A technique where data is read in place without making copies. Sea-Claw&rsquo;s JSON parser points into the original input buffer instead of copying strings. Much faster and uses less memory.</td></tr>
</table>
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
