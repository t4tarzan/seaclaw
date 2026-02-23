// Part 1: Introduction + Getting Started sections

var CONTENT_INTRO = `
<section class="doc-section" id="what-is-seaclaw">
<h2>What is Sea-Claw?</h2>
<p>Sea-Claw is a <strong>sovereign AI agent platform</strong> &mdash; a single binary written in C11 that turns any computer (from a Raspberry Pi to a cloud server) into an intelligent assistant. It connects to large language models (LLMs) like GPT-4, Claude, or Gemini, but keeps <em>you</em> in control of your data, your tools, and your security.</p>
<div class="callout info">
<div class="callout-title">In Plain English</div>
Sea-Claw is a program you run on your own computer. You talk to it (via Telegram, a terminal, or a web API), and it talks to an AI brain in the cloud. But unlike ChatGPT, Sea-Claw can also <em>do things</em> &mdash; read files, run commands, manage tasks, search the web, schedule jobs &mdash; all through 63 built-in tools that the AI can call on your behalf.
</div>
<h3>The Key Idea</h3>
<p>Most AI assistants are <strong>cloud-only</strong>: your data goes to someone else's server. Sea-Claw flips this: the <strong>brain</strong> (LLM) lives in the cloud, but the <strong>body</strong> (tools, memory, database, security) lives on <em>your machine</em>. The AI can only do what your tools allow.</p>
<div class="diagram">                     Your Machine
  +----------+  +----------+  +----------+  +---------+
  | Security |  | 63 Tools |  | Database |  | Memory  |
  | Shield   |  | (Hands)  |  | (SQLite) |  | System  |
  +----+-----+  +----+-----+  +----+-----+  +----+----+
       |              |              |              |
       +--------------+--------------+--------------+
                           |
                     [ Sea-Claw Binary ]
                           |  HTTPS (encrypted)
                     [ Cloud LLM (GPT/Claude/Gemini) ]</div>
<h3>What Makes It Different</h3>
<ul>
  <li><strong>Single binary</strong> &mdash; One file, no Python, no Node.js. Just compile and run.</li>
  <li><strong>Zero malloc</strong> &mdash; Arena memory allocation. No leaks, no GC pauses, no fragmentation.</li>
  <li><strong>63 built-in tools</strong> &mdash; File I/O, shell, web search, tasks, cron, DNS, SSL, and more.</li>
  <li><strong>Grammar Shield</strong> &mdash; Every byte validated against security grammars. Injection caught in &lt;1&micro;s.</li>
  <li><strong>Multi-channel</strong> &mdash; Telegram, terminal (TUI), HTTP API, WebSocket, or Slack.</li>
  <li><strong>Multi-agent</strong> &mdash; Delegate tasks to other agents via the SeaZero bridge.</li>
  <li><strong>Runs anywhere</strong> &mdash; x86 servers, ARM Raspberry Pi, Docker containers, bare metal.</li>
</ul>
</section>

<section class="doc-section" id="why-seaclaw">
<h2>Why Sea-Claw?</h2>
<h3>The Problem with Current AI Tools</h3>
<ol>
  <li><strong>No sovereignty</strong> &mdash; Your data goes to someone else's server.</li>
  <li><strong>No tools</strong> &mdash; ChatGPT can talk, but can't read your files or manage your server.</li>
  <li><strong>No safety guarantees</strong> &mdash; Code interpreters run arbitrary code with no security boundary.</li>
</ol>
<h3>How Sea-Claw Solves Each One</h3>
<table>
<tr><th>Problem</th><th>Solution</th><th>How</th></tr>
<tr><td>No sovereignty</td><td>Runs on <em>your</em> machine</td><td>Single binary, SQLite DB, all data local. Only LLM API calls leave your network.</td></tr>
<tr><td>No tools</td><td>63 built-in tools</td><td>Static registry compiled into binary. AI calls tools by name; Sea-Claw executes locally.</td></tr>
<tr><td>No safety</td><td>Grammar Shield + static registry</td><td>Every byte validated. No <code>eval()</code>, no <code>exec()</code>, no dynamic loading.</td></tr>
</table>
<div class="callout tip">
<div class="callout-title">For Non-Technical Readers</div>
Imagine hiring a smart assistant. With ChatGPT, they work in someone else's locked office. With Sea-Claw, they work <em>in your house</em>, using <em>your tools</em>, following <em>your rules</em>. You can watch everything they do.
</div>
</section>

<section class="doc-section" id="who-is-it-for">
<h2>Who Is It For?</h2>
<table>
<tr><th>Audience</th><th>Use Case</th><th>Example</th></tr>
<tr><td><strong>Developers</strong></td><td>AI-powered DevOps</td><td>"Check disk usage, alert if above 80%"</td></tr>
<tr><td><strong>Sysadmins</strong></td><td>Server monitoring via Telegram</td><td>Send <code>/status</code> from your phone</td></tr>
<tr><td><strong>Hobbyists</strong></td><td>Raspberry Pi automation</td><td>Control IoT devices via Telegram</td></tr>
<tr><td><strong>Researchers</strong></td><td>Data processing</td><td>Parse CSV, compute stats via natural language</td></tr>
<tr><td><strong>Enterprises</strong></td><td>On-premise AI with audit trail</td><td>Every action logged. Full compliance.</td></tr>
<tr><td><strong>Students</strong></td><td>Learning C &amp; systems programming</td><td>Clean C11 codebase, minimal dependencies</td></tr>
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
<table>
<tr><th>Metric</th><th>Value</th><th>Why It Matters</th></tr>
<tr><td>Binary size (stripped)</td><td>~280 KB</td><td>Fits on any device, instant startup</td></tr>
<tr><td>Memory usage</td><td>~17 MB</td><td>16 MB arena + 1 MB request + overhead</td></tr>
<tr><td>External deps</td><td>libcurl + libsqlite3</td><td>Ubiquitous, pre-installed on most Linux</td></tr>
<tr><td>Build time</td><td>~3 seconds</td><td>Full rebuild from scratch</td></tr>
<tr><td>Startup time</td><td>&lt;10 ms</td><td>From <code>./sea_claw</code> to ready</td></tr>
<tr><td>Architectures</td><td>x86-64, ARM64</td><td>Servers, desktops, Raspberry Pi</td></tr>
</table>
</section>

<section class="doc-section" id="how-it-compares">
<h2>How It Compares</h2>
<table>
<tr><th>Feature</th><th>Sea-Claw</th><th>LangChain</th><th>AutoGPT</th><th>ChatGPT</th></tr>
<tr><td>Language</td><td><span class="badge badge-green">C11</span></td><td>Python</td><td>Python</td><td>Cloud</td></tr>
<tr><td>Binary size</td><td><strong>~280 KB</strong></td><td>~500 MB</td><td>~300 MB</td><td>N/A</td></tr>
<tr><td>Memory</td><td><strong>~17 MB</strong></td><td>200-500 MB</td><td>500 MB+</td><td>N/A</td></tr>
<tr><td>Startup</td><td><strong>&lt;10 ms</strong></td><td>2-5 sec</td><td>5-10 sec</td><td>N/A</td></tr>
<tr><td>Memory safety</td><td><span class="badge badge-green">Arena</span></td><td>GC</td><td>GC</td><td>N/A</td></tr>
<tr><td>Input validation</td><td><span class="badge badge-green">Grammar Shield</span></td><td>None</td><td>None</td><td>Cloud</td></tr>
<tr><td>Tool security</td><td><span class="badge badge-green">Static registry</span></td><td>Dynamic</td><td>Dynamic</td><td>Sandboxed</td></tr>
<tr><td>Data sovereignty</td><td><span class="badge badge-green">100% local</span></td><td>Local</td><td>Local</td><td><span class="badge badge-red">Cloud only</span></td></tr>
<tr><td>Raspberry Pi</td><td><span class="badge badge-green">Yes</span></td><td>Barely</td><td>No</td><td>No</td></tr>
</table>
</section>
`;
