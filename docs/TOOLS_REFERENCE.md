# Sea-Claw Tools Reference

> Complete documentation for all 50 tools compiled into Sea-Claw v1.0.0

---

## Table of Contents

- [Overview](#overview)
- [Tool Signature](#tool-signature)
- [Tool Categories](#tool-categories)
- [Core Tools (1-2)](#core-tools)
- [File I/O Tools (3-4, 15-16, 40-41)](#file-io-tools)
- [System Tools (5, 17, 14, 34-35, 42)](#system-tools)
- [Network Tools (6, 18, 32, 37, 43-45, 47)](#network-tools)
- [Search Tools (9)](#search-tools)
- [Text Processing Tools (10-11, 25, 27-31, 38)](#text-processing-tools)
- [Data Processing Tools (12, 26, 36, 46)](#data-processing-tools)
- [Encoding & Hashing Tools (13, 23-24)](#encoding--hashing-tools)
- [Math & Utility Tools (19-22, 33, 39, 48-50)](#math--utility-tools)
- [Task & Database Tools (7-8)](#task--database-tools)

---

## Overview

All 50 tools share a single function signature and are registered at compile time in `src/hands/sea_tools.c`. Tools are invoked via:

- **Telegram:** `/exec <tool_name> <args>`
- **TUI:** `/exec <tool_name> <args>`
- **LLM Agent:** Automatic tool calling from natural language
- **C API:** `sea_tool_exec("tool_name", args, &arena, &output)`

## Tool Signature

Every tool implements this exact signature:

```c
SeaError tool_name(SeaSlice args, SeaArena* arena, SeaSlice* output);
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `args` | `SeaSlice` | Input arguments as raw bytes |
| `arena` | `SeaArena*` | Arena for output allocation |
| `output` | `SeaSlice*` | Pointer to write result into |
| **return** | `SeaError` | `SEA_OK` on success |

**Convention:** Tools never malloc. All output is written into the arena. Tools return `SEA_OK` even for user errors (writing the error message into `output`), reserving non-OK returns for system failures like `SEA_ERR_ARENA_FULL`.

## Tool Categories

| Category | Count | Tools |
|----------|-------|-------|
| Core | 2 | echo, system_status |
| File I/O | 6 | file_read, file_write, dir_list, file_info, checksum_file, file_search |
| System | 6 | shell_exec, process_list, env_get, disk_usage, syslog_read, uptime |
| Network | 8 | web_fetch, dns_lookup, net_info, http_request, ip_info, whois_lookup, ssl_check, weather |
| Search | 1 | exa_search |
| Text | 9 | text_summarize, text_transform, regex_match, diff_text, grep_text, wc, head_tail, sort_text, string_replace |
| Data | 4 | json_format, csv_parse, json_query, json_to_csv |
| Encoding | 3 | hash_compute, url_parse, encode_decode |
| Math/Utility | 7 | timestamp, math_eval, uuid_gen, random_gen, cron_parse, calendar, unit_convert |
| Security | 1 | password_gen |
| Dev | 1 | count_lines |
| Tasks/DB | 2 | task_manage, db_query |

---

## Core Tools

### Tool 1: `echo`

**File:** `src/hands/impl/tool_echo.c`  
**Category:** Core  
**Args:** Any text  
**Returns:** The same text back  

The simplest tool — validates the tool pipeline works end-to-end.

```
/exec echo Hello, Sea-Claw!
→ Hello, Sea-Claw!
```

**Implementation:**
```c
SeaError tool_echo(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("(empty)");
        return SEA_OK;
    }
    u8* dst = (u8*)sea_arena_push_bytes(arena, args.data, args.len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst;
    output->len  = args.len;
    return SEA_OK;
}
```

---

### Tool 2: `system_status`

**File:** `src/hands/impl/tool_system_status.c`  
**Category:** Core  
**Args:** (none)  
**Returns:** Memory usage, uptime, arena stats  

```
/exec system_status
→ Sea-Claw v1.0.0
  Uptime: 0 days, 2 hours, 15 minutes
  Arena: 0.1% used (1024 / 1048576 bytes)
  Tools: 50 registered
```

---

## File I/O Tools

### Tool 3: `file_read`

**File:** `src/hands/impl/tool_file_read.c`  
**Category:** File I/O  
**Args:** `<file_path>`  
**Returns:** File contents (truncated to 8KB)  
**Security:** Path validated by Shield  

```
/exec file_read /etc/hostname
→ seaclaw-server

/exec file_read /root/seaclaw/README.md
→ # Sea-Claw Sovereign System
  ...
```

---

### Tool 4: `file_write`

**File:** `src/hands/impl/tool_file_write.c`  
**Category:** File I/O  
**Args:** `<path>|<content>` (pipe-separated)  
**Returns:** Confirmation with bytes written  
**Security:** Path validated by Shield. Dangerous paths rejected.  

```
/exec file_write /tmp/test.txt|Hello from Sea-Claw!
→ Written 20 bytes to /tmp/test.txt
```

---

### Tool 15: `dir_list`

**File:** `src/hands/impl/tool_dir_list.c`  
**Category:** File I/O  
**Args:** `<directory_path>`  
**Returns:** Directory listing with file sizes and types  

```
/exec dir_list /root/seaclaw/src/core
→ Directory: /root/seaclaw/src/core
    sea_arena.c     4,521 bytes
    sea_config.c    3,102 bytes
    sea_db.c        5,890 bytes
    sea_log.c       1,234 bytes
  (4 entries)
```

---

### Tool 16: `file_info`

**File:** `src/hands/impl/tool_file_info.c`  
**Category:** File I/O  
**Args:** `<file_path>`  
**Returns:** File metadata (size, permissions, timestamps, type)  

```
/exec file_info /root/seaclaw/sea_claw
→ File: /root/seaclaw/sea_claw
  Size:     82,944 bytes
  Type:     regular file
  Perms:    -rwxr-xr-x
  Modified: 2026-02-11 16:00:00
```

---

### Tool 40: `checksum_file`

**File:** `src/hands/impl/tool_checksum_file.c`  
**Category:** File I/O / Security  
**Args:** `<filepath>`  
**Returns:** CRC32 and FNV-1a checksums of the file  

Useful for verifying file integrity and detecting changes.

```
/exec checksum_file /root/seaclaw/sea_claw
→ File: /root/seaclaw/sea_claw
    Size:   82944 bytes
    CRC32:  a1b2c3d4
    FNV-1a: 0123456789abcdef
```

**Implementation (core loop):**
```c
u32 crc = 0xFFFFFFFF;
u64 fnv = 0xcbf29ce484222325ULL;
while ((n = fread(buf, 1, sizeof(buf), fp)) > 0) {
    for (size_t i = 0; i < n; i++) {
        crc ^= buf[i];
        for (int j = 0; j < 8; j++)
            crc = (crc >> 1) ^ (0xEDB88320 & (-(crc & 1)));
        fnv ^= buf[i];
        fnv *= 0x100000001b3ULL;
    }
}
crc = ~crc;
```

---

### Tool 41: `file_search`

**File:** `src/hands/impl/tool_file_search.c`  
**Category:** File I/O  
**Args:** `<pattern> [directory]`  
**Returns:** Matching files with sizes (max 30 results, depth 5)  

```
/exec file_search .c /root/seaclaw/src
→ Search: '*.c*' in /root/seaclaw/src
      4521  /root/seaclaw/src/core/sea_arena.c
      3102  /root/seaclaw/src/core/sea_config.c
      ...
  (50 files found)

/exec file_search config /root/seaclaw
→ Search: '*config*' in /root/seaclaw
       892  /root/seaclaw/config/config.example.json
      3102  /root/seaclaw/src/core/sea_config.c
  (2 files found)
```

---

## System Tools

### Tool 5: `shell_exec`

**File:** `src/hands/impl/tool_shell_exec.c`  
**Category:** System  
**Args:** `<command>`  
**Returns:** stdout + stderr (truncated to 8KB)  
**Security:** Command validated by Shield. Dangerous patterns rejected (rm -rf, mkfs, etc.)  

```
/exec shell_exec uname -a
→ Linux seaclaw 5.15.0 #1 SMP x86_64 GNU/Linux

/exec shell_exec ls -la /root/seaclaw/src/core/
→ total 56
  -rw-r--r-- 1 root root 4521 Feb 11 sea_arena.c
  ...
```

---

### Tool 14: `env_get`

**File:** `src/hands/impl/tool_env_get.c`  
**Category:** System  
**Args:** `<VAR_NAME>`  
**Returns:** Environment variable value (whitelisted variables only)  
**Security:** Only safe variables exposed. API keys are masked.  

```
/exec env_get HOME
→ /root

/exec env_get PATH
→ /usr/local/bin:/usr/bin:/bin
```

---

### Tool 17: `process_list`

**File:** `src/hands/impl/tool_process_list.c`  
**Category:** System  
**Args:** `[filter]` (optional process name filter)  
**Returns:** Top processes by CPU/memory usage  

```
/exec process_list
→ PID   CPU%  MEM%  COMMAND
  1234  2.1   0.5   sea_claw
  5678  1.0   3.2   postgres
  ...

/exec process_list sea_claw
→ PID   CPU%  MEM%  COMMAND
  1234  2.1   0.5   sea_claw
```

---

### Tool 34: `disk_usage`

**File:** `src/hands/impl/tool_disk_usage.c`  
**Category:** System  
**Args:** `[path]` (default: /)  
**Returns:** Filesystem usage with human-readable sizes  

```
/exec disk_usage
→ Filesystem      Size   Used  Avail  Use%  Mounted
  /dev/sda1       78G    23G    51G   31%   /
  tmpfs           3.9G   0      3.9G  0%    /dev/shm

/exec disk_usage /root
→ /root: 1.2G used
```

---

### Tool 35: `syslog_read`

**File:** `src/hands/impl/tool_syslog_read.c`  
**Category:** System  
**Args:** `[lines] [filter]` (default: 20 lines, no filter)  
**Returns:** Recent syslog/journal entries  

```
/exec syslog_read
→ (last 20 syslog entries)

/exec syslog_read 50
→ (last 50 entries)

/exec syslog_read 30 error
→ (last 30 entries containing "error")
```

---

### Tool 42: `uptime`

**File:** `src/hands/impl/tool_uptime.c`  
**Category:** System  
**Args:** (none)  
**Returns:** System uptime, load averages, RAM usage  

Uses `sysinfo()` system call — no shell commands.

```
/exec uptime
→ System Uptime:
    Up:       12 days, 5 hours, 30 minutes
    Load:     0.15, 0.10, 0.08 (1m, 5m, 15m)
    Procs:    142 running
    RAM:      3200 / 8192 MB (39.1% used)
```

**Implementation:**
```c
struct sysinfo si;
sysinfo(&si);
long days  = si.uptime / 86400;
long hours = (si.uptime % 86400) / 3600;
long mins  = (si.uptime % 3600) / 60;
// Load averages are fixed-point: divide by 65536
double load1 = si.loads[0] / 65536.0;
```

---

## Network Tools

### Tool 6: `web_fetch`

**File:** `src/hands/impl/tool_web_fetch.c`  
**Category:** Network  
**Args:** `<url>`  
**Returns:** HTTP response body (truncated to 8KB)  

```
/exec web_fetch https://httpbin.org/ip
→ {"origin": "46.225.121.221"}
```

---

### Tool 18: `dns_lookup`

**File:** `src/hands/impl/tool_dns_lookup.c`  
**Category:** Network  
**Args:** `<hostname>`  
**Returns:** Resolved IP addresses  

```
/exec dns_lookup google.com
→ google.com:
    142.250.185.78 (IPv4)
    2a00:1450:4001:82a::200e (IPv6)
```

---

### Tool 32: `net_info`

**File:** `src/hands/impl/tool_net_info.c`  
**Category:** System / Network  
**Args:** `<interfaces|ip|ping <host>|ports>`  
**Returns:** Network information  

```
/exec net_info interfaces
→ eth0: 10.0.0.5/24, UP

/exec net_info ip
→ Public IP: 46.225.121.221

/exec net_info ping google.com
→ PING google.com: 64 bytes, time=12.3ms

/exec net_info ports
→ Active listening ports:
  tcp  0.0.0.0:22    LISTEN
  tcp  0.0.0.0:5432  LISTEN
```

---

### Tool 37: `http_request`

**File:** `src/hands/impl/tool_http_request.c`  
**Category:** Network  
**Args:** `<GET|POST|HEAD> <url> [body]`  
**Returns:** Status code, headers, and body  

```
/exec http_request GET https://httpbin.org/get
→ HTTP 200
  {"args":{},"headers":{...},"origin":"46.225.121.221",...}

/exec http_request POST https://httpbin.org/post {"key":"value"}
→ HTTP 200
  {"data":"{\"key\":\"value\"}","json":{"key":"value"},...}

/exec http_request HEAD https://example.com
→ HTTP 200
  Content-Type: text/html
  Content-Length: 1256
```

---

### Tool 43: `ip_info`

**File:** `src/hands/impl/tool_ip_info.c`  
**Category:** Network  
**Args:** `[ip_address]` (default: your public IP)  
**Returns:** IP geolocation data from ip-api.com (free, no key needed)  

```
/exec ip_info
→ IP: 46.225.121.221
    Country:  Germany
    Region:   Bavaria
    City:     Nuremberg
    ISP:      Hetzner Online GmbH
    Org:      Hetzner
    Timezone: Europe/Berlin

/exec ip_info 8.8.8.8
→ IP: 8.8.8.8
    Country:  United States
    City:     Mountain View
    ISP:      Google LLC
```

---

### Tool 44: `whois_lookup`

**File:** `src/hands/impl/tool_whois_lookup.c`  
**Category:** Network  
**Args:** `<domain>`  
**Returns:** WHOIS registration data (registrar, dates, nameservers)  

```
/exec whois_lookup google.com
→ WHOIS: google.com
  Registrar: MarkMonitor Inc.
  Creation Date: 1997-09-15
  Registry Expiry Date: 2028-09-14
  Name Server: ns1.google.com
```

---

### Tool 45: `ssl_check`

**File:** `src/hands/impl/tool_ssl_check.c`  
**Category:** Network / Security  
**Args:** `<domain>`  
**Returns:** SSL certificate details (issuer, expiry, subject, serial)  

```
/exec ssl_check github.com
→ SSL Certificate: github.com
    subject=CN = github.com
    issuer=C = US, O = DigiCert Inc, CN = DigiCert SHA2 High Assurance
    notBefore=Feb  1 00:00:00 2024 GMT
    notAfter=Mar  1 23:59:59 2025 GMT
    serial=0A1B2C3D4E5F
```

---

### Tool 47: `weather`

**File:** `src/hands/impl/tool_weather.c`  
**Category:** Network / Utility  
**Args:** `<city_name>`  
**Returns:** Current weather from wttr.in (free, no API key)  

```
/exec weather London
→ Weather for London:
    London: Partly cloudy +12C 65% ↗15km/h 0.0mm

/exec weather Tokyo
→ Weather for Tokyo:
    Tokyo: Clear +8C 45% →5km/h 0.0mm
```

---

## Search Tools

### Tool 9: `exa_search`

**File:** `src/hands/impl/tool_exa_search.c`  
**Category:** Search  
**Args:** `<search_query>`  
**Returns:** Top web search results from Exa API  
**Requires:** `EXA_API_KEY` environment variable  

```
/exec exa_search "Rust vs C performance benchmarks 2024"
→ Results:
  1. "Rust vs C: Performance Comparison" — https://...
  2. "Benchmarking Systems Languages" — https://...
  ...
```

---

## Text Processing Tools

### Tool 10: `text_summarize`

**File:** `src/hands/impl/tool_text_summarize.c`  
**Category:** Text  
**Args:** `<text>`  
**Returns:** Character count, word count, line count, preview  

```
/exec text_summarize "Hello world\nThis is a test\nThree lines"
→ Characters: 38
  Words: 8
  Lines: 3
  Preview: Hello world...
```

---

### Tool 11: `text_transform`

**File:** `src/hands/impl/tool_text_transform.c`  
**Category:** Text  
**Args:** `<operation> <text>`  
**Operations:** `upper`, `lower`, `reverse`, `length`, `trim`, `base64enc`, `base64dec`  

```
/exec text_transform upper hello world
→ HELLO WORLD

/exec text_transform reverse Sea-Claw
→ walC-aeS

/exec text_transform base64enc "Hello, World!"
→ SGVsbG8sIFdvcmxkIQ==

/exec text_transform base64dec SGVsbG8sIFdvcmxkIQ==
→ Hello, World!
```

---

### Tool 25: `regex_match`

**File:** `src/hands/impl/tool_regex_match.c`  
**Category:** Text Processing  
**Args:** `<pattern> <text>`  
**Returns:** All matches with positions  
**Engine:** POSIX Extended Regular Expressions (ERE)  

```
/exec regex_match [0-9]+ "There are 42 cats and 7 dogs"
→ Pattern: [0-9]+
  Match 1: "42" at position 10-12
  Match 2: "7" at position 22-23
  (2 matches)

/exec regex_match https?://[^ ]+ "Visit https://example.com today"
→ Pattern: https?://[^ ]+
  Match 1: "https://example.com" at position 6-25
  (1 match)
```

---

### Tool 27: `diff_text`

**File:** `src/hands/impl/tool_diff_text.c`  
**Category:** Text Processing  
**Args:** `<text1>|||<text2>` (separated by `|||`)  
**Returns:** Line-by-line comparison showing additions and removals  

```
/exec diff_text "hello\nworld\nfoo"|||"hello\nearth\nfoo\nbar"
→ Diff:
    hello     hello
  - world
  + earth
    foo       foo
  +           bar
```

---

### Tool 28: `grep_text`

**File:** `src/hands/impl/tool_grep_text.c`  
**Category:** Text Processing  
**Args:** `<pattern> <text_or_filepath>`  
**Returns:** Matching lines with line numbers  

```
/exec grep_text error /var/log/syslog
→ 142: Feb 11 10:23:15 error: connection refused
  287: Feb 11 11:45:02 error: timeout
  (2 matches)

/exec grep_text TODO "line1\nTODO fix this\nline3\nTODO refactor"
→ 2: TODO fix this
  4: TODO refactor
  (2 matches)
```

---

### Tool 29: `wc`

**File:** `src/hands/impl/tool_wc.c`  
**Category:** Text Processing  
**Args:** `<filepath_or_text>`  
**Returns:** Lines, words, characters, bytes count  

```
/exec wc /root/seaclaw/src/main.c
→ File: /root/seaclaw/src/main.c
    Lines: 803
    Words: 2,456
    Chars: 28,901
    Bytes: 28,901

/exec wc "hello world\nfoo bar baz"
→ Lines: 2
  Words: 5
  Chars: 23
  Bytes: 23
```

---

### Tool 30: `head_tail`

**File:** `src/hands/impl/tool_head_tail.c`  
**Category:** Text Processing  
**Args:** `<head|tail> [N] <path_or_text>`  
**Returns:** First or last N lines (default: 10)  

```
/exec head_tail head 5 /root/seaclaw/src/main.c
→ 1: /*
  2:  * main.c — The Nervous System
  3:  *
  4:  * Sea-Claw Sovereign Terminal entry point.
  5:  * Single-threaded event loop. Arena-based memory.

/exec head_tail tail 3 /root/seaclaw/src/main.c
→ 801:     return ret;
  802: }
  803:
```

---

### Tool 31: `sort_text`

**File:** `src/hands/impl/tool_sort_text.c`  
**Category:** Text Processing  
**Args:** `[options] <text>`  
**Options:** `-r` (reverse), `-n` (numeric), `-u` (unique)  

```
/exec sort_text "banana\napple\ncherry"
→ apple
  banana
  cherry

/exec sort_text -n "10\n2\n30\n1"
→ 1
  2
  10
  30

/exec sort_text -u "a\nb\na\nc\nb"
→ a
  b
  c
```

---

### Tool 38: `string_replace`

**File:** `src/hands/impl/tool_string_replace.c`  
**Category:** Text Processing  
**Args:** `<find>|||<replace>|||<text>` (separated by `|||`)  
**Returns:** Text with replacements + occurrence count  

```
/exec string_replace foo|||bar|||"foo is great, foo is fun"
→ bar is great, bar is fun
  (2 replacements)

/exec string_replace http|||https|||"visit http://example.com and http://test.com"
→ visit https://example.com and https://test.com
  (2 replacements)
```

---

## Data Processing Tools

### Tool 12: `json_format`

**File:** `src/hands/impl/tool_json_format.c`  
**Category:** Data Processing  
**Args:** `<json_string>`  
**Returns:** Pretty-printed JSON with indentation  

```
/exec json_format {"name":"Sea-Claw","tools":50}
→ {
    "name": "Sea-Claw",
    "tools": 50
  }
```

---

### Tool 26: `csv_parse`

**File:** `src/hands/impl/tool_csv_parse.c`  
**Category:** Data Processing  
**Args:** `<headers|count|col_num> <csv_data>`  
**Returns:** Parsed CSV information  

```
/exec csv_parse headers "name,age,city\nAlice,30,London\nBob,25,Paris"
→ Headers: name, age, city

/exec csv_parse count "name,age\nAlice,30\nBob,25\nCharlie,35"
→ Rows: 3 (excluding header)

/exec csv_parse 2 "name,age,city\nAlice,30,London\nBob,25,Paris"
→ Column 2 (age):
  30
  25
```

---

### Tool 36: `json_query`

**File:** `src/hands/impl/tool_json_query.c`  
**Category:** Data Processing  
**Args:** `<key.path> <json>`  
**Returns:** Value at the specified JSON path  

Supports dot-notation and array indexing.

```
/exec json_query name {"name":"Sea-Claw","version":"1.0"}
→ "Sea-Claw"

/exec json_query users.0.name {"users":[{"name":"Alice"},{"name":"Bob"}]}
→ "Alice"

/exec json_query config.arena_mb {"config":{"arena_mb":16,"debug":true}}
→ 16
```

---

### Tool 46: `json_to_csv`

**File:** `src/hands/impl/tool_json_to_csv.c`  
**Category:** Data Processing  
**Args:** `<json_array_of_objects>`  
**Returns:** CSV with headers from first object's keys  

```
/exec json_to_csv [{"name":"Alice","age":30},{"name":"Bob","age":25}]
→ name,age
  Alice,30
  Bob,25
```

**Implementation (key logic):**
```c
// Header row from first object's keys
for (u32 k = 0; k < first->object.count; k++) {
    if (k > 0) buf[pos++] = ',';
    pos = append_csv_field(buf, pos, MAX_OUTPUT, first->object.keys[k]);
}
// Data rows
for (u32 r = 0; r < root.array.count; r++) {
    // Match each key from header in current row
}
```

---

## Encoding & Hashing Tools

### Tool 13: `hash_compute`

**File:** `src/hands/impl/tool_hash_compute.c`  
**Category:** Encoding / Security  
**Args:** `<crc32|djb2|fnv1a> <text>`  
**Returns:** Hash value in hex  

All hash algorithms implemented in pure C — no OpenSSL dependency.

```
/exec hash_compute crc32 "Hello, World!"
→ CRC32: ec4ac3d0

/exec hash_compute djb2 "Hello, World!"
→ DJB2: 0beec7b5ea3f0fdb

/exec hash_compute fnv1a "Hello, World!"
→ FNV-1a: cbf29ce484222325
```

---

### Tool 23: `url_parse`

**File:** `src/hands/impl/tool_url_parse.c`  
**Category:** Encoding  
**Args:** `<url>`  
**Returns:** Parsed URL components (scheme, host, port, path, query, fragment)  

```
/exec url_parse https://api.example.com:8080/v1/search?q=test&limit=10#results
→ Scheme:   https
  Host:     api.example.com
  Port:     8080
  Path:     /v1/search
  Query:    q=test&limit=10
  Fragment: results
```

---

### Tool 24: `encode_decode`

**File:** `src/hands/impl/tool_encode_decode.c`  
**Category:** Encoding  
**Args:** `<urlencode|urldecode|htmlencode|htmldecode> <text>`  
**Returns:** Encoded/decoded text  

```
/exec encode_decode urlencode "hello world & foo=bar"
→ hello%20world%20%26%20foo%3Dbar

/exec encode_decode urldecode "hello%20world%20%26%20foo%3Dbar"
→ hello world & foo=bar

/exec encode_decode htmlencode "<script>alert('xss')</script>"
→ &lt;script&gt;alert(&#39;xss&#39;)&lt;/script&gt;

/exec encode_decode htmldecode "&lt;b&gt;bold&lt;/b&gt;"
→ <b>bold</b>
```

---

## Math & Utility Tools

### Tool 19: `timestamp`

**File:** `src/hands/impl/tool_timestamp.c`  
**Category:** Utility  
**Args:** `[unix|iso|utc|date]` (optional, default: all formats)  
**Returns:** Current time in requested format  

```
/exec timestamp
→ Unix:  1739290800
  ISO:   2026-02-11T16:00:00+0000
  UTC:   Tue, 11 Feb 2026 16:00:00 UTC
  Date:  2026-02-11

/exec timestamp unix
→ 1739290800
```

---

### Tool 20: `math_eval`

**File:** `src/hands/impl/tool_math_eval.c`  
**Category:** Math  
**Args:** `<expression>` (e.g., `2+3*4`)  
**Returns:** Evaluated result  
**Supports:** `+`, `-`, `*`, `/`, `%`, parentheses, integers, decimals  

Recursive descent parser — no `eval()` or shell.

```
/exec math_eval 2 + 3 * 4
→ 14

/exec math_eval (10 + 5) / 3
→ 5

/exec math_eval 3.14 * 2.5 * 2.5
→ 19.625
```

---

### Tool 21: `uuid_gen`

**File:** `src/hands/impl/tool_uuid_gen.c`  
**Category:** Utility  
**Args:** `[count]` (default: 1, max: 10)  
**Returns:** UUID v4 (random, from `/dev/urandom`)  

```
/exec uuid_gen
→ f47ac10b-58cc-4372-a567-0e02b2c3d479

/exec uuid_gen 3
→ f47ac10b-58cc-4372-a567-0e02b2c3d479
  7c9e6679-7425-40de-944b-e07fc1f90ae7
  550e8400-e29b-41d4-a716-446655440000
```

---

### Tool 22: `random_gen`

**File:** `src/hands/impl/tool_random_gen.c`  
**Category:** Utility  
**Args:** `<number [min] [max]>` | `<string [length]>` | `<hex [length]>` | `<coin>` | `<dice [sides]>`  
**Returns:** Random value from `/dev/urandom`  

```
/exec random_gen number 1 100
→ 42

/exec random_gen string 16
→ aB3xK9mPqR2wYz7n

/exec random_gen hex 32
→ a1b2c3d4e5f6a7b8c9d0e1f2a3b4c5d6

/exec random_gen coin
→ Heads

/exec random_gen dice 20
→ 17 (d20)
```

---

### Tool 33: `cron_parse`

**File:** `src/hands/impl/tool_cron_parse.c`  
**Category:** System / Utility  
**Args:** `<cron_expression>` (5 fields: min hour dom mon dow)  
**Returns:** Human-readable explanation of the schedule  

```
/exec cron_parse "0 9 * * 1-5"
→ At 09:00, Monday through Friday

/exec cron_parse "0 0 1 * *"
→ At midnight on the 1st of every month
```

**Implementation (custom `str_to_int` to avoid stdlib.h conflict):**
```c
static int str_to_int(const char* s) {
    int v = 0, neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') v = v * 10 + (*s++ - '0');
    return neg ? -v : v;
}
```

---

### Tool 39: `calendar`

**File:** `src/hands/impl/tool_calendar.c`  
**Category:** Utility  
**Args:** `[month year]` | `weekday <date>` | `diff <date1> <date2>`  
**Returns:** Calendar view, weekday lookup, or date difference  

```
/exec calendar
→ (current month calendar)

/exec calendar 2 2026
→     February 2026
  Su Mo Tu We Th Fr Sa
   1  2  3  4  5  6  7
   8  9 10 11 12 13 14
  15 16 17 18 19 20 21
  22 23 24 25 26 27 28

/exec calendar weekday 2026-02-11
→ Wednesday
```

---

### Tool 48: `unit_convert`

**File:** `src/hands/impl/tool_unit_convert.c`  
**Category:** Math / Utility  
**Args:** `<value> <from_unit> <to_unit>`  
**Returns:** Converted value  

**Supported Units:**
| Category | Units |
|----------|-------|
| Length | km, m, cm, mm, mi, ft, in, yd |
| Weight | kg, g, lb, oz |
| Temperature | c, f, k |
| Data | b, kb, mb, gb, tb |
| Time | ms, s, min, hr, day |

```
/exec unit_convert 100 km mi
→ 100 km = 62.14 mi

/exec unit_convert 72 f c
→ 72 f = 22.22 c

/exec unit_convert 1024 mb gb
→ 1024 mb = 1 gb

/exec unit_convert 3600 s hr
→ 3600 s = 1 hr
```

**Implementation (temperature special case):**
```c
// Convert to Celsius first, then to target
if (strcmp(from, "f") == 0) c = (val - 32) * 5.0 / 9.0;
else if (strcmp(from, "k") == 0) c = val - 273.15;
else c = val;
if (strcmp(to, "f") == 0) result = c * 9.0 / 5.0 + 32;
else if (strcmp(to, "k") == 0) result = c + 273.15;
else result = c;
```

---

### Tool 49: `password_gen`

**File:** `src/hands/impl/tool_password_gen.c`  
**Category:** Security / Utility  
**Args:** `[length] [-n no symbols]` (default: 20 chars)  
**Returns:** Cryptographically random password from `/dev/urandom`  

```
/exec password_gen
→ aB3$xK9!mP@qR2#wYz7
  (length: 20, entropy: ~131 bits)

/exec password_gen 32
→ aB3$xK9!mP@qR2#wYz7nL5&jH8*vQ4%
  (length: 32, entropy: ~210 bits)

/exec password_gen 16 -n
→ aB3xK9mPqR2wYz7n
  (length: 16, entropy: ~95 bits)
```

---

### Tool 50: `count_lines`

**File:** `src/hands/impl/tool_count_lines.c`  
**Category:** Development / Utility  
**Args:** `[directory] [extension]` (default: current dir, .c and .h)  
**Returns:** Lines of code per file and total  

```
/exec count_lines /root/seaclaw/src
→ Lines of code in /root/seaclaw/src:
     83 src/core/sea_log.c
    156 src/core/sea_config.c
    ...
   9159 total

/exec count_lines /root/seaclaw/src .c
→ (only .c files)

/exec count_lines /root/seaclaw/include .h
→ (only .h files)
```

---

## Task & Database Tools

### Tool 7: `task_manage`

**File:** `src/hands/impl/tool_task_manage.c`  
**Category:** Tasks  
**Args:** `list` | `create|<title>` | `done|<id>`  
**Returns:** Task list or confirmation  

```
/exec task_manage list
→ Tasks (3):
  ○ [medium] Implement weather tool
  ► [high]   Fix SSL check timeout
  ✓ [low]    Update README

/exec task_manage create|Write documentation
→ Task created: #4 "Write documentation"

/exec task_manage done|2
→ Task #2 marked as completed
```

---

### Tool 8: `db_query`

**File:** `src/hands/impl/tool_db_query.c`  
**Category:** Database  
**Args:** `<SELECT_sql>` (read-only queries only)  
**Returns:** Query results in tabular format  
**Security:** Only SELECT and PRAGMA allowed. INSERT/UPDATE/DELETE/DROP blocked.  

```
/exec db_query SELECT count(*) FROM tasks
→ count(*)
  --------
  4

/exec db_query SELECT title, status FROM tasks WHERE status = 'pending'
→ title                    | status
  -------------------------+--------
  Write documentation      | pending
  Add benchmarks           | pending

/exec db_query DROP TABLE tasks
→ Error: mutation queries not allowed (SELECT/PRAGMA only)
```

---

## Adding a New Tool

1. **Create** `src/hands/impl/tool_yourname.c`:
```c
/*
 * tool_yourname.c — Description
 *
 * Tool ID:    51
 * Category:   Your Category
 * Args:       <arg_description>
 * Returns:    What it returns
 *
 * Examples:
 *   /exec yourname arg1 arg2
 *
 * Security: How inputs are validated.
 */

#include "seaclaw/sea_tools.h"
#include <stdio.h>
#include <string.h>

SeaError tool_yourname(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    if (args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: <args>");
        return SEA_OK;
    }
    char buf[256];
    int len = snprintf(buf, sizeof(buf), "Result: %.*s", (int)args.len, (const char*)args.data);
    u8* dst = (u8*)sea_arena_push_bytes(arena, buf, (u64)len);
    if (!dst) return SEA_ERR_ARENA_FULL;
    output->data = dst;
    output->len  = (u32)len;
    return SEA_OK;
}
```

2. **Register** in `src/hands/sea_tools.c`:
```c
extern SeaError tool_yourname(SeaSlice args, SeaArena* arena, SeaSlice* output);
// In s_registry[]:
{51, "yourname", "Description. Args: <args>", tool_yourname},
```

3. **Add to Makefile** `HANDS_SRC`:
```makefile
src/hands/impl/tool_yourname.c \
```

4. **Build and test:**
```bash
make clean && make all && make test
```

---

*Generated from Sea-Claw v1.0.0 — 50 tools, 9,159 lines of C11*
