#include "stdafx.h"
#include "ToolDraw.h"
#include "cJSON.h"
#include "adscodes.h"

ToolDraw::ToolDraw(void)
{
}

ToolDraw::~ToolDraw(void)
{
}

void ToolDraw::create_line(cJSON* params)
{
    // 1. Get JSON items
    cJSON* x1Item = cJSON_GetObjectItem(params, "x1");
    cJSON* y1Item = cJSON_GetObjectItem(params, "y1");
    cJSON* x2Item = cJSON_GetObjectItem(params, "x2");
    cJSON* y2Item = cJSON_GetObjectItem(params, "y2");

    // 2. Validate and extract values
    if (!x1Item || !y1Item || !x2Item || !y2Item ||
        !cJSON_IsNumber(x1Item) || !cJSON_IsNumber(y1Item) ||
        !cJSON_IsNumber(x2Item) || !cJSON_IsNumber(y2Item)) {
        // Handle error: invalid parameters
        return;
    }

    double x1 = x1Item->valuedouble;
    double y1 = y1Item->valuedouble;
    double x2 = x2Item->valuedouble;
    double y2 = y2Item->valuedouble;

    // 3. Format coordinates into strings
    // Using a buffer size large enough to hold the coordinate string
    TCHAR pt1Str[64];
    TCHAR pt2Str[64];

    // Format as "x,y"
    _stprintf_s(pt1Str, _T("%f,%f"), x1, y1);
    _stprintf_s(pt2Str, _T("%f,%f"), x2, y2);

    // 4. Execute the LINE command
    int result = acedCommand(
        RTSTR, _T("_LINE"),      // Start LINE command
        RTSTR, pt1Str,           // First point
        RTSTR, pt2Str,           // Second point
        RTSTR, _T(""),           // End point (empty string to finish)
        RTNONE                   // End of command sequence
    );
}
