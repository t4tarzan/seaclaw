# Nanobot - Complete Features Report

## Overview
**nanobot** is an ultra-lightweight personal AI assistant written in Python. It delivers core agent functionality in just ~4,000 lines of code (3,668 lines verified) — 99% smaller than Clawdbot's 430k+ lines.

**Version:** v0.1.3.post7 (latest)  
**Language:** Python ≥3.11  
**Lines of Code:** ~3,668 (core agent)  
**License:** MIT  
**Repository:** https://github.com/HKUDS/nanobot  
**PyPI:** https://pypi.org/project/nanobot-ai/

---

## Core Architecture Features

### 1. **Ultra-Lightweight Design**
- **~4,000 lines** of core agent code
- **99% smaller** than Clawdbot (430k+ lines)
- **Fast startup** - Minimal footprint
- **Low resource usage** - Efficient memory management
- **Research-ready** - Clean, readable code
- **Easy to extend** - Simple architecture

### 2. **Agent Loop**
- **Core processing engine** - Main event loop
- **Message handling:**
  - Receives messages from bus
  - Builds context with history, memory, skills
  - Calls LLM with tool definitions
  - Executes tool calls
  - Sends responses back
- **Iteration control:**
  - Max iterations: 20 (configurable)
  - Tool execution loop
  - Reflection prompts after tool calls
- **Session management:**
  - Per-channel isolation
  - Per-chat isolation
  - History tracking (50 messages default)
  - Memory consolidation

### 3. **Context Builder**
- **System prompt assembly:**
  - Core identity
  - Bootstrap files (AGENTS.md, SOUL.md, USER.md, TOOLS.md, IDENTITY.md)
  - Memory context
  - Skills (always-loaded + available)
  - Current time and timezone
  - Runtime information
  - Workspace path
- **Progressive skill loading:**
  - Always-loaded skills (full content)
  - Available skills (summary only)
  - On-demand loading via read_file tool
- **Dynamic context** - Adapts to conversation needs

---

## LLM Provider Support

### 4. **Multi-Provider Architecture**
- **Provider Registry** - Single source of truth
- **Supported providers (15):**
  - `custom` - Any OpenAI-compatible endpoint
  - `openrouter` - Multi-model gateway (recommended)
  - `anthropic` - Claude direct
  - `openai` - GPT direct
  - `deepseek` - DeepSeek direct
  - `groq` - LLM + voice transcription (Whisper)
  - `gemini` - Gemini direct
  - `minimax` - MiniMax direct
  - `aihubmix` - API gateway
  - `dashscope` - Qwen (Alibaba)
  - `moonshot` - Moonshot/Kimi
  - `zhipu` - Zhipu GLM
  - `vllm` - Local/self-hosted
  - `openai_codex` - OpenAI Codex (OAuth)
  - Additional custom providers
- **Auto-detection:**
  - Model name keyword matching
  - API key prefix detection
  - API base URL detection
- **Fallback support** - Automatic provider switching
- **LiteLLM integration** - Unified API interface

### 5. **Provider Features**
- **API key management:**
  - Config file storage
  - Environment variables
  - Per-provider configuration
- **Custom headers** - Extra headers support (e.g., AiHubMix APP-Code)
- **API base override** - Custom endpoints
- **Model prefixing** - Automatic model name formatting
- **Gateway detection:**
  - OpenRouter (sk-or- prefix)
  - AiHubMix (URL detection)
  - Custom gateways
- **OAuth support** - OpenAI Codex authentication
- **Model overrides** - Per-model parameter tuning

### 6. **LLM Configuration**
- **Temperature** - 0.7 default (configurable)
- **Max tokens** - 4096 default (configurable)
- **Model selection** - Per-provider defaults
- **Reasoning support** - Kimi K2.5, DeepSeek-R1
- **Tool calling** - Function calling support
- **Streaming** - Real-time responses

---

## Built-in Tools

### 7. **File System Tools**
- **read_file** - Read file contents
- **write_file** - Write/create files
- **edit_file** - In-place file editing
- **list_dir** - Directory listing
- **Workspace restriction** - Optional sandboxing
- **Path validation** - Security checks

### 8. **Shell Execution**
- **exec** - Run shell commands
- **Working directory** - Workspace-based
- **Timeout control** - 60s default (configurable)
- **Output capture** - stdout/stderr
- **Error handling** - Exit code tracking
- **Workspace restriction** - Optional sandboxing

### 9. **Web Tools**
- **web_search** - Brave Search API integration
  - Configurable max results (5 default)
  - API key support
  - Query formatting
- **web_fetch** - HTTP GET requests
  - URL content retrieval
  - Response parsing

### 10. **Communication Tools**
- **message** - Send messages to chat channels
  - Channel routing
  - Chat ID targeting
  - Context-aware (knows current channel)
- **spawn** - Spawn subagents for background tasks
  - Isolated execution
  - Result reporting
  - System message routing

### 11. **Scheduling Tool**
- **cron** - Task scheduling
  - Create scheduled jobs
  - List active jobs
  - Remove jobs
  - Natural language scheduling
  - Integration with cron service

---

## MCP (Model Context Protocol) Support

### 12. **MCP Integration**
- **External tool servers** - Connect to MCP servers
- **Transport modes:**
  - **Stdio** - Local process (npx, uvx)
  - **HTTP** - Remote endpoints (SSE)
- **Auto-discovery** - Tools registered on startup
- **Native integration** - LLM can use MCP tools alongside built-in tools
- **Config compatibility** - Claude Desktop / Cursor format
- **Server management:**
  - Connection lifecycle
  - Error handling
  - Graceful shutdown

### 13. **MCP Configuration**
```json
{
  "tools": {
    "mcpServers": {
      "server_name": {
        "command": "npx",
        "args": ["-y", "@modelcontextprotocol/server-filesystem", "/path"],
        "env": {"KEY": "value"}
      }
    }
  }
}
```

---

## Chat Channel Integrations

### 14. **Telegram Channel**
- **Long-polling** - No webhook required
- **Features:**
  - Text messages
  - Voice messages (auto-transcription via Groq Whisper)
  - User allowlist
  - Bot commands
- **Configuration:**
  - Bot token
  - Allow from (user IDs)
- **Groq integration** - Free voice transcription

### 15. **Discord Channel**
- **Bot integration** - Discord.py
- **Features:**
  - Message content intent
  - Server members intent
  - User allowlist
  - Direct messages
  - Server messages
- **Configuration:**
  - Bot token
  - Allow from (user IDs)

### 16. **WhatsApp Channel**
- **QR code authentication** - Device linking
- **Features:**
  - Text messages
  - User allowlist (phone numbers)
  - Session persistence
- **Requirements:** Node.js ≥18
- **Configuration:**
  - Allow from (phone numbers)

### 17. **Feishu Channel (飞书)**
- **WebSocket long connection** - No public IP required
- **Features:**
  - Text messages
  - User allowlist (ou_xxx)
  - Event subscriptions
- **Configuration:**
  - App ID
  - App Secret
  - Encrypt key (optional)
  - Verification token (optional)

### 18. **Mochat Channel (Claw IM)**
- **Socket.IO WebSocket** - Default transport
- **HTTP polling fallback** - Automatic
- **Features:**
  - Session filtering
  - Panel filtering
  - Reply delay modes (non-mention, always)
  - Agent ownership binding
- **Configuration:**
  - Base URL
  - Socket URL
  - Claw token
  - Agent user ID
  - Sessions/panels filters

### 19. **DingTalk Channel (钉钉)**
- **Stream Mode** - No public IP required
- **Features:**
  - Text messages
  - User allowlist (staff IDs)
  - Robot capability
- **Configuration:**
  - Client ID (App Key)
  - Client Secret (App Secret)

### 20. **Slack Channel**
- **Socket Mode** - No public URL required
- **Features:**
  - Direct messages
  - Channel messages
  - @mentions
  - Thread support
  - Reactions
- **Group policies:**
  - `mention` - Respond only when @mentioned (default)
  - `open` - Respond to all channel messages
  - `allowlist` - Restrict to specific channels
- **Configuration:**
  - Bot token (xoxb-)
  - App token (xapp-)
  - Group policy
  - DM settings

### 21. **Email Channel**
- **IMAP polling** - Incoming mail
- **SMTP sending** - Outgoing replies
- **Features:**
  - Email reading
  - Auto-reply (optional)
  - Sender allowlist
  - Consent gate (safety)
- **Configuration:**
  - IMAP host/port/credentials
  - SMTP host/port/credentials
  - From address
  - Allow from (email addresses)
  - Consent granted flag

### 22. **QQ Channel (QQ单聊)**
- **botpy SDK** - WebSocket
- **Features:**
  - Private messages only (current)
  - User allowlist (openids)
  - Sandbox testing
- **Configuration:**
  - App ID
  - Secret
  - Allow from (openids)

### 23. **Channel Manager**
- **Multi-channel coordination** - Simultaneous channels
- **Message routing** - Inbound/outbound dispatch
- **Channel lifecycle:**
  - Initialization
  - Start/stop
  - Error handling
- **Status monitoring** - Channel health checks

---

## Memory & Session Management

### 24. **Session Manager**
- **Per-channel sessions** - Isolated contexts
- **Per-chat sessions** - Individual conversations
- **Session storage:**
  - JSON files in workspace
  - Automatic persistence
  - Load on demand
- **History management:**
  - Message history (user/assistant)
  - Tool usage tracking
  - Timestamp tracking
- **Memory window** - 50 messages default (configurable)

### 25. **Memory Store**
- **Long-term memory** - Persistent across sessions
- **Memory files:**
  - `memory/MEMORY.md` - Important facts
  - `memory/HISTORY.md` - Event log (grep-searchable)
- **Context injection** - Relevant memory in prompts
- **Consolidation:**
  - Automatic when history exceeds window
  - Archive old messages
  - Background processing
  - Async operation

### 26. **Memory Consolidation**
- **Trigger:** When session exceeds memory window
- **Process:**
  - Extract important information
  - Append to MEMORY.md
  - Archive to HISTORY.md
  - Clear old messages
- **Background task** - Non-blocking
- **Session cleanup** - `/new` command support

---

## Skills System

### 27. **Skills Loader**
- **Skill discovery:**
  - Workspace skills (`~/workspace/skills/`)
  - Bundled skills (package skills)
- **Skill structure:**
  - `SKILL.md` - Skill definition
  - YAML frontmatter (name, description, version, author, dependencies)
  - Markdown body (instructions)
- **Loading modes:**
  - Always-loaded skills (full content in prompt)
  - Available skills (summary only, load on demand)
- **Dependency checking:**
  - Command availability
  - Package availability
  - Availability status in summary

### 28. **Bundled Skills**
- **GitHub skill** - GitHub API integration
- **Weather skill** - Weather data
- **tmux skill** - Terminal multiplexer control
- **Additional skills** - Extensible system

### 29. **Custom Skills**
- **User-defined skills** - `~/workspace/skills/`
- **Progressive loading** - Read on demand
- **Dependency management** - Automatic checking
- **Version tracking** - Skill versioning

---

## Scheduling & Automation

### 30. **Cron Service**
- **Job storage** - JSON file persistence
- **Schedule types:**
  - Cron expressions (e.g., `0 9 * * *`)
  - Interval (seconds)
  - One-time execution
- **Job management:**
  - Create, list, remove
  - Status tracking
  - Last run timestamp
  - Next run calculation
- **Execution:**
  - Background processing
  - Agent integration
  - Message delivery (optional)
  - Result capture

### 31. **Cron CLI**
- **Commands:**
  - `nanobot cron add` - Create job
  - `nanobot cron list` - List jobs
  - `nanobot cron remove` - Delete job
- **Options:**
  - `--name` - Job name
  - `--message` - Task message
  - `--cron` - Cron expression
  - `--every` - Interval in seconds
  - `--channel` - Target channel
  - `--to` - Target chat ID
  - `--deliver` - Auto-deliver results

### 32. **Heartbeat Service**
- **Proactive wake-up** - Periodic self-prompting
- **Interval:** 30 minutes default
- **Purpose:**
  - Proactive task checking
  - Memory consolidation
  - Status updates
- **Agent integration** - Direct processing
- **Background operation** - Non-blocking

---

## Message Bus & Routing

### 33. **Message Bus**
- **Queue system:**
  - Inbound queue (from channels)
  - Outbound queue (to channels)
- **Async operations** - Non-blocking
- **Message types:**
  - InboundMessage (channel, sender_id, chat_id, content, media, metadata)
  - OutboundMessage (channel, chat_id, content, metadata)
- **Timeout handling** - Configurable waits
- **Thread-safe** - Concurrent access

### 34. **Event System**
- **InboundMessage:**
  - Channel identification
  - Sender tracking
  - Chat ID routing
  - Content payload
  - Media attachments
  - Metadata (thread_ts, etc.)
- **OutboundMessage:**
  - Channel targeting
  - Chat ID routing
  - Content payload
  - Metadata passthrough

---

## Subagent System

### 35. **Subagent Manager**
- **Background task execution** - Long-running operations
- **Isolated contexts** - Separate from main agent
- **Features:**
  - Spawn subagents
  - Task delegation
  - Result reporting
  - System message routing
- **Integration:**
  - Spawn tool
  - Message bus
  - Session management

### 36. **Subagent Execution**
- **Process:**
  1. Main agent spawns subagent
  2. Subagent processes task independently
  3. Result sent as system message
  4. Main agent receives and processes
- **Use cases:**
  - Complex multi-step tasks
  - Time-consuming operations
  - Parallel processing

---

## Configuration System

### 37. **Config Schema**
- **Pydantic-based** - Type validation
- **Nested structure:**
  - `agents` - Agent defaults
  - `channels` - Channel configs
  - `providers` - LLM providers
  - `gateway` - Server settings
  - `tools` - Tool configs
- **Environment variables:**
  - Prefix: `NANOBOT_`
  - Nested delimiter: `__`
  - Override config values

### 38. **Agent Configuration**
- **Defaults:**
  - Model (e.g., `anthropic/claude-opus-4-5`)
  - Temperature (0.7)
  - Max tokens (4096)
  - Max tool iterations (20)
  - Memory window (50)
  - Workspace path (`~/workspace`)

### 39. **Tools Configuration**
- **Web tools:**
  - Brave Search API key
  - Max results (5)
- **Exec tool:**
  - Timeout (60s)
- **Workspace restriction:**
  - `restrictToWorkspace` (false default)
  - Sandbox all tools to workspace
- **MCP servers:**
  - Server definitions
  - Command/args or URL
  - Environment variables

### 40. **Gateway Configuration**
- **Host:** 0.0.0.0 (default)
- **Port:** 18790 (default)
- **Multi-channel support** - All enabled channels

---

## CLI Commands

### 41. **Setup Commands**
- **`nanobot onboard`** - Initialize config & workspace
  - Creates `~/.nanobot/config.json`
  - Creates workspace directory
  - Creates bootstrap files (AGENTS.md, SOUL.md, USER.md)
  - Creates memory directory
  - Creates skills directory
  - Refresh mode (preserve existing values)

### 42. **Agent Commands**
- **`nanobot agent`** - Interactive chat mode
- **`nanobot agent -m "..."`** - Single message
- **`nanobot agent --no-markdown`** - Plain text output
- **`nanobot agent --logs`** - Show runtime logs
- **Exit commands:** `exit`, `quit`, `/exit`, `/quit`, `:q`, `Ctrl+D`

### 43. **Gateway Command**
- **`nanobot gateway`** - Start gateway server
  - Starts all enabled channels
  - Starts agent loop
  - Starts cron service
  - Starts heartbeat service
  - Multi-channel coordination
- **Options:**
  - `--port` - Gateway port (18790)
  - `--verbose` - Verbose output

### 44. **Status Commands**
- **`nanobot status`** - Show system status
  - Config validation
  - Provider status
  - Channel status
  - Workspace info
  - Version info

### 45. **Provider Commands**
- **`nanobot provider login openai-codex`** - OAuth login
  - Interactive authentication
  - Token storage
  - Requires ChatGPT Plus/Pro

### 46. **Channel Commands**
- **`nanobot channels login`** - WhatsApp QR code
- **`nanobot channels status`** - Channel health

### 47. **Cron Commands**
- **`nanobot cron add`** - Create scheduled job
- **`nanobot cron list`** - List jobs
- **`nanobot cron remove <id>`** - Delete job

---

## Docker Support

### 48. **Docker Deployment**
- **Dockerfile included** - Official image
- **Volume mounting:**
  - `-v ~/.nanobot:/root/.nanobot`
  - Config persistence
  - Workspace persistence
- **Port mapping:**
  - `-p 18790:18790` (gateway)
- **Commands:**
  - `docker build -t nanobot .`
  - `docker run ... nanobot onboard`
  - `docker run ... nanobot gateway`
  - `docker run ... nanobot agent -m "..."`

---

## Security Features

### 49. **Workspace Sandboxing**
- **`restrictToWorkspace` flag** - Restrict all tools
- **Affected tools:**
  - File read/write/edit
  - Directory listing
  - Shell execution
- **Path validation** - Prevent traversal
- **Production recommended** - Enable for deployments

### 50. **Channel Access Control**
- **`allowFrom` lists** - User whitelists
- **Per-channel configuration** - Independent access control
- **Empty = allow all** - Default open
- **Non-empty = whitelist only** - Restricted access
- **User ID formats:**
  - Telegram: user IDs
  - Discord: user IDs
  - WhatsApp: phone numbers
  - Email: email addresses
  - Feishu: ou_xxx
  - QQ: openids
  - Slack: user IDs

### 51. **Credential Management**
- **Config file storage** - `~/.nanobot/config.json`
- **Environment variables** - Override support
- **API key protection** - Not logged
- **OAuth support** - Secure authentication
- **Token storage** - Encrypted where applicable

---

## Agent Social Network

### 52. **Platform Integration**
- **Moltbook** - Agent community
  - Auto-join via skill.md
  - One-message setup
- **ClawdChat** - Agent chat network
  - Auto-join via skill.md
  - One-message setup
- **Skill-based onboarding:**
  - Read skill instructions
  - Follow setup steps
  - Automatic configuration

---

## Installation Methods

### 53. **PyPI Installation**
```bash
pip install nanobot-ai
```

### 54. **uv Installation**
```bash
uv tool install nanobot-ai
```

### 55. **Source Installation**
```bash
git clone https://github.com/HKUDS/nanobot.git
cd nanobot
pip install -e .
```

---

## Project Structure

### 56. **Directory Layout**
```
nanobot/
├── agent/          # Core agent logic
│   ├── loop.py     # Agent loop
│   ├── context.py  # Prompt builder
│   ├── memory.py   # Persistent memory
│   ├── skills.py   # Skills loader
│   ├── subagent.py # Background tasks
│   └── tools/      # Built-in tools
├── skills/         # Bundled skills
├── channels/       # Chat integrations
├── bus/            # Message routing
├── cron/           # Scheduled tasks
├── heartbeat/      # Proactive wake-up
├── providers/      # LLM providers
├── session/        # Conversation sessions
├── config/         # Configuration
└── cli/            # Commands
```

---

## Performance & Optimization

### 57. **Lightweight Design**
- **~3,668 lines** - Core agent code
- **Fast startup** - Minimal initialization
- **Low memory** - Efficient resource usage
- **Quick iterations** - Rapid development

### 58. **Async Architecture**
- **asyncio-based** - Non-blocking operations
- **Concurrent channels** - Parallel processing
- **Background tasks** - Async execution
- **Queue-based routing** - Efficient message handling

---

## Development Features

### 59. **Research-Ready**
- **Clean code** - Easy to understand
- **Modular design** - Easy to modify
- **Extensible** - Easy to extend
- **Well-documented** - Clear structure

### 60. **Provider Registry**
- **Single source of truth** - `providers/registry.py`
- **Easy to add providers:**
  1. Add ProviderSpec to registry
  2. Add field to ProvidersConfig
  - Automatic env vars, prefixing, matching, status display

### 61. **Tool Registry**
- **Centralized registration** - `agent/tools/registry.py`
- **Auto-discovery** - Tools registered at startup
- **Definition generation** - JSON schema for LLM
- **Execution routing** - Name-based dispatch

---

## Community & Support

### 62. **Communication Channels**
- **Discord** - https://discord.gg/MnCvHqpUGB
- **Feishu Group** - See COMMUNICATION.md
- **WeChat Group** - See COMMUNICATION.md
- **GitHub Discussions** - Feature requests, Q&A
- **GitHub Issues** - Bug reports

### 63. **Documentation**
- **README.md** - Comprehensive guide
- **GitHub Wiki** - Additional docs
- **Code comments** - Inline documentation
- **Examples** - Usage examples

---

## Recent Updates (v0.1.3.post7)

### 64. **Latest Features**
- **MCP support** - Model Context Protocol integration
- **Security hardening** - Enhanced security measures
- **Memory redesign** - Improved reliability
- **MiniMax support** - New provider
- **Enhanced CLI** - Better user experience
- **Multi-channel** - Slack, Email, QQ support
- **Provider refactor** - Simplified provider addition
- **Qwen support** - Dashscope integration
- **Discord integration** - Discord channel
- **Moonshot/Kimi** - New provider
- **Feishu channel** - 飞书 integration
- **DeepSeek provider** - DeepSeek support
- **Docker support** - Container deployment
- **vLLM integration** - Local LLM support

---

## Roadmap

### 65. **Planned Features**
- **Multi-modal** - Images, voice, video
- **Long-term memory** - Enhanced memory system
- **Better reasoning** - Multi-step planning
- **More integrations** - Calendar and more
- **Self-improvement** - Learning from feedback

---

## Statistics Summary

- **Lines of Code:** ~3,668 (core agent)
- **Code Reduction:** 99% smaller than Clawdbot
- **Python Version:** ≥3.11
- **LLM Providers:** 15 supported
- **Chat Channels:** 9 integrations
- **Built-in Tools:** 11 core tools
- **MCP Support:** Yes (stdio + HTTP)
- **Docker Support:** Yes
- **License:** MIT
- **Active Development:** Yes

---

## Key Differentiators

### 66. **Compared to Other Agents**
- **Ultra-lightweight** - 99% smaller codebase
- **Research-ready** - Clean, readable code
- **Lightning fast** - Minimal footprint
- **Easy to use** - One-click deploy
- **Highly extensible** - Simple architecture
- **Multi-channel** - 9 chat platforms
- **MCP support** - External tool integration
- **Provider flexibility** - 15 LLM providers

---

## Resources

- **Repository:** https://github.com/HKUDS/nanobot
- **PyPI:** https://pypi.org/project/nanobot-ai/
- **Discord:** https://discord.gg/MnCvHqpUGB
- **License:** MIT
- **Contributors:** https://github.com/HKUDS/nanobot/graphs/contributors

---

*Report generated on 2026-02-17*
