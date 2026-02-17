/*
 * sea_risk.h — Command Risk Classification
 *
 * Classify shell commands by risk level and require approval for risky commands.
 */

#ifndef SEA_RISK_H
#define SEA_RISK_H

#include "sea_types.h"

/* Risk levels */
typedef enum {
    SEA_RISK_LOW = 0,      /* Safe commands: ls, echo, cat */
    SEA_RISK_MEDIUM = 1,   /* Moderate risk: curl, wget, git */
    SEA_RISK_HIGH = 2,     /* Dangerous: rm, dd, chmod */
    SEA_RISK_CRITICAL = 3  /* Extremely dangerous: rm -rf /, mkfs */
} SeaRiskLevel;

/* Risk classification result */
typedef struct {
    SeaRiskLevel level;
    const char*  reason;
    bool         requires_approval;
} SeaRiskResult;

/* Classify a shell command by risk level */
SeaRiskResult sea_risk_classify_command(const char* command);

/* Get risk level name */
const char* sea_risk_level_name(SeaRiskLevel level);

#endif /* SEA_RISK_H */
