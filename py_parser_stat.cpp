#include "config.h"
#include "py_parser.h"
#include "py_log.h"

// Global STAT table (definition in config.cpp)
StatTable statTable;

// Work buffer for all names/values
static char statWork[STAT_WORKBUF_SIZE];
static int  statWP = 0;

// ---------------------------------------------------------
// Copy into work buffer (RAM‑friendly, char* based)
// ---------------------------------------------------------
static const char* copyToWork(const char* src, int len) {
    if (len <= 0) return nullptr;
    if (statWP + len + 1 >= STAT_WORKBUF_SIZE)
        return nullptr;

    memcpy(&statWork[statWP], src, len);
    statWork[statWP + len] = '\0';
    const char* ptr = &statWork[statWP];
    statWP += len + 1;
    return ptr;
}

// ---------------------------------------------------------
// STAT parser (RAM‑optimized, no String/vector, no StatData)
// ---------------------------------------------------------
ParseResult parseStatFrame(int moduleIndex,
                           const String& raw)
{
    if (moduleIndex <= 0 || moduleIndex > 32) {
        Log(LOG_WARN, "STAT parser: invalid module index " + String(moduleIndex));
        return PARSE_IGNORED;
    }

    statTable.count = 0;
    statWP          = 0;

    Log(LOG_INFO, "STAT parser: raw length=" + String(raw.length()));

    int start = raw.indexOf('@');
    int end   = raw.indexOf("$$");

    if (start < 0 || end < 0 || end <= start) {
        Log(LOG_WARN, "STAT parser: no valid @ ... $$ frame found");
        return PARSE_FAIL;
    }

    const char* buf = raw.c_str();
    int pos = start + 1;
    int len = end;

    int lineCount = 0;

    // Iterate over lines
    while (pos < len) {
        int ls = pos;
        int le = ls;

        // Find end of line
        while (le < len) {
            char c = buf[le];
            if (c == '\r' || c == '\n')
                break;
            le++;
        }

        // Trim left/right
        int s = ls;
        int e = le;
        while (s < e && buf[s] == ' ') s++;
        while (e > s && buf[e - 1] == ' ') e--;

        if (e > s) {
            lineCount++;

            // "Command completed" → stop
            if ((e - s) >= 18 && strncmp(buf + s, "Command completed", 17) == 0)
                break;

            // Extract key/value
            int colon = -1;
            for (int i = s; i < e; i++) {
                if (buf[i] == ':') {
                    colon = i;
                    break;
                }
            }

            int ks, ke, vs, ve;

            if (colon >= 0) {
                ks = s;
                ke = colon;
                vs = colon + 1;
                ve = e;
            } else {
                // No ':' → use last space as separator
                int split = -1;
                for (int i = e - 1; i >= s; i--) {
                    if (buf[i] == ' ') {
                        split = i;
                        break;
                    }
                }
                if (split <= s) {
                    pos = le + 1;
                    continue;
                }
                ks = s;
                ke = split;
                vs = split + 1;
                ve = e;
            }

            // Trim key
            while (ks < ke && buf[ks] == ' ') ks++;
            while (ke > ks && buf[ke - 1] == ' ') ke--;

            // Trim value
            while (vs < ve && buf[vs] == ' ') vs++;
            while (ve > vs && buf[ve - 1] == ' ') ve--;

            int klen = ke - ks;
            int vlen = ve - vs;

            if (klen <= 0 || vlen <= 0) {
                pos = le + 1;
                continue;
            }

            // Only numeric values (as before), no 0‑filter
            char c0 = buf[vs];
            if (!(isdigit((unsigned char)c0) || c0 == '-' || c0 == '+')) {
                pos = le + 1;
                continue;
            }

            if (statTable.count >= STAT_MAX_FIELDS) {
                Log(LOG_WARN, "STAT parser: too many fields");
                break;
            }

            const char* kptr = copyToWork(buf + ks, klen);
            const char* vptr = copyToWork(buf + vs, vlen);
            if (!kptr || !vptr) {
                Log(LOG_WARN, "STAT parser: work buffer full");
                break;
            }

            int idx = statTable.count++;
            statTable.name[idx]  = kptr;
            statTable.value[idx] = vptr;
        }

        pos = le + 1;
    }

    Log(LOG_INFO, "STAT parser: parsed " + String(statTable.count) +
                  " fields for module " + String(moduleIndex));

    if (statTable.count == 0)
        return PARSE_FAIL;

    return PARSE_OK;
}
