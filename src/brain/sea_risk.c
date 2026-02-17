/*
 * sea_risk.c — Command Risk Classification
 *
 * Pattern-based risk classification for shell commands.
 */

#include "seaclaw/sea_risk.h"
#include "seaclaw/sea_log.h"
#include <string.h>
#include <ctype.h>

/* ── Risk patterns ────────────────────────────────────────── */

typedef struct {
    const char*  pattern;
    SeaRiskLevel level;
    const char*  reason;
} RiskPattern;

static const RiskPattern s_risk_patterns[] = {
    /* CRITICAL - System destruction */
    { "rm -rf /",           SEA_RISK_CRITICAL, "Recursive delete of root filesystem" },
    { "rm -rf /*",          SEA_RISK_CRITICAL, "Recursive delete of root filesystem" },
    { "mkfs",               SEA_RISK_CRITICAL, "Format filesystem" },
    { "dd if=/dev/zero",    SEA_RISK_CRITICAL, "Disk wipe operation" },
    { ":(){ :|:& };:",      SEA_RISK_CRITICAL, "Fork bomb" },
    { "chmod -R 777 /",     SEA_RISK_CRITICAL, "Recursive permission change on root" },
    
    /* HIGH - Dangerous operations */
    { "rm -rf",             SEA_RISK_HIGH, "Recursive delete" },
    { "rm -r",              SEA_RISK_HIGH, "Recursive delete" },
    { "dd ",                SEA_RISK_HIGH, "Direct disk operation" },
    { "fdisk",              SEA_RISK_HIGH, "Disk partitioning" },
    { "parted",             SEA_RISK_HIGH, "Disk partitioning" },
    { "shutdown",           SEA_RISK_HIGH, "System shutdown" },
    { "reboot",             SEA_RISK_HIGH, "System reboot" },
    { "halt",               SEA_RISK_HIGH, "System halt" },
    { "init 0",             SEA_RISK_HIGH, "System shutdown" },
    { "init 6",             SEA_RISK_HIGH, "System reboot" },
    { "passwd",             SEA_RISK_HIGH, "Password change" },
    { "useradd",            SEA_RISK_HIGH, "User creation" },
    { "userdel",            SEA_RISK_HIGH, "User deletion" },
    { "groupadd",           SEA_RISK_HIGH, "Group creation" },
    { "visudo",             SEA_RISK_HIGH, "Sudo configuration" },
    { "iptables",           SEA_RISK_HIGH, "Firewall modification" },
    { "ufw ",               SEA_RISK_HIGH, "Firewall modification" },
    { "systemctl stop",     SEA_RISK_HIGH, "Service stop" },
    { "systemctl disable",  SEA_RISK_HIGH, "Service disable" },
    { "kill -9",            SEA_RISK_HIGH, "Force kill process" },
    { "pkill",              SEA_RISK_HIGH, "Kill processes by name" },
    { "killall",            SEA_RISK_HIGH, "Kill all processes" },
    { "> /dev/sda",         SEA_RISK_HIGH, "Write to disk device" },
    { "> /dev/hda",         SEA_RISK_HIGH, "Write to disk device" },
    { "chmod 777",          SEA_RISK_HIGH, "Insecure permissions" },
    { "chown root",         SEA_RISK_HIGH, "Change ownership to root" },
    
    /* MEDIUM - Network/external operations */
    { "curl ",              SEA_RISK_MEDIUM, "HTTP request" },
    { "wget ",              SEA_RISK_MEDIUM, "HTTP download" },
    { "nc ",                SEA_RISK_MEDIUM, "Network connection" },
    { "netcat",             SEA_RISK_MEDIUM, "Network connection" },
    { "ssh ",               SEA_RISK_MEDIUM, "SSH connection" },
    { "scp ",               SEA_RISK_MEDIUM, "Secure copy" },
    { "rsync",              SEA_RISK_MEDIUM, "File synchronization" },
    { "git clone",          SEA_RISK_MEDIUM, "Git clone" },
    { "docker run",         SEA_RISK_MEDIUM, "Docker container" },
    { "docker exec",        SEA_RISK_MEDIUM, "Docker exec" },
    { "sudo ",              SEA_RISK_MEDIUM, "Elevated privileges" },
    { "su ",                SEA_RISK_MEDIUM, "Switch user" },
    { "apt install",        SEA_RISK_MEDIUM, "Package installation" },
    { "apt-get install",    SEA_RISK_MEDIUM, "Package installation" },
    { "yum install",        SEA_RISK_MEDIUM, "Package installation" },
    { "pip install",        SEA_RISK_MEDIUM, "Python package installation" },
    { "npm install",        SEA_RISK_MEDIUM, "NPM package installation" },
    { "gem install",        SEA_RISK_MEDIUM, "Ruby gem installation" },
    { "chmod +x",           SEA_RISK_MEDIUM, "Make file executable" },
    { "./",                 SEA_RISK_MEDIUM, "Execute local file" },
    { "bash ",              SEA_RISK_MEDIUM, "Execute bash script" },
    { "sh ",                SEA_RISK_MEDIUM, "Execute shell script" },
    { "python ",            SEA_RISK_MEDIUM, "Execute Python script" },
    { "perl ",              SEA_RISK_MEDIUM, "Execute Perl script" },
    { "ruby ",              SEA_RISK_MEDIUM, "Execute Ruby script" },
    { "eval ",              SEA_RISK_MEDIUM, "Dynamic code evaluation" },
    { "exec ",              SEA_RISK_MEDIUM, "Execute command" },
    { "source ",            SEA_RISK_MEDIUM, "Source script" },
    { ". ",                 SEA_RISK_MEDIUM, "Source script" },
    
    /* Terminator */
    { NULL, SEA_RISK_LOW, NULL }
};

/* ── Helper: normalize command ────────────────────────────── */

static void normalize_command(const char* cmd, char* out, size_t out_size) {
    size_t i = 0, j = 0;
    
    /* Skip leading whitespace */
    while (cmd[i] && isspace(cmd[i])) i++;
    
    /* Copy and normalize spaces */
    bool prev_space = false;
    while (cmd[i] && j < out_size - 1) {
        if (isspace(cmd[i])) {
            if (!prev_space) {
                out[j++] = ' ';
                prev_space = true;
            }
        } else {
            out[j++] = cmd[i];
            prev_space = false;
        }
        i++;
    }
    out[j] = '\0';
}

/* ── Classify command ─────────────────────────────────────── */

SeaRiskResult sea_risk_classify_command(const char* command) {
    SeaRiskResult result = {
        .level = SEA_RISK_LOW,
        .reason = "Safe command",
        .requires_approval = false
    };
    
    if (!command || !*command) {
        return result;
    }
    
    /* Normalize command */
    char normalized[2048];
    normalize_command(command, normalized, sizeof(normalized));
    
    /* Check against risk patterns */
    SeaRiskLevel highest_risk = SEA_RISK_LOW;
    const char* risk_reason = "Safe command";
    
    for (int i = 0; s_risk_patterns[i].pattern; i++) {
        if (strstr(normalized, s_risk_patterns[i].pattern)) {
            if (s_risk_patterns[i].level > highest_risk) {
                highest_risk = s_risk_patterns[i].level;
                risk_reason = s_risk_patterns[i].reason;
            }
        }
    }
    
    result.level = highest_risk;
    result.reason = risk_reason;
    
    /* Require approval for MEDIUM and above */
    result.requires_approval = (highest_risk >= SEA_RISK_MEDIUM);
    
    if (highest_risk > SEA_RISK_LOW) {
        SEA_LOG_INFO("RISK", "Command classified as %s: %s", 
                     sea_risk_level_name(highest_risk), normalized);
    }
    
    return result;
}

/* ── Get risk level name ──────────────────────────────────── */

const char* sea_risk_level_name(SeaRiskLevel level) {
    switch (level) {
        case SEA_RISK_LOW:      return "LOW";
        case SEA_RISK_MEDIUM:   return "MEDIUM";
        case SEA_RISK_HIGH:     return "HIGH";
        case SEA_RISK_CRITICAL: return "CRITICAL";
        default:                return "UNKNOWN";
    }
}
