// Navigation structure — defines sidebar groups and items
var NAV_DATA = [
  { group: "intro", title: "Introduction", items: [
    { id: "what-is-seaclaw", label: "What is Sea-Claw?" },
    { id: "why-seaclaw", label: "Why Sea-Claw?" },
    { id: "who-is-it-for", label: "Who Is It For?" },
    { id: "quick-stats", label: "Quick Stats" },
    { id: "how-it-compares", label: "How It Compares" }
  ]},
  { group: "start", title: "Getting Started", items: [
    { id: "prerequisites", label: "Prerequisites" },
    { id: "installation", label: "Installation" },
    { id: "first-run", label: "First Run" },
    { id: "configuration", label: "Configuration" },
    { id: "env-variables", label: "Environment Variables" },
    { id: "docker-setup", label: "Docker Setup" },
    { id: "raspberry-pi", label: "Raspberry Pi Setup" }
  ]},
  { group: "arch", title: "Architecture", items: [
    { id: "five-pillars", label: "The Five Pillars" },
    { id: "directory-structure", label: "Directory Structure" },
    { id: "dependency-order", label: "Dependency Order" },
    { id: "memory-model", label: "Memory Model" },
    { id: "security-model", label: "Security Model" },
    { id: "build-system", label: "Build System" }
  ]},
  { group: "core", title: "Core Modules", items: [
    { id: "mod-types", label: "sea_types \u2014 Foundation Types" },
    { id: "mod-arena", label: "sea_arena \u2014 Memory Allocator" },
    { id: "mod-log", label: "sea_log \u2014 Structured Logging" },
    { id: "mod-json", label: "sea_json \u2014 JSON Parser" },
    { id: "mod-shield", label: "sea_shield \u2014 Grammar Filter" },
    { id: "mod-http", label: "sea_http \u2014 HTTP Client" },
    { id: "mod-db", label: "sea_db \u2014 Database" },
    { id: "mod-config", label: "sea_config \u2014 Configuration" }
  ]},
  { group: "intel", title: "Intelligence Layer", items: [
    { id: "mod-agent", label: "sea_agent \u2014 LLM Brain" },
    { id: "mod-tools", label: "sea_tools \u2014 Tool Registry" },
    { id: "tool-catalog", label: "Tool Catalog (63 Tools)" },
    { id: "mod-memory", label: "sea_memory \u2014 Workspace Memory" },
    { id: "mod-recall", label: "sea_recall \u2014 Fact Index" },
    { id: "mod-skill", label: "sea_skill \u2014 Learned Skills" },
    { id: "mod-graph", label: "sea_graph \u2014 Knowledge Graph" },
    { id: "sse-streaming", label: "SSE Streaming" }
  ]},
  { group: "channels", title: "Channels & Interfaces", items: [
    { id: "mod-telegram", label: "Telegram Bot" },
    { id: "mod-channel", label: "Channel Abstraction" },
    { id: "mod-slack", label: "Slack Webhook" },
    { id: "mod-ws", label: "WebSocket Server" },
    { id: "mod-api", label: "HTTP API Server" },
    { id: "mod-bus", label: "Message Bus" },
    { id: "tui-commands", label: "TUI Commands (30+)" }
  ]},
  { group: "seazero", title: "SeaZero Multi-Agent", items: [
    { id: "seazero-overview", label: "What is SeaZero?" },
    { id: "seazero-bridge", label: "Bridge API" },
    { id: "seazero-proxy", label: "LLM Proxy" },
    { id: "seazero-workspace", label: "Workspace Isolation" },
    { id: "seazero-docker", label: "Docker Orchestration" }
  ]},
  { group: "infra", title: "Infrastructure", items: [
    { id: "mod-auth", label: "Authentication & Tokens" },
    { id: "mod-cron", label: "Cron Scheduler" },
    { id: "mod-heartbeat", label: "Heartbeat Monitor" },
    { id: "mod-mesh", label: "Mesh Networking" },
    { id: "mod-session", label: "Session Management" },
    { id: "mod-usage", label: "Usage Tracking" },
    { id: "mod-pii", label: "PII Detection" },
    { id: "mod-ext", label: "Extension System" },
    { id: "mod-a2a", label: "Agent-to-Agent (A2A)" }
  ]},
  { group: "usecases", title: "Use Cases", items: [
    { id: "uc-personal", label: "Personal AI Assistant" },
    { id: "uc-devops", label: "DevOps Automation" },
    { id: "uc-iot", label: "IoT & Raspberry Pi" },
    { id: "uc-enterprise", label: "Enterprise Deployment" },
    { id: "uc-education", label: "Education & Research" },
    { id: "uc-multi-agent", label: "Multi-Agent Workflows" }
  ]},
  { group: "edge", title: "Edge Cases & FAQ", items: [
    { id: "edge-memory", label: "Memory Limits" },
    { id: "edge-security", label: "Security Edge Cases" },
    { id: "edge-network", label: "Network Failures" },
    { id: "edge-concurrency", label: "Concurrency" },
    { id: "faq", label: "Frequently Asked Questions" }
  ]},
  { group: "dev", title: "Developer Guide", items: [
    { id: "dev-add-tool", label: "Adding a New Tool" },
    { id: "dev-add-channel", label: "Adding a New Channel" },
    { id: "dev-add-provider", label: "Adding an LLM Provider" },
    { id: "dev-testing", label: "Writing Tests" },
    { id: "dev-debugging", label: "Debugging Tips" },
    { id: "dev-contributing", label: "Contributing" }
  ]},
  { group: "appendix", title: "Appendix", items: [
    { id: "error-codes", label: "Error Codes Reference" },
    { id: "grammar-types", label: "Grammar Types Reference" },
    { id: "db-schema", label: "Database Schema" },
    { id: "api-reference", label: "API Endpoint Reference" },
    { id: "changelog", label: "Changelog" },
    { id: "license", label: "License" }
  ]}
];

// Build sidebar HTML
(function buildNav() {
  var nav = document.getElementById('sidebar-nav');
  var html = '';
  NAV_DATA.forEach(function(g) {
    html += '<div class="nav-group" data-group="' + g.group + '">';
    html += '<div class="nav-group-title" onclick="toggleGroup(this)">' + g.title + ' <span class="arrow">&#9660;</span></div>';
    html += '<ul class="nav-items">';
    g.items.forEach(function(item) {
      html += '<li class="nav-item"><a href="#' + item.id + '">' + item.label + '</a></li>';
    });
    html += '</ul></div>';
  });
  nav.innerHTML = html;
})();
