// Part 1: Introduction sections (enhanced with analogies, tooltips, Claw Family, narratives)

var CONTENT_INTRO = `
<section class="doc-section" id="what-is-seaclaw">
<h2>What is Sea-Claw?</h2>

<p style="font-size:1.15rem;color:#f0f6fc;margin-bottom:18px;"><strong>Sea-Claw is your own private AI assistant that lives on your computer, not in someone else&rsquo;s cloud.</strong></p>

<p>You talk to it through <span class="glossary" data-def="A messaging app with 900+ million users. Sea-Claw runs as a Telegram bot you can message from your phone.">Telegram</span>, a terminal, or a web <span class="glossary" data-def="Application Programming Interface &mdash; a way for programs to talk to each other over the internet using structured requests.">API</span>. It thinks using cloud AI brains (like GPT-4 or Claude), but everything it <em>does</em> &mdash; reading files, running commands, managing tasks &mdash; happens on <em>your machine</em>, under <em>your control</em>.</p>

<div class="callout info">
<div class="callout-title">The 30-Second Version</div>
<p>ChatGPT can <em>talk</em>. Sea-Claw can <em>talk and do</em>. It has 63 built-in tools &mdash; it can read your files, check your server, search the web, schedule jobs, and more. And unlike ChatGPT, your data never leaves your computer. The AI brain is rented; the body is yours.</p>
</div>

<h3>The Key Idea</h3>
<p>Most AI assistants are like <strong>renting a furnished apartment</strong>: someone else owns it, you can&rsquo;t change the locks, and they can look through your stuff. Sea-Claw is like <strong>building your own house</strong>: you own the walls (the binary), the safe (the database), the security system (the Grammar Shield), and the toolbox (63 tools). You just hire a smart architect (the cloud LLM) to help you think.</p>

<div class="diagram">                     YOUR MACHINE (you own everything here)
  +----------+  +----------+  +----------+  +---------+
  | Security |  | 63 Tools |  | Database |  | Memory  |
  | Shield   |  | (Hands)  |  | (SQLite) |  | System  |
  +----+-----+  +----+-----+  +----+-----+  +----+----+
       |              |              |              |
       +--------------+--------------+--------------+
                           |
                     [ Sea-Claw Binary ]  &lt;-- single file, ~280 KB
                           |  HTTPS (encrypted)
                     [ Cloud LLM ]  &lt;-- rented brain
                     (GPT / Claude / Gemini)</div>

<h3>What Makes It Different</h3>
<ul>
  <li><strong>Single binary</strong> &mdash; One file. No Python, no Node.js, no virtual environments. Like a Swiss Army knife vs. a toolbox full of separate tools.</li>
  <li><strong>Zero malloc</strong> &mdash; Uses <span class="glossary" data-def="A memory strategy where you pre-allocate one big block and hand out pieces sequentially. When done, reset the whole block instantly. No leaks possible.">arena memory</span>. Think of it as a whiteboard: write, erase everything at once, start fresh. No memory leaks, ever.</li>
  <li><strong>63 built-in tools</strong> &mdash; File I/O, shell commands, web search, task management, cron jobs, DNS lookup, SSL checks, and more. The AI&rsquo;s hands.</li>
  <li><strong>Grammar Shield</strong> &mdash; Every byte of input is checked against security rules in under 1 microsecond. That&rsquo;s faster than light travels 300 meters. Hackers can&rsquo;t sneak commands past it.</li>
  <li><strong>Multi-channel</strong> &mdash; Talk via Telegram, terminal, HTTP API, WebSocket, or Slack. One brain, many mouths.</li>
  <li><strong>Multi-agent</strong> &mdash; Delegate complex tasks to other AI agents via the <span class="glossary" data-def="Sea-Claw's multi-agent orchestration layer. It lets Sea-Claw delegate tasks to Agent Zero (a Python AI) running in a Docker container.">SeaZero</span> bridge.</li>
  <li><strong>Runs anywhere</strong> &mdash; From a $35 Raspberry Pi to a cloud server. If it has a CPU, Sea-Claw runs on it.</li>
</ul>
</section>

<section class="doc-section" id="why-seaclaw">
<h2>Why Sea-Claw?</h2>
<h3>The Problem with Current AI Tools</h3>

<div class="flow-step"><div class="step-num">1</div><div class="step-text"><strong>No sovereignty</strong> &mdash; Your conversations, files, and data go to someone else&rsquo;s server. It&rsquo;s like keeping your diary in a stranger&rsquo;s filing cabinet &mdash; they promise not to read it, but you can&rsquo;t verify that.</div></div>
<div class="flow-step"><div class="step-num">2</div><div class="step-text"><strong>No tools</strong> &mdash; ChatGPT can diagnose your problem but can&rsquo;t operate. It&rsquo;s a doctor who can tell you &ldquo;your disk is full&rdquo; but can&rsquo;t clean it up. Sea-Claw can both diagnose <em>and</em> fix.</div></div>
<div class="flow-step"><div class="step-num">3</div><div class="step-text"><strong>No safety guarantees</strong> &mdash; When AI tools <em>do</em> exist (like code interpreters), they run arbitrary code with no guardrails. It&rsquo;s like giving your house keys to a stranger and hoping they don&rsquo;t rearrange the furniture &mdash; or burn it down.</div></div>

<h3>How Sea-Claw Solves Each One</h3>
<table>
<tr><th>Problem</th><th>Solution</th><th>Analogy</th></tr>
<tr><td>No sovereignty</td><td>Runs on <em>your</em> machine</td><td>Your diary stays in <em>your</em> safe. Only the &ldquo;thinking&rdquo; part goes to the cloud.</td></tr>
<tr><td>No tools</td><td>63 built-in tools</td><td>The doctor can now also operate, prescribe, and follow up &mdash; all in your home clinic.</td></tr>
<tr><td>No safety</td><td>Grammar Shield + static registry</td><td>The house keys only open specific doors. The AI can&rsquo;t pick locks or make new keys.</td></tr>
</table>

<div class="callout tip">
<div class="callout-title">For Non-Technical Readers</div>
<p>Imagine hiring a brilliant assistant. With ChatGPT, they work in a locked room at someone else&rsquo;s office &mdash; you slide notes under the door and hope for the best. With Sea-Claw, they work <em>in your house</em>, using <em>your tools</em>, following <em>your rules</em>, and you can watch everything they do through a glass wall.</p>
</div>
</section>

<section class="doc-section" id="who-is-it-for">
<h2>Who Is It For?</h2>
<table>
<tr><th>Audience</th><th>Use Case</th><th>Real-World Scenario</th></tr>
<tr><td><strong>Developers</strong></td><td>AI-powered DevOps</td><td>It&rsquo;s 2 AM. Your monitoring alert fires. Instead of SSH-ing into 5 servers, you text Sea-Claw on Telegram: &ldquo;Check disk usage on all servers.&rdquo; It runs the commands, finds the problem, and tells you which server needs cleanup.</td></tr>
<tr><td><strong>Sysadmins</strong></td><td>Server monitoring</td><td>You&rsquo;re at lunch. You send <code>/status</code> from your phone. Sea-Claw replies with CPU, memory, disk, and uptime &mdash; all in 2 seconds. No VPN needed.</td></tr>
<tr><td><strong>Hobbyists</strong></td><td>Raspberry Pi automation</td><td>Your Pi runs Sea-Claw 24/7 on 5 watts. You message it: &ldquo;What&rsquo;s the temperature in my server room?&rdquo; It reads the sensor, checks the threshold, and texts you back.</td></tr>
<tr><td><strong>Researchers</strong></td><td>Data processing</td><td>You have 50 CSV files from an experiment. You tell Sea-Claw: &ldquo;Parse all CSVs in /data, compute the mean of column 3, and create a summary.&rdquo; It does it in seconds.</td></tr>
<tr><td><strong>Enterprises</strong></td><td>On-premise AI</td><td>Compliance requires all AI interactions to be auditable. Sea-Claw logs every action to SQLite. Your auditor can query the database directly. No cloud vendor involved.</td></tr>
<tr><td><strong>Students</strong></td><td>Learning C</td><td>You want to understand how a real system is built in C. Sea-Claw is 23K lines of clean C11 with zero magic &mdash; no frameworks, no metaclasses. Every function does what it says.</td></tr>
</table>
</section>

<section class="doc-section" id="quick-stats">
<h2>Quick Stats</h2>
<div class="stats-grid">
  <div class="stat-card"><div class="stat-value">23,437</div><div class="stat-label">Lines of C11</div></div>
  <div class="stat-card"><div class="stat-value">124</div><div class="stat-label">Source Files</div></div>
  <div class="stat-card"><div class="stat-value">63</div><div class="stat-label">Built-in Tools</div></div>
  <div class="stat-card"><div class="stat-value">29</div><div class="stat-label">Header Files</div></div>
  <div class="stat-card"><div class="stat-value">26</div><div class="stat-label">Test Suites</div></div>
  <div class="stat-card"><div class="stat-value">2</div><div class="stat-label">External Deps</div></div>
  <div class="stat-card"><div class="stat-value">&lt;1&micro;s</div><div class="stat-label">Shield Check</div></div>
  <div class="stat-card"><div class="stat-value">11ns</div><div class="stat-label">Arena Alloc</div></div>
</div>

<details class="expand">
<summary>What do these numbers actually mean?</summary>
<div class="expand-content">
<ul>
  <li><strong>11 nanoseconds per allocation</strong> &mdash; That&rsquo;s 90 million memory allocations per second. Faster than light travels 3.3 meters.</li>
  <li><strong>&lt;1 microsecond shield check</strong> &mdash; Sea-Claw validates your entire message for security threats in less time than it takes a hummingbird to flap its wings once.</li>
  <li><strong>~280 KB binary</strong> &mdash; Smaller than a single photo on your phone. For comparison, a typical Python AI framework is 500 MB &mdash; that&rsquo;s 1,800 times larger.</li>
  <li><strong>~17 MB memory</strong> &mdash; Uses less RAM than a single Chrome tab. You could run 100 Sea-Claw instances in the memory Chrome uses for 5 tabs.</li>
  <li><strong>&lt;10 ms startup</strong> &mdash; From &ldquo;run&rdquo; to &ldquo;ready&rdquo; in the time it takes you to blink. Python frameworks take 2-10 seconds.</li>
  <li><strong>2 external dependencies</strong> &mdash; Just libcurl (for HTTP) and SQLite (for database). Compare to 400+ npm packages for a typical Node.js agent.</li>
</ul>
</div>
</details>
</section>

<section class="doc-section" id="how-it-compares">
<h2>How It Compares (vs. Frameworks)</h2>
<p>Sea-Claw vs. popular AI agent frameworks:</p>
<table>
<tr><th>Feature</th><th>Sea-Claw</th><th>LangChain</th><th>AutoGPT</th><th>ChatGPT</th></tr>
<tr><td>Language</td><td><span class="badge badge-green">C11</span></td><td>Python</td><td>Python</td><td>Cloud</td></tr>
<tr><td>Binary size</td><td><strong>~280 KB</strong></td><td>~500 MB</td><td>~300 MB</td><td>N/A</td></tr>
<tr><td>Memory</td><td><strong>~17 MB</strong></td><td>200-500 MB</td><td>500 MB+</td><td>N/A</td></tr>
<tr><td>Startup</td><td><strong>&lt;10 ms</strong></td><td>2-5 sec</td><td>5-10 sec</td><td>N/A</td></tr>
<tr><td>Memory safety</td><td><span class="badge badge-green">Arena (zero leaks)</span></td><td>GC</td><td>GC</td><td>N/A</td></tr>
<tr><td>Input validation</td><td><span class="badge badge-green">Grammar Shield</span></td><td>None built-in</td><td>None built-in</td><td>Cloud-side</td></tr>
<tr><td>Tool security</td><td><span class="badge badge-green">Static registry</span></td><td>Dynamic (eval)</td><td>Dynamic (exec)</td><td>Sandboxed</td></tr>
<tr><td>Data sovereignty</td><td><span class="badge badge-green">100% local</span></td><td>Local</td><td>Local</td><td><span class="badge badge-red">Cloud only</span></td></tr>
<tr><td>Raspberry Pi</td><td><span class="badge badge-green">Yes</span></td><td>Barely</td><td>No</td><td>No</td></tr>
<tr><td>Audit trail</td><td><span class="badge badge-green">SQLite</span></td><td>Custom</td><td>Logs</td><td>None</td></tr>
<tr><td>Multi-channel</td><td><span class="badge badge-green">5 channels</span></td><td>Custom</td><td>Web UI</td><td>Web/API</td></tr>
</table>
</section>

<section class="doc-section" id="claw-family">
<h2>The Claw Family &mdash; How Sea-Claw Fits In</h2>
<p>Sea-Claw is part of a broader ecosystem of AI agent platforms. Here&rsquo;s how they compare:</p>

<table>
<tr><th>Feature</th><th>Sea-Claw</th><th>OpenClaw</th><th>PicoClaw</th><th>ZeroClaw</th><th>Nanobot</th><th>MimicLaw</th></tr>
<tr><td>Language</td><td><span class="badge badge-green">C11</span></td><td>TypeScript</td><td>Go</td><td>Rust</td><td>Python</td><td>C (ESP32)</td></tr>
<tr><td>Binary/Install</td><td>~280 KB</td><td>~200 MB (npm)</td><td>~15 MB</td><td>~3.4 MB</td><td>pip install</td><td>Firmware flash</td></tr>
<tr><td>RAM at idle</td><td><strong>17 MB</strong></td><td>200-300 MB</td><td>~40 MB</td><td>~20 MB</td><td>~100 MB</td><td>~512 KB</td></tr>
<tr><td>Startup</td><td><strong>&lt;10 ms</strong></td><td>~3 sec</td><td>~50 ms</td><td>~30 ms</td><td>~2 sec</td><td>~500 ms</td></tr>
<tr><td>Tools</td><td><strong>63</strong></td><td>~20</td><td>~12</td><td>~8 plugins</td><td>~15</td><td>~5</td></tr>
<tr><td>Channels</td><td>5 (TG/TUI/API/WS/Slack)</td><td>10+ (TG/WA/Discord/Slack/...)</td><td>2 (TUI/TG)</td><td>2 (TUI/TG)</td><td>3 (TUI/TG/WA)</td><td>2 (TG/WS)</td></tr>
<tr><td>Security</td><td><span class="badge badge-green">6-layer Shield</span></td><td>YARA rules</td><td>Input sanitize</td><td>Trait-based</td><td>Basic</td><td>Basic</td></tr>
<tr><td>Memory model</td><td>Arena (zero malloc)</td><td>V8 GC</td><td>Go GC</td><td>Rust ownership</td><td>Python GC</td><td>Static alloc</td></tr>
<tr><td>Multi-agent</td><td><span class="badge badge-green">SeaZero bridge</span></td><td>Partial</td><td>None</td><td>None</td><td>None</td><td>None</td></tr>
<tr><td>Runs on Pi</td><td><span class="badge badge-green">Yes</span></td><td>No (needs Node.js)</td><td>Yes</td><td>Yes</td><td>Barely</td><td>ESP32 only</td></tr>
<tr><td>Dependencies</td><td><strong>2</strong></td><td>400+ npm</td><td>~20 Go</td><td>~30 crates</td><td>~50 pip</td><td>0 (bare metal)</td></tr>
<tr><td>Audit trail</td><td><span class="badge badge-green">SQLite</span></td><td>Cloud</td><td>None</td><td>File logs</td><td>None</td><td>None</td></tr>
<tr><td>Cron scheduler</td><td><span class="badge badge-green">Built-in</span></td><td>External</td><td>None</td><td>None</td><td>None</td><td>cron.json</td></tr>
<tr><td>Best for</td><td>Sovereign edge AI</td><td>Multi-channel SaaS</td><td>Lightweight Go fans</td><td>Rust enthusiasts</td><td>Quick prototyping</td><td>IoT / $5 chips</td></tr>
</table>

<div class="callout info">
<div class="callout-title">How to Read This Table</div>
<p><strong>OpenClaw</strong> is the most feature-rich (10+ channels, huge community) but needs 200+ MB RAM and Node.js. <strong>Sea-Claw</strong> trades channel breadth for extreme efficiency, security, and edge deployment. <strong>MimicLaw</strong> is the most constrained &mdash; it runs on a $5 ESP32 chip with no OS. <strong>ZeroClaw</strong> (Rust) is the closest competitor in philosophy but has fewer tools and no multi-agent support.</p>
</div>
</section>

<section class="doc-section" id="day-in-the-life">
<h2>A Day in the Life with Sea-Claw</h2>
<p>Here&rsquo;s what a typical day looks like when Sea-Claw runs on your Raspberry Pi as a 24/7 Telegram bot:</p>

<h3>7:00 AM &mdash; Morning Briefing</h3>
<p>A cron job fires automatically. Sea-Claw checks your server health and sends you a Telegram message before you wake up:</p>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble"><strong>Morning Report (auto)</strong><br>All 3 servers healthy. Disk: 45%/62%/38%. No SSL certs expiring this week. 2 pending tasks from yesterday.</div></div>
</div>

<h3>9:30 AM &mdash; Quick Check from Your Phone</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">/status</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">CPU: 12% | RAM: 2.1/4.0 GB | Disk: 45% | Uptime: 14d 6h<br>LLM: OpenRouter (kimi-k2.5) | Tools: 63 | Shield: ACTIVE</div></div>
</div>

<h3>2:00 PM &mdash; Natural Language Task</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">Find all log files larger than 100MB and tell me which ones I can safely delete</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Found 3 files over 100MB:<br><code>/var/log/syslog.1</code> (240 MB) &mdash; rotated, safe to delete<br><code>/var/log/nginx/access.log.2</code> (180 MB) &mdash; archived, safe to delete<br><code>/var/log/postgresql/main.log</code> (120 MB) &mdash; active DB log, keep this one<br><br>Want me to delete the first two? That would free 420 MB.</div></div>
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">Yes, delete them</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Done. Deleted 2 files, freed 420 MB. Disk now at 38%.</div></div>
</div>

<h3>6:00 PM &mdash; Delegate a Complex Task</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar user">Y</div><div class="convo-bubble">/delegate Research the top 5 Rust web frameworks in 2025 and create a comparison table</div></div>
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble">Delegated to Agent Zero. Task ID: SZ-042. I&rsquo;ll notify you when it&rsquo;s done. (You can check progress with <code>/sztasks</code>)</div></div>
</div>

<h3>11:00 PM &mdash; Evening Cron Report</h3>
<div class="convo">
  <div class="convo-msg"><div class="convo-avatar bot">S</div><div class="convo-bubble"><strong>Daily Summary (auto)</strong><br>Tasks completed: 4 | Tools used: 12 | LLM tokens: 8,420<br>Agent Zero task SZ-042: COMPLETED (report saved to /home/pi/reports/rust-frameworks.md)<br>No security alerts. Goodnight.</div></div>
</div>
</section>

<section class="doc-section" id="how-message-travels">
<h2>How a Message Travels Through Sea-Claw</h2>
<p>When you send a message, here&rsquo;s exactly what happens inside Sea-Claw, step by step:</p>

<div class="flow-step"><div class="step-num">1</div><div class="step-text"><strong>You type a message</strong> &mdash; via Telegram, terminal, or HTTP API. Example: &ldquo;What&rsquo;s my disk usage?&rdquo;</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">2</div><div class="step-text"><strong>Channel receives it</strong> &mdash; The Telegram bot, TUI, or API server picks up your message and puts it on the internal message bus.</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">3</div><div class="step-text"><strong>Grammar Shield validates</strong> &mdash; Every byte is checked against security rules. Shell injection like <code>; rm -rf /</code> is caught here in &lt;1&micro;s. If rejected, you get an error immediately.</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">4</div><div class="step-text"><strong>Brain builds context</strong> &mdash; Your message + conversation history + system prompt + tool descriptions are assembled into a request for the LLM.</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">5</div><div class="step-text"><strong>LLM thinks</strong> &mdash; The request is sent to OpenRouter (or fallback to OpenAI, then Gemini). The LLM decides: &ldquo;I should call the <code>disk_usage</code> tool.&rdquo;</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">6</div><div class="step-text"><strong>Tool executes locally</strong> &mdash; Sea-Claw looks up <code>disk_usage</code> in the static registry (63 compiled tools), calls the function, and gets the result. This happens on YOUR machine &mdash; the LLM never sees your filesystem.</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">7</div><div class="step-text"><strong>Brain interprets result</strong> &mdash; The tool output goes back to the LLM, which formats a human-friendly answer: &ldquo;Your disk is 45% full (18 GB free).&rdquo;</div></div>
<div class="flow-arrow">&darr;</div>
<div class="flow-step"><div class="step-num">8</div><div class="step-text"><strong>Response sent back</strong> &mdash; The answer goes through the channel back to you. The request arena is reset (one pointer move), freeing all temporary memory instantly.</div></div>

<div class="callout tip">
<div class="callout-title">Key Insight</div>
<p>Notice that your <em>files and data</em> never leave your machine. The LLM only sees the <em>question</em> and the <em>tool result</em>. It never gets direct access to your filesystem, database, or network. Sea-Claw is the middleman that keeps everything safe.</p>
</div>
</section>
`;
