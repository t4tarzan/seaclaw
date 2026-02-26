/*
 * tool_voice.c — Voice Transcription Tool
 *
 * Transcribes audio files using the Groq Whisper API.
 * Falls back to local whisper.cpp if GROQ_API_KEY is not set.
 *
 * Tool ID: 77 — voice_transcribe
 *
 * Args format: <file_path> [language]
 *   file_path — path to audio file (.mp3, .wav, .ogg, .m4a, .webm)
 *   language  — optional ISO-639-1 code (e.g. "en", "es") — default: auto
 *
 * Env vars:
 *   GROQ_API_KEY     — Groq API key (required for cloud transcription)
 *   WHISPER_MODEL    — model override (default: whisper-large-v3-turbo)
 *
 * Output: plain transcribed text
 */

#include "seaclaw/sea_tools.h"
#include "seaclaw/sea_log.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>

/* ── Constants ───────────────────────────────────────────── */

#define GROQ_WHISPER_URL  "https://api.groq.com/openai/v1/audio/transcriptions"
#define DEFAULT_MODEL     "whisper-large-v3-turbo"
#define MAX_FILE_SIZE     (25 * 1024 * 1024) /* 25MB Groq limit */
#define OUT_BUF_SIZE      (16 * 1024)        /* 16KB output buffer */

/* ── Helpers ─────────────────────────────────────────────── */

static bool file_exists(const char* path) {
    struct stat st;
    return stat(path, &st) == 0 && S_ISREG(st.st_mode);
}

static long file_size(const char* path) {
    struct stat st;
    if (stat(path, &st) != 0) return -1;
    return (long)st.st_size;
}

/* Extract "text":"<value>" from Groq JSON response */
static int extract_text_field(const char* json, char* out, size_t out_max) {
    const char* key = "\"text\":\"";
    const char* p = strstr(json, key);
    if (!p) return 0;
    p += strlen(key);

    size_t i = 0;
    /* Handle escaped characters */
    while (*p && i < out_max - 1) {
        if (*p == '"') break;
        if (*p == '\\' && *(p+1)) {
            p++;
            if      (*p == 'n')  { out[i++] = '\n'; }
            else if (*p == 't')  { out[i++] = '\t'; }
            else if (*p == '"')  { out[i++] = '"';  }
            else if (*p == '\\') { out[i++] = '\\'; }
            else                 { out[i++] = *p;   }
        } else {
            out[i++] = *p;
        }
        p++;
    }
    out[i] = '\0';
    return (int)i;
}

/* ── Tool Implementation ─────────────────────────────────── */

SeaError tool_voice_transcribe(SeaSlice args, SeaArena* arena, SeaSlice* output) {
    (void)arena;

    /* Parse args: <file_path> [language] */
    char file_path[512] = {0};
    char language[8]    = {0};

    if (!args.data || args.len == 0) {
        *output = SEA_SLICE_LIT("Usage: voice_transcribe <file_path> [language]");
        return SEA_OK;
    }

    /* Copy args to null-terminated string */
    char arg_buf[600];
    u32 copy_len = args.len < sizeof(arg_buf) - 1 ? args.len : sizeof(arg_buf) - 1;
    memcpy(arg_buf, args.data, copy_len);
    arg_buf[copy_len] = '\0';

    /* Split on first space */
    char* sp = strchr(arg_buf, ' ');
    if (sp) {
        *sp = '\0';
        strncpy(language, sp + 1, sizeof(language) - 1);
        /* Trim trailing whitespace from language */
        size_t ll = strlen(language);
        while (ll > 0 && (language[ll-1] == ' ' || language[ll-1] == '\n')) {
            language[--ll] = '\0';
        }
    }
    strncpy(file_path, arg_buf, sizeof(file_path) - 1);

    /* Validate file */
    if (!file_exists(file_path)) {
        static char err_buf[600];
        snprintf(err_buf, sizeof(err_buf), "Error: file not found: %s", file_path);
        output->data = (const u8*)err_buf;
        output->len  = (u32)strlen(err_buf);
        return SEA_OK;
    }

    long fsize = file_size(file_path);
    if (fsize <= 0) {
        *output = SEA_SLICE_LIT("Error: file is empty or unreadable");
        return SEA_OK;
    }
    if (fsize > MAX_FILE_SIZE) {
        static char err_buf[128];
        snprintf(err_buf, sizeof(err_buf),
                 "Error: file too large (%ldMB, max 25MB)", fsize / 1024 / 1024);
        output->data = (const u8*)err_buf;
        output->len  = (u32)strlen(err_buf);
        return SEA_OK;
    }

    /* Check for Groq API key */
    const char* api_key = getenv("GROQ_API_KEY");
    const char* model   = getenv("WHISPER_MODEL");
    if (!model || !model[0]) model = DEFAULT_MODEL;

    static char out_buf[OUT_BUF_SIZE];

    if (api_key && api_key[0]) {
        /* ── Groq Cloud Transcription ──────────────────────── */
        char lang_flag[32] = {0};
        if (language[0]) {
            snprintf(lang_flag, sizeof(lang_flag), " -F 'language=%s'", language);
        }

        /* Build curl command for multipart form upload */
        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "curl -sf -m 60 "
            "-H 'Authorization: Bearer %s' "
            "-F 'file=@%s' "
            "-F 'model=%s' "
            "-F 'response_format=json'"
            "%s "
            "'%s' 2>&1",
            api_key, file_path, model, lang_flag, GROQ_WHISPER_URL);

        FILE* p = popen(cmd, "r");
        if (!p) {
            *output = SEA_SLICE_LIT("Error: failed to run curl for transcription");
            return SEA_OK;
        }

        char resp_buf[OUT_BUF_SIZE];
        size_t total = 0, n;
        while (total < sizeof(resp_buf) - 1 &&
               (n = fread(resp_buf + total, 1, sizeof(resp_buf) - 1 - total, p)) > 0) {
            total += n;
        }
        resp_buf[total] = '\0';
        pclose(p);

        if (total == 0) {
            *output = SEA_SLICE_LIT("Error: empty response from Groq API");
            return SEA_OK;
        }

        /* Check for API error */
        if (strstr(resp_buf, "\"error\"")) {
            snprintf(out_buf, sizeof(out_buf), "Groq API error: %.400s", resp_buf);
            output->data = (const u8*)out_buf;
            output->len  = (u32)strlen(out_buf);
            return SEA_OK;
        }

        int text_len = extract_text_field(resp_buf, out_buf, sizeof(out_buf));
        if (text_len <= 0) {
            snprintf(out_buf, sizeof(out_buf), "Error: could not parse response: %.300s", resp_buf);
            output->data = (const u8*)out_buf;
            output->len  = (u32)strlen(out_buf);
            return SEA_OK;
        }

        SEA_LOG_INFO("VOICE", "Transcribed %s via Groq Whisper (%d chars)", file_path, text_len);

    } else {
        /* ── Local whisper.cpp fallback ───────────────────── */
        /* Try whisper-cli (whisper.cpp) if installed */
        char which_buf[256];
        FILE* wh = popen("which whisper-cli 2>/dev/null || which whisper 2>/dev/null", "r");
        which_buf[0] = '\0';
        if (wh) {
            size_t wn = fread(which_buf, 1, sizeof(which_buf) - 1, wh);
            which_buf[wn] = '\0';
            pclose(wh);
            /* Trim newline */
            char* nl = strchr(which_buf, '\n');
            if (nl) *nl = '\0';
        }

        if (!which_buf[0]) {
            snprintf(out_buf, sizeof(out_buf),
                "No transcription backend available.\n"
                "Set GROQ_API_KEY for cloud transcription, "
                "or install whisper.cpp (whisper-cli) for local.");
            output->data = (const u8*)out_buf;
            output->len  = (u32)strlen(out_buf);
            return SEA_OK;
        }

        char lang_flag[16] = {0};
        if (language[0]) snprintf(lang_flag, sizeof(lang_flag), " -l %s", language);

        char cmd[1024];
        snprintf(cmd, sizeof(cmd),
            "%s -f '%s'%s --output-txt --no-timestamps 2>/dev/null",
            which_buf, file_path, lang_flag);

        FILE* p = popen(cmd, "r");
        if (!p) {
            *output = SEA_SLICE_LIT("Error: failed to run whisper-cli");
            return SEA_OK;
        }

        size_t total = 0, n;
        while (total < sizeof(out_buf) - 1 &&
               (n = fread(out_buf + total, 1, sizeof(out_buf) - 1 - total, p)) > 0) {
            total += n;
        }
        out_buf[total] = '\0';
        pclose(p);

        if (total == 0) {
            *output = SEA_SLICE_LIT("Error: whisper-cli produced no output");
            return SEA_OK;
        }

        SEA_LOG_INFO("VOICE", "Transcribed %s via local whisper (%zu chars)", file_path, total);
    }

    output->data = (const u8*)out_buf;
    output->len  = (u32)strlen(out_buf);
    return SEA_OK;
}
