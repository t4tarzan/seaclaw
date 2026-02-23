// Part 4: Core Modules + Intelligence Layer (enhanced with plain-English intros)

var CONTENT_MODULES = `
<section class="doc-section" id="mod-types">
<h2>sea_types &mdash; Foundation Types</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_types.h</code></p>
<p>Every building has a foundation. <code>sea_types</code> is Sea-Claw&rsquo;s foundation &mdash; it defines the basic building blocks that every other module uses. Think of it as the <strong>alphabet</strong> of the codebase: before you can write words (modules), you need letters (types).</p>
<p>The most important idea here is <strong>fixed-width types</strong>. In C, an <code>int</code> can be different sizes on different computers (like how a &ldquo;cup&rdquo; means different sizes at different coffee shops). Fixed-width types are like standardized measuring cups &mdash; <code>u32</code> is <em>always</em> 32 bits, everywhere, every time. This prevents subtle bugs when Sea-Claw runs on different hardware.</p>
<h3>Fixed-Width Types</h3>
<pre>typedef uint8_t  u8;    typedef int8_t   i8;
typedef uint16_t u16;   typedef int16_t  i16;
typedef uint32_t u32;   typedef int32_t  i32;
typedef uint64_t u64;   typedef int64_t  i64;
typedef float    f32;   typedef double   f64;</pre>
<h3>SeaSlice &mdash; Zero-Copy String View</h3>
<pre>typedef struct {
    const u8* data;   // Pointer to bytes (NOT null-terminated)
    u32       len;    // Length in bytes
} SeaSlice;

#define SEA_SLICE_EMPTY ((SeaSlice){NULL, 0})
#define SEA_SLICE_LIT(s) ((SeaSlice){(const u8*)(s), sizeof(s) - 1})</pre>
<p>A <code>SeaSlice</code> is a <strong>view</strong> into existing memory &mdash; it never owns or copies data. This is the key to performance.</p>
<h3>Error Codes (22 total)</h3>
<pre>SEA_OK, SEA_ERR_OOM, SEA_ERR_ARENA_FULL, SEA_ERR_IO,
SEA_ERR_EOF, SEA_ERR_TIMEOUT, SEA_ERR_PARSE,
SEA_ERR_INVALID_JSON, SEA_ERR_INVALID_INPUT,
SEA_ERR_GRAMMAR_REJECT, SEA_ERR_TOOL_NOT_FOUND,
SEA_ERR_TOOL_FAILED, SEA_ERR_CONFIG, SEA_ERR_DB,
SEA_ERR_AUTH, SEA_ERR_RATE_LIMIT, SEA_ERR_NOT_FOUND,
SEA_ERR_ALREADY_EXISTS, SEA_ERR_NOT_IMPLEMENTED,
SEA_ERR_INTERNAL, SEA_ERR_CANCELLED, SEA_ERR_UNKNOWN

// Convert to string:
const char* sea_error_str(SeaError err);
// sea_error_str(SEA_ERR_OOM) -> "out of memory"</pre>
</section>

<section class="doc-section" id="mod-arena">
<h2>sea_arena &mdash; Memory Allocator</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_arena.h</code> <span class="badge badge-green">9 tests</span></p>
<p>This is Sea-Claw&rsquo;s secret weapon. Most programs ask the operating system for memory one piece at a time (like ordering dishes one by one at a restaurant). Sea-Claw grabs <strong>one big block</strong> upfront (like reserving the whole buffet table) and hands out pieces from it.</p>
<p>The magic: when you&rsquo;re done, you don&rsquo;t return each piece individually. You just <strong>reset the whole block</strong> in one instant operation (11 nanoseconds). This makes memory leaks <em>physically impossible</em> &mdash; there&rsquo;s nothing to forget to free. It&rsquo;s like a whiteboard: write whatever you want, then erase the whole thing in one swipe.</p>
<h3>API</h3>
<pre>SeaError sea_arena_create(SeaArena* arena, u64 size);
void     sea_arena_destroy(SeaArena* arena);
void*    sea_arena_alloc(SeaArena* arena, u64 size, u64 align);
void*    sea_arena_push(SeaArena* arena, u64 size);
SeaSlice sea_arena_push_cstr(SeaArena* arena, const char* s);
void*    sea_arena_push_bytes(SeaArena* arena, const void* data, u64 len);
void     sea_arena_reset(SeaArena* arena);
u64      sea_arena_used(const SeaArena* arena);
u64      sea_arena_remaining(const SeaArena* arena);</pre>
<h3>Example</h3>
<pre>SeaArena arena;
sea_arena_create(&amp;arena, 1024 * 1024);  // 1 MB

char* buf = sea_arena_alloc(&amp;arena, 256, 1);
snprintf(buf, 256, "Hello from arena!");

SeaSlice name = sea_arena_push_cstr(&amp;arena, "Sea-Claw");
printf("Name: %.*s\\n", (int)name.len, (const char*)name.data);

sea_arena_reset(&amp;arena);   // instant reset
sea_arena_destroy(&amp;arena);</pre>
<h3>Edge Cases</h3>
<ul>
  <li><strong>Arena full</strong>: Returns <code>NULL</code>. Always check.</li>
  <li><strong>Zero-size alloc</strong>: Returns valid pointer (bookmark).</li>
  <li><strong>After reset</strong>: All previous pointers are invalid.</li>
</ul>
</section>

<section class="doc-section" id="mod-log">
<h2>sea_log &mdash; Structured Logging</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_log.h</code></p>
<p>Logging is Sea-Claw&rsquo;s <strong>diary</strong>. Every important event gets written down with a timestamp, a tag (which module is speaking), and a severity level. When something goes wrong, you read the diary to figure out what happened. It&rsquo;s like a flight recorder (black box) for your AI agent.</p>
<pre>// Output: T+&lt;ms&gt; [&lt;TAG&gt;] &lt;LEVEL&gt;: &lt;message&gt;
SEA_LOG_INFO("HANDS", "Tool registry loaded: %u tools", 63);
// -> T+0ms [HANDS] INF: Tool registry loaded: 63 tools</pre>
</section>

<section class="doc-section" id="mod-json">
<h2>sea_json &mdash; Zero-Copy JSON Parser</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_json.h</code> <span class="badge badge-green">17 tests</span></p>
<p><span class="glossary" data-def="JavaScript Object Notation &mdash; a way to structure data as labeled fields. Like a form with named boxes.">JSON</span> is how computers exchange structured data &mdash; like a form with labeled fields. Every LLM API speaks JSON. Sea-Claw&rsquo;s parser is special because it&rsquo;s <strong>zero-copy</strong>: instead of photocopying every page of a book, it just points to the original pages. This means no extra memory is used and parsing takes only 5.4 microseconds.</p>
<pre>SeaError sea_json_parse(SeaSlice input, SeaArena* arena, SeaJsonValue* out);
SeaSlice sea_json_get_string(const SeaJsonValue* obj, const char* key);
f64      sea_json_get_number(const SeaJsonValue* obj, const char* key, f64 fallback);
bool     sea_json_get_bool(const SeaJsonValue* obj, const char* key, bool fallback);</pre>
<h3>Example</h3>
<pre>const char* json = "{\\"name\\":\\"Sea-Claw\\",\\"tools\\":63}";
SeaSlice input = { .data = (const u8*)json, .len = strlen(json) };
SeaJsonValue root;
sea_json_parse(input, &amp;arena, &amp;root);
SeaSlice name = sea_json_get_string(&amp;root, "name"); // "Sea-Claw"
f64 tools = sea_json_get_number(&amp;root, "tools", 0);  // 63.0</pre>
</section>

<section class="doc-section" id="mod-shield">
<h2>sea_shield &mdash; Grammar Filter</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_shield.h</code> <span class="badge badge-green">19 tests</span></p>
<p>The Grammar Shield is Sea-Claw&rsquo;s <strong>immune system</strong>. Just like your body checks every molecule that enters for threats, the Shield checks every <em>byte</em> of input against a set of allowed character patterns. If something doesn&rsquo;t belong (like a shell command hidden inside a chat message), it&rsquo;s rejected instantly &mdash; in less than 1 microsecond. That&rsquo;s faster than a hummingbird flaps its wings once.</p>
<h3>10 Grammar Types</h3>
<pre>SEA_GRAMMAR_SAFE_TEXT   // Printable ASCII
SEA_GRAMMAR_NUMERIC     // Digits, dot, minus
SEA_GRAMMAR_ALPHA       // Letters only
SEA_GRAMMAR_ALPHANUM    // Letters + digits
SEA_GRAMMAR_FILENAME    // Alphanumeric + . - _ /
SEA_GRAMMAR_URL         // URL-safe chars
SEA_GRAMMAR_JSON        // Valid JSON chars
SEA_GRAMMAR_COMMAND     // / + alphanumeric
SEA_GRAMMAR_HEX         // 0-9, a-f, A-F
SEA_GRAMMAR_BASE64      // A-Z, a-z, 0-9, +, /, =</pre>
<h3>Example</h3>
<pre>SeaSlice input = SEA_SLICE_LIT("hello world");
if (sea_shield_detect_injection(input)) {
    return SEA_ERR_INVALID_INPUT;  // shell/SQL/XSS
}
if (!sea_shield_check(input, SEA_GRAMMAR_SAFE_TEXT)) {
    return SEA_ERR_GRAMMAR_REJECT; // invalid bytes
}</pre>
</section>

<section class="doc-section" id="mod-http">
<h2>sea_http &mdash; HTTP Client</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_http.h</code></p>
<p>This is Sea-Claw&rsquo;s <strong>telephone</strong> &mdash; how it calls the outside world. Every time Sea-Claw talks to an LLM API, fetches a webpage, or sends a Telegram message, it uses <code>sea_http</code>. Built on top of <span class="glossary" data-def="A battle-tested C library for making HTTP requests. Used by billions of devices worldwide, including cars, TVs, and phones.">libcurl</span>, which powers billions of devices worldwide.</p>
<pre>SeaError sea_http_get(const char* url, SeaArena* arena, SeaHttpResponse* resp);
SeaError sea_http_post_json(const char* url, SeaSlice body, SeaArena* arena, SeaHttpResponse* resp);
SeaError sea_http_post_json_auth(const char* url, SeaSlice body, const char* auth,
                                  SeaArena* arena, SeaHttpResponse* resp);
// SSE streaming (E13)
SeaError sea_http_post_stream(const char* url, SeaSlice body,
                               const char** headers, u32 header_count,
                               SeaStreamCb stream_cb, void* user_data,
                               SeaArena* arena, SeaHttpResponse* resp);</pre>
</section>

<section class="doc-section" id="mod-db">
<h2>sea_db &mdash; Database (SQLite)</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_db.h</code> <span class="badge badge-green">10 tests</span></p>
<p>Sea-Claw&rsquo;s <strong>filing cabinet</strong>. Everything that needs to survive a restart goes here: chat history, tasks, configuration, audit trail, and SeaZero agent data. It uses <span class="glossary" data-def="A tiny database engine that stores everything in a single file. No server needed. Used by Firefox, Android, and billions of devices.">SQLite</span> &mdash; a single file on disk, no database server needed. When Sea-Claw starts, it opens <code>seaclaw.db</code>. When it stops, the file stays. All your data is always there.</p>
<h3>Tables</h3>
<ul>
  <li><code>trajectory</code> &mdash; Audit log</li>
  <li><code>config</code> &mdash; Key-value store</li>
  <li><code>tasks</code> &mdash; Task manager</li>
  <li><code>chat_history</code> &mdash; Conversation memory</li>
  <li><code>usability_tests</code> &mdash; Integration test results</li>
  <li><code>seazero_agents</code>, <code>seazero_tasks</code>, <code>seazero_llm_usage</code></li>
</ul>
<h3>API Highlights</h3>
<pre>SeaError sea_db_open(SeaDb** db, const char* path);
void     sea_db_close(SeaDb* db);
SeaError sea_db_log_event(SeaDb* db, const char* type, const char* title, const char* content);
SeaError sea_db_config_set(SeaDb* db, const char* key, const char* value);
const char* sea_db_config_get(SeaDb* db, const char* key, SeaArena* arena);
SeaError sea_db_task_create(SeaDb* db, const char* title, const char* priority, const char* content);
SeaError sea_db_chat_log(SeaDb* db, i64 chat_id, const char* role, const char* content);</pre>
</section>

<section class="doc-section" id="mod-config">
<h2>sea_config &mdash; Configuration</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_config.h</code> <span class="badge badge-green">6 tests</span></p>
<p>Sea-Claw&rsquo;s <strong>settings panel</strong>. It reads your preferences from <code>config.json</code> (which AI model to use, how creative it should be) and secrets from <code>.env</code> (API keys, bot tokens). Think of <code>config.json</code> as your public profile and <code>.env</code> as your private keychain. Environment variables always override the JSON file, so you can change settings without editing files.</p>
<pre>SeaError sea_config_load(SeaConfig* cfg, const char* path, SeaArena* arena);</pre>
</section>

<section class="doc-section" id="mod-agent">
<h2>sea_agent &mdash; LLM Brain</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_agent.h</code></p>
<p>This is the <strong>brain</strong> of Sea-Claw &mdash; the most important module. It takes your question, packages it with context (conversation history, tool descriptions, system instructions), sends it to a cloud <span class="glossary" data-def="Large Language Model &mdash; an AI that understands and generates human language. Examples: GPT-4, Claude, Gemini.">LLM</span>, and interprets the response. If the LLM wants to use a tool, the agent executes it and loops back. It&rsquo;s like a project manager who talks to the client (you), consults the expert (LLM), and delegates work to specialists (tools).</p>
<p>The agent also has a <strong>fallback chain</strong>: if the primary LLM is down, it automatically tries the next one. Like having backup phone numbers for your most important contacts.</p>
<h3>Provider Chain</h3>
<pre>Primary: OpenRouter (moonshotai/kimi-k2.5)
    |  (on failure)
Fallback 1: OpenAI (gpt-4o-mini)
    |  (on failure)
Fallback 2: Gemini (gemini-2.0-flash)</pre>
<h3>API</h3>
<pre>void           sea_agent_init(SeaAgentConfig* cfg);
SeaAgentResult sea_agent_chat(SeaAgentConfig* cfg,
                               SeaChatMsg* history, u32 count,
                               const char* input, SeaArena* arena);</pre>
<div class="diagram">User Input
    |
    v
[Shield Validate] -- reject injection
    |
[Build Messages]  -- system prompt + history + input
    |
[Call LLM API]    -- with fallback chain
    |
[Parse Response]
    |
    +-- Tool call? --YES--> [Execute Tool] --> [Loop Again]
    +-- No ----------------> [Return Response]</div>
<h3>Streaming (E13)</h3>
<p>When <code>stream_cb</code> is set, injects <code>"stream":true</code> and uses SSE. Tokens delivered in real-time.</p>
</section>

<section class="doc-section" id="mod-tools">
<h2>sea_tools &mdash; Tool Registry</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_tools.h</code></p>
<p>The tool registry is Sea-Claw&rsquo;s <strong>vending machine</strong>. It has exactly 63 buttons, each wired to a specific function. The AI can press any button, but it <em>cannot add new buttons</em> or rewire existing ones. This is a critical security feature: even if the AI is tricked by a prompt injection, it can only call tools that were compiled into the binary. No <code>eval()</code>, no <code>exec()</code>, no dynamic code loading.</p>
<pre>typedef SeaError (*SeaToolFunc)(SeaSlice args, SeaArena* arena, SeaSlice* output);

u32      sea_tools_count(void);
SeaError sea_tool_exec(const char* name, SeaSlice args, SeaArena* arena, SeaSlice* output);</pre>
<h3>Adding a New Tool</h3>
<pre>// 1. Create src/hands/impl/tool_example.c
SeaError tool_example(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "Hello: %.*s",
                       (int)args.len, (const char*)args.data);
    u8* dst = sea_arena_push_bytes(arena, buf, len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst; output->len = len;
    return SEA_OK;
}
// 2. Register in sea_tools.c
// 3. Add to Makefile HANDS_SRC</pre>
</section>

<section class="doc-section" id="tool-catalog">
<h2>Tool Catalog (63 Tools)</h2>
<h3>File &amp; System</h3>
<div class="tool-grid">
  <div class="tool-card"><div class="tool-name">file_read</div><div class="tool-desc">Read file contents</div></div>
  <div class="tool-card"><div class="tool-name">file_write</div><div class="tool-desc">Write to file</div></div>
  <div class="tool-card"><div class="tool-name">edit_file</div><div class="tool-desc">Find/replace in file</div></div>
  <div class="tool-card"><div class="tool-name">file_info</div><div class="tool-desc">File metadata</div></div>
  <div class="tool-card"><div class="tool-name">file_search</div><div class="tool-desc">Search by pattern</div></div>
  <div class="tool-card"><div class="tool-name">dir_list</div><div class="tool-desc">List directory</div></div>
  <div class="tool-card"><div class="tool-name">shell_exec</div><div class="tool-desc">Run shell command</div></div>
  <div class="tool-card"><div class="tool-name">process_list</div><div class="tool-desc">Running processes</div></div>
  <div class="tool-card"><div class="tool-name">disk_usage</div><div class="tool-desc">Disk space</div></div>
  <div class="tool-card"><div class="tool-name">uptime</div><div class="tool-desc">System uptime</div></div>
  <div class="tool-card"><div class="tool-name">system_status</div><div class="tool-desc">Full health</div></div>
  <div class="tool-card"><div class="tool-name">syslog_read</div><div class="tool-desc">System logs</div></div>
  <div class="tool-card"><div class="tool-name">env_get</div><div class="tool-desc">Env variable</div></div>
  <div class="tool-card"><div class="tool-name">checksum_file</div><div class="tool-desc">File checksum</div></div>
</div>
<h3>Text Processing</h3>
<div class="tool-grid">
  <div class="tool-card"><div class="tool-name">grep_text</div><div class="tool-desc">Regex search</div></div>
  <div class="tool-card"><div class="tool-name">sort_text</div><div class="tool-desc">Sort lines</div></div>
  <div class="tool-card"><div class="tool-name">wc</div><div class="tool-desc">Word/line count</div></div>
  <div class="tool-card"><div class="tool-name">count_lines</div><div class="tool-desc">Line count</div></div>
  <div class="tool-card"><div class="tool-name">head_tail</div><div class="tool-desc">First/last N lines</div></div>
  <div class="tool-card"><div class="tool-name">diff_text</div><div class="tool-desc">Compare texts</div></div>
  <div class="tool-card"><div class="tool-name">string_replace</div><div class="tool-desc">Find &amp; replace</div></div>
  <div class="tool-card"><div class="tool-name">text_transform</div><div class="tool-desc">Upper/lower/reverse</div></div>
  <div class="tool-card"><div class="tool-name">text_summarize</div><div class="tool-desc">Summarize</div></div>
  <div class="tool-card"><div class="tool-name">regex_match</div><div class="tool-desc">Pattern matching</div></div>
</div>
<h3>Data &amp; Encoding</h3>
<div class="tool-grid">
  <div class="tool-card"><div class="tool-name">json_format</div><div class="tool-desc">Pretty-print JSON</div></div>
  <div class="tool-card"><div class="tool-name">json_query</div><div class="tool-desc">Query with path</div></div>
  <div class="tool-card"><div class="tool-name">json_to_csv</div><div class="tool-desc">JSON to CSV</div></div>
  <div class="tool-card"><div class="tool-name">csv_parse</div><div class="tool-desc">Parse CSV</div></div>
  <div class="tool-card"><div class="tool-name">encode_decode</div><div class="tool-desc">Base64, URL</div></div>
  <div class="tool-card"><div class="tool-name">hash_compute</div><div class="tool-desc">CRC32/DJB2/FNV</div></div>
  <div class="tool-card"><div class="tool-name">math_eval</div><div class="tool-desc">Math expression</div></div>
  <div class="tool-card"><div class="tool-name">unit_convert</div><div class="tool-desc">Unit conversion</div></div>
  <div class="tool-card"><div class="tool-name">timestamp</div><div class="tool-desc">Date/time ops</div></div>
  <div class="tool-card"><div class="tool-name">uuid_gen</div><div class="tool-desc">Generate UUID</div></div>
  <div class="tool-card"><div class="tool-name">random_gen</div><div class="tool-desc">Random number</div></div>
  <div class="tool-card"><div class="tool-name">password_gen</div><div class="tool-desc">Secure password</div></div>
</div>
<h3>Network &amp; Web</h3>
<div class="tool-grid">
  <div class="tool-card"><div class="tool-name">web_fetch</div><div class="tool-desc">Fetch URL</div></div>
  <div class="tool-card"><div class="tool-name">web_search</div><div class="tool-desc">Web search</div></div>
  <div class="tool-card"><div class="tool-name">http_request</div><div class="tool-desc">Custom HTTP</div></div>
  <div class="tool-card"><div class="tool-name">dns_lookup</div><div class="tool-desc">DNS resolution</div></div>
  <div class="tool-card"><div class="tool-name">ip_info</div><div class="tool-desc">IP geolocation</div></div>
  <div class="tool-card"><div class="tool-name">ssl_check</div><div class="tool-desc">SSL cert check</div></div>
  <div class="tool-card"><div class="tool-name">url_parse</div><div class="tool-desc">Parse URL</div></div>
  <div class="tool-card"><div class="tool-name">whois_lookup</div><div class="tool-desc">WHOIS info</div></div>
  <div class="tool-card"><div class="tool-name">net_info</div><div class="tool-desc">Network interfaces</div></div>
  <div class="tool-card"><div class="tool-name">weather</div><div class="tool-desc">Weather forecast</div></div>
  <div class="tool-card"><div class="tool-name">exa_search</div><div class="tool-desc">Neural search</div></div>
</div>
<h3>Productivity &amp; AI</h3>
<div class="tool-grid">
  <div class="tool-card"><div class="tool-name">task_manage</div><div class="tool-desc">Task CRUD</div></div>
  <div class="tool-card"><div class="tool-name">db_query</div><div class="tool-desc">SQLite (SELECT)</div></div>
  <div class="tool-card"><div class="tool-name">cron_manage</div><div class="tool-desc">Schedule jobs</div></div>
  <div class="tool-card"><div class="tool-name">memory_manage</div><div class="tool-desc">Workspace memory</div></div>
  <div class="tool-card"><div class="tool-name">recall</div><div class="tool-desc">Fact recall</div></div>
  <div class="tool-card"><div class="tool-name">echo</div><div class="tool-desc">Echo (testing)</div></div>
  <div class="tool-card"><div class="tool-name">message</div><div class="tool-desc">Send to channel</div></div>
  <div class="tool-card"><div class="tool-name">spawn</div><div class="tool-desc">Spawn sub-agent</div></div>
  <div class="tool-card"><div class="tool-name">agent_zero</div><div class="tool-desc">Delegate to A0</div></div>
</div>
<h3>Google Integrations</h3>
<div class="tool-grid">
  <div class="tool-card"><div class="tool-name">google_calendar_today</div><div class="tool-desc">Today's events</div></div>
  <div class="tool-card"><div class="tool-name">google_contacts_search</div><div class="tool-desc">Search contacts</div></div>
  <div class="tool-card"><div class="tool-name">google_drive_search</div><div class="tool-desc">Search Drive</div></div>
  <div class="tool-card"><div class="tool-name">google_gmail_search</div><div class="tool-desc">Search Gmail</div></div>
  <div class="tool-card"><div class="tool-name">google_tasks_list</div><div class="tool-desc">Google Tasks</div></div>
</div>
</section>

<section class="doc-section" id="mod-memory">
<h2>sea_memory &mdash; Workspace Memory</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_memory.h</code></p>
<p>LLMs have no memory between conversations &mdash; every chat starts from zero. <code>sea_memory</code> gives Sea-Claw a <strong>long-term notebook</strong>. It stores important facts, your preferences, and context in Markdown files (<code>IDENTITY.md</code>, <code>AGENTS.md</code>, <code>MEMORY.md</code>) that persist across sessions. When a new conversation starts, the agent reads these files to &ldquo;remember&rdquo; who you are and what you&rsquo;ve been working on. It&rsquo;s like a personal assistant who reviews their notes before each meeting.</p>
</section>

<section class="doc-section" id="mod-recall">
<h2>sea_recall &mdash; Fact Index</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_recall.h</code></p>
<p>While <code>sea_memory</code> is the notebook, <code>sea_recall</code> is the <strong>index at the back of the book</strong>. It stores categorized facts (like &ldquo;User prefers dark mode&rdquo; or &ldquo;Server IP is 10.0.0.5&rdquo;) and retrieves them by keyword. The recall system is budget-limited to 800 tokens &mdash; it picks the most relevant facts to inject into the system prompt, so the AI always has the right context without wasting token budget.</p>
</section>

<section class="doc-section" id="mod-skill">
<h2>sea_skill &mdash; Learned Skills</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_skill.h</code></p>
<p>Skills are Sea-Claw&rsquo;s <strong>muscle memory</strong>. When you teach Sea-Claw a multi-step procedure (like &ldquo;check all servers, compare disk usage, send a summary to Telegram&rdquo;), it can save that as a reusable skill. Next time, instead of figuring it out from scratch, it replays the learned steps. Like teaching someone to ride a bike &mdash; once learned, it becomes automatic.</p>
</section>

<section class="doc-section" id="mod-graph">
<h2>sea_graph &mdash; Knowledge Graph</h2>
<p><span class="badge badge-blue">Header</span> <code>include/seaclaw/sea_graph.h</code></p>
<p>The knowledge graph is Sea-Claw&rsquo;s <strong>mind map</strong>. It stores entities (people, projects, servers) and the relationships between them (Alice <em>manages</em> Project X, Server A <em>hosts</em> Database B). You can ask questions like &ldquo;What is connected to Server A?&rdquo; and get all related entities. It&rsquo;s like a detective&rsquo;s evidence board with strings connecting the clues.</p>
</section>

<section class="doc-section" id="sse-streaming">
<h2>SSE Streaming (E13)</h2>
<p>Without streaming, you send a question and wait... and wait... until the entire answer arrives at once. With <span class="glossary" data-def="Server-Sent Events &mdash; a way for a server to push data to a client in real-time, one piece at a time.">SSE</span> streaming, you see the answer being <strong>typed out in real-time</strong>, word by word, just like watching someone type in a chat. This makes Sea-Claw feel much more responsive, especially for long answers.</p>
<ol>
  <li>Agent injects <code>"stream":true</code> into request JSON</li>
  <li><code>sea_http_post_stream()</code> opens streaming HTTP connection</li>
  <li>Line-buffered SSE parser calls <code>stream_cb</code> for each <code>data:</code> line</li>
  <li>Callback extracts content deltas from OpenAI/Anthropic JSON chunks</li>
  <li>Tokens printed to user in real-time</li>
</ol>
<pre>// Enable in TUI:
/stream on

// Tokens appear as they arrive:
&gt; What is an arena allocator?
An arena allocator is a memory management... [real-time]</pre>
</section>
`;
