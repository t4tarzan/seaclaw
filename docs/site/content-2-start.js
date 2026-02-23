// Part 2: Getting Started sections

var CONTENT_START = `
<section class="doc-section" id="prerequisites">
<h2>Prerequisites</h2>
<table>
<tr><th>Requirement</th><th>Version</th><th>Why</th><th>Install</th></tr>
<tr><td>GCC or Clang</td><td>C11</td><td>Compiler</td><td><code>apt install build-essential</code></td></tr>
<tr><td>libcurl</td><td>7.x+</td><td>HTTP client</td><td><code>apt install libcurl4-openssl-dev</code></td></tr>
<tr><td>libsqlite3</td><td>3.x+</td><td>Database</td><td><code>apt install libsqlite3-dev</code></td></tr>
<tr><td>libreadline</td><td>8.x+</td><td>TUI (optional)</td><td><code>apt install libreadline-dev</code></td></tr>
</table>
<h3>One-Line Install (Debian/Ubuntu/Raspberry Pi OS)</h3>
<pre>sudo apt update &amp;&amp; sudo apt install -y build-essential libcurl4-openssl-dev libsqlite3-dev libreadline-dev</pre>
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
<p>When you start Sea-Claw for the first time:</p>
<pre>  ____  ______  ___      ________  ___  ___      __
 / ___// ____/ / /  |   / ____/ / / /  | |     / /
 \\__ \\/ __/   / /|  |  / /   / / / /   | | /| / /
 ___/ / /___  / ___ | / /___/ /_/ /    | |/ |/ /
/____/_____/ /_/  |_| \\____/\\____/     |_/__|__/

T+0ms  [CONFIG]  INF: Loaded .env file: .env
T+0ms  [SYSTEM]  INF: Substrate initializing. Arena: 16MB (Fixed).
T+0ms  [HANDS]   INF: Tool registry loaded: 63 static tools
T+6ms  [DB]      INF: Opened database: seaclaw.db
T+6ms  [AGENT]   INF: Provider: OpenRouter, Model: moonshotai/kimi-k2.5
T+8ms  [SHIELD]  INF: Grammar Filter: ACTIVE.
T+8ms  [STATUS]  INF: Waiting for command... (Type /help)

&gt; </pre>
<p>Type <code>/help</code> to see all commands, or just ask a natural language question.</p>
<div class="callout warn">
<div class="callout-title">API Key Required</div>
Sea-Claw needs at least one LLM API key. Get a free one from <a href="https://openrouter.ai" style="color:#58a6ff">OpenRouter</a>.
</div>
</section>

<section class="doc-section" id="configuration">
<h2>Configuration</h2>
<p>Two sources: <code>config.json</code> (non-secret) and <code>.env</code> (secrets).</p>
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
<tr><td><code>llm_max_tokens</code></td><td>int</td><td>1024</td><td>Max response tokens</td></tr>
<tr><td><code>llm_temperature</code></td><td>float</td><td>0.3</td><td>Creativity (0=deterministic)</td></tr>
<tr><td><code>arena_size_mb</code></td><td>int</td><td>16</td><td>Session arena size</td></tr>
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
