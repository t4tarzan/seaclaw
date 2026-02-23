// Part 2: Getting Started sections (enhanced with plain-English, try-it boxes, annotations)

var CONTENT_START = `
<section class="doc-section" id="prerequisites">
<h2>Prerequisites</h2>
<p>Sea-Claw needs a few standard tools to build. Think of it like baking a cake &mdash; you need an oven (compiler), a mixing bowl (libraries), and a recipe book (build system). Most Linux systems already have these.</p>
<table>
<tr><th>Requirement</th><th>What It Is</th><th>Why Sea-Claw Needs It</th><th>Install</th></tr>
<tr><td><strong>GCC or Clang</strong></td><td>A <span class="glossary" data-def="A program that translates human-readable C code into machine code that your computer can execute. Like a translator turning a recipe into actual cooking instructions.">compiler</span> &mdash; translates C code into a program your computer can run</td><td>Builds the Sea-Claw binary</td><td><code>apt install build-essential</code></td></tr>
<tr><td><strong>libcurl</strong></td><td>A library for making <span class="glossary" data-def="HyperText Transfer Protocol &mdash; the language web browsers and servers use to communicate. When you visit a website, your browser speaks HTTP.">HTTP</span> requests &mdash; like a phone for your program to call websites</td><td>Talks to Telegram, LLM APIs, and web services</td><td><code>apt install libcurl4-openssl-dev</code></td></tr>
<tr><td><strong>libsqlite3</strong></td><td>A tiny <span class="glossary" data-def="A structured collection of data stored in a file. SQLite is a database that lives in a single file on your computer &mdash; no server needed.">database</span> engine that stores data in a single file</td><td>Stores chat history, tasks, config, and audit trail</td><td><code>apt install libsqlite3-dev</code></td></tr>
<tr><td><strong>libreadline</strong></td><td>Makes the terminal input nicer (arrow keys, history)</td><td>Better typing experience in TUI mode (optional)</td><td><code>apt install libreadline-dev</code></td></tr>
</table>
<h3>One-Line Install (Debian/Ubuntu/Raspberry Pi OS)</h3>
<div class="try-it">
<div class="try-it-header"><div class="dots"><div class="dot red"></div><div class="dot yellow"></div><div class="dot green"></div></div><div class="label">TERMINAL</div><button class="try-it-copy">Copy</button></div>
<div class="try-it-body"><span class="prompt">$</span> sudo apt update && sudo apt install -y build-essential libcurl4-openssl-dev libsqlite3-dev libreadline-dev</div>
</div>
<h3>macOS</h3>
<pre>brew install curl sqlite readline</pre>
</section>

<section class="doc-section" id="installation">
<h2>Installation</h2>
<h3>Step 1: Clone</h3>
<pre>git clone https://github.com/t4tarzan/seabot.git seaclaw
cd seaclaw</pre>
<h3>Step 2: Build</h3>
<pre># Debug build (with AddressSanitizer)
make all

# Release build (optimized, stripped)
make release</pre>
<h3>Step 3: Test</h3>
<pre>make test
# Expected: 26 test suites, all passing, 0 failures</pre>
<h3>Step 4: Configure</h3>
<pre>cp config/config.example.json config.json
cat &gt; .env &lt;&lt; 'EOF'
OPENROUTER_API_KEY=sk-or-v1-your-key-here
TELEGRAM_BOT_TOKEN=123456:ABC-your-token-here
TELEGRAM_CHAT_ID=your-chat-id-here
EOF</pre>
<h3>Step 5: Run</h3>
<pre>./sea_claw --config config.json</pre>
</section>

<section class="doc-section" id="first-run">
<h2>First Run</h2>
<p>When you start Sea-Claw, you&rsquo;ll see a startup log. Here&rsquo;s what each line means:</p>
<div class="try-it">
<div class="try-it-header"><div class="dots"><div class="dot red"></div><div class="dot yellow"></div><div class="dot green"></div></div><div class="label">SEA-CLAW STARTUP</div></div>
<div class="try-it-body"><span class="output">  ____  ______  ___      ________  ___  ___      __
 / ___// ____/ / /  |   / ____/ / / /  | |     / /
 \\__ \\/ __/   / /|  |  / /   / / / /   | | /| / /
 ___/ / /___  / ___ | / /___/ /_/ /    | |/ |/ /
/____/_____/ /_/  |_| \\____/\\____/     |_/__|__/</span>

<span class="highlight">T+0ms</span>  [CONFIG]  INF: Loaded .env file: .env
<span class="highlight">T+0ms</span>  [SYSTEM]  INF: Substrate initializing. Arena: 16MB (Fixed).
<span class="highlight">T+0ms</span>  [HANDS]   INF: Tool registry loaded: 63 static tools
<span class="highlight">T+6ms</span>  [DB]      INF: Opened database: seaclaw.db
<span class="highlight">T+6ms</span>  [AGENT]   INF: Provider: OpenRouter, Model: moonshotai/kimi-k2.5
<span class="highlight">T+8ms</span>  [SHIELD]  INF: Grammar Filter: ACTIVE.
<span class="highlight">T+8ms</span>  [STATUS]  INF: Waiting for command... (Type /help)

<span class="prompt">&gt;</span> </div>
</div>

<details class="expand">
<summary>What does each line mean?</summary>
<div class="expand-content">
<ul>
  <li><strong>T+0ms</strong> &mdash; Time since startup in milliseconds. <code>T+6ms</code> means 6 thousandths of a second have passed.</li>
  <li><strong>[CONFIG]</strong> &mdash; The tag tells you which module is speaking. CONFIG = configuration, HANDS = tools, DB = database, etc.</li>
  <li><strong>INF</strong> &mdash; Log level. INF = information (normal), WRN = warning, ERR = error.</li>
  <li><strong>Arena: 16MB</strong> &mdash; Sea-Claw reserved 16 megabytes of memory upfront. This is its &ldquo;whiteboard&rdquo; for all temporary work.</li>
  <li><strong>63 static tools</strong> &mdash; All 63 tools are compiled into the binary. The AI can call any of them but cannot create new ones.</li>
  <li><strong>Grammar Filter: ACTIVE</strong> &mdash; The security system is running. Every input will be checked for injection attacks.</li>
</ul>
<p>Notice: the entire startup took <strong>8 milliseconds</strong>. That&rsquo;s faster than a single frame of a 120fps video game.</p>
</div>
</details>

<p>Now type <code>/help</code> to see all commands, or just ask a natural language question like &ldquo;What files are in the current directory?&rdquo;</p>
<div class="callout warn">
<div class="callout-title">API Key Required</div>
Sea-Claw needs at least one <span class="glossary" data-def="Large Language Model &mdash; an AI that understands and generates human language. Examples: GPT-4, Claude, Gemini. Sea-Claw uses these as its 'brain' for thinking.">LLM</span> API key to think. Without it, the tools still work but the AI brain has nothing to connect to. Get a free key from <a href="https://openrouter.ai" style="color:#58a6ff">OpenRouter</a> (many models have free tiers).
</div>
</section>

<section class="doc-section" id="configuration">
<h2>Configuration</h2>
<p>Sea-Claw has two config files &mdash; think of them as a <strong>public profile</strong> and a <strong>private keychain</strong>:</p>
<ul>
  <li><strong><code>config.json</code></strong> &mdash; Non-secret settings (which AI model to use, how creative it should be, how much memory to reserve). Safe to share.</li>
  <li><strong><code>.env</code></strong> &mdash; Secret keys (API passwords, bot tokens). <em>Never</em> share this file.</li>
</ul>
<h3>config.json</h3>
<pre>{
  "telegram_token": "",
  "telegram_chat_id": 0,
  "db_path": "seaclaw.db",
  "log_level": "info",
  "arena_size_mb": 16,
  "llm_provider": "openrouter",
  "llm_model": "moonshotai/kimi-k2.5",
  "llm_max_tokens": 1024,
  "llm_temperature": 0.3,
  "llm_fallbacks": [
    { "provider": "openai", "model": "gpt-4o-mini" },
    { "provider": "gemini", "model": "gemini-2.0-flash" }
  ]
}</pre>
<table>
<tr><th>Field</th><th>Type</th><th>Default</th><th>Description</th></tr>
<tr><td><code>llm_provider</code></td><td>string</td><td>"openrouter"</td><td>Primary LLM provider</td></tr>
<tr><td><code>llm_model</code></td><td>string</td><td>"moonshotai/kimi-k2.5"</td><td>Model name</td></tr>
<tr><td><code>llm_max_tokens</code></td><td>int</td><td>1024</td><td>Max words in AI response. Think of <span class="glossary" data-def="Tokens are word-pieces. 1 token is roughly 3/4 of a word. 1024 tokens is about 750 words &mdash; roughly 1.5 pages of text.">tokens</span> as word-pieces (~750 words).</td></tr>
<tr><td><code>llm_temperature</code></td><td>float</td><td>0.3</td><td>AI creativity dial. 0 = robotic and predictable (like a calculator). 1.0 = creative and surprising (like a poet). 0.3 is a good default.</td></tr>
<tr><td><code>arena_size_mb</code></td><td>int</td><td>16</td><td>Size of Sea-Claw&rsquo;s &ldquo;whiteboard&rdquo; for temporary work. 16 MB handles most tasks. Reduce to 4 for tiny devices.</td></tr>
<tr><td><code>db_path</code></td><td>string</td><td>"seaclaw.db"</td><td>SQLite file path</td></tr>
<tr><td><code>log_level</code></td><td>string</td><td>"info"</td><td>debug/info/warn/error</td></tr>
<tr><td><code>llm_fallbacks</code></td><td>array</td><td>[]</td><td>Fallback providers</td></tr>
</table>
</section>

<section class="doc-section" id="env-variables">
<h2>Environment Variables</h2>
<pre># LLM API Keys
OPENROUTER_API_KEY=sk-or-v1-...
OPENAI_API_KEY=sk-...
ANTHROPIC_API_KEY=sk-ant-...
GEMINI_API_KEY=AI...

# Telegram
TELEGRAM_BOT_TOKEN=123456:ABC...
TELEGRAM_CHAT_ID=890034905

# Optional
EXA_API_KEY=exa-...
SLACK_WEBHOOK_URL=https://hooks.slack.com/services/...
SEA_API_PORT=8899</pre>
<div class="callout danger">
<div class="callout-title">Never Commit .env</div>
The <code>.env</code> file is in <code>.gitignore</code>. If accidentally committed, rotate all keys immediately.
</div>
</section>

<section class="doc-section" id="docker-setup">
<h2>Docker Setup</h2>
<div class="callout info">
<div class="callout-title">What is Docker?</div>
<p>Think of <span class="glossary" data-def="Docker is a tool that packages software into 'containers' &mdash; like shipping containers for code. Everything the program needs is inside the container, so it runs the same everywhere.">Docker</span> as a shipping container for software. Instead of installing Sea-Claw directly on your computer, you put it in a container that has everything it needs. The container runs the same on any machine &mdash; your laptop, a server, or a Raspberry Pi. It&rsquo;s the easiest way to deploy Sea-Claw without worrying about dependencies.</p>
</div>
<pre># Build
docker build -t seaclaw .

# Run interactively
docker run -it --rm -v seaclaw-data:/root/.config/seaclaw \\
  -e OPENROUTER_API_KEY=sk-or-v1-... seaclaw

# Run as background Telegram bot
docker run -d --name seaclaw-bot --restart unless-stopped \\
  -v seaclaw-data:/root/.config/seaclaw --env-file .env seaclaw run</pre>
<table>
<tr><th>Stage</th><th>Base</th><th>Purpose</th><th>Size</th></tr>
<tr><td>Builder</td><td>debian:bookworm-slim</td><td>Compile</td><td>~350 MB</td></tr>
<tr><td>Runtime</td><td>debian:bookworm-slim</td><td>Binary + libs</td><td>~85 MB</td></tr>
</table>
</section>

<section class="doc-section" id="raspberry-pi">
<h2>Raspberry Pi Setup</h2>
<p>Sea-Claw runs on a Pi 4 (2GB) as a 24/7 Telegram bot on ~5 watts.</p>
<h3>Why Pi + Sea-Claw?</h3>
<ul>
  <li><strong>Always-on AI</strong> &mdash; 24/7 on ~$5/year electricity</li>
  <li><strong>Home automation</strong> &mdash; Control IoT via Telegram</li>
  <li><strong>Private server</strong> &mdash; Data never leaves your network</li>
</ul>
<h3>Setup</h3>
<pre>sudo apt update &amp;&amp; sudo apt install -y build-essential \\
  libcurl4-openssl-dev libsqlite3-dev libreadline-dev git
git clone https://github.com/t4tarzan/seabot.git seaclaw
cd seaclaw &amp;&amp; make release
sudo cp dist/sea_claw /usr/local/bin/</pre>
<h3>Run as systemd Service</h3>
<pre>sudo tee /etc/systemd/system/seaclaw.service &gt; /dev/null &lt;&lt; 'EOF'
[Unit]
Description=Sea-Claw AI Agent
After=network-online.target
[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/seaclaw
ExecStart=/usr/local/bin/sea_claw --config config.json
Restart=always
RestartSec=10
[Install]
WantedBy=multi-user.target
EOF
sudo systemctl enable seaclaw &amp;&amp; sudo systemctl start seaclaw</pre>
<h3>Performance</h3>
<table>
<tr><th>Metric</th><th>Pi 4 (2GB)</th><th>Pi 5 (8GB)</th></tr>
<tr><td>Build time</td><td>~25s</td><td>~12s</td></tr>
<tr><td>Binary size</td><td>~260 KB</td><td>~260 KB</td></tr>
<tr><td>Memory</td><td>~17 MB</td><td>~17 MB</td></tr>
<tr><td>Startup</td><td>~15 ms</td><td>~8 ms</td></tr>
</table>
</section>
`;
