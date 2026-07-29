#pragma once
// Define a custom message ID
#define WM_USER_TOOL (WM_USER + 1000)
#define WM_USER_A_CREATELINE (WM_USER + 1001)
#define WM_USER_A_CREATECIRCLE (WM_USER + 1002)
#define WM_USER_A_CREATERECTANGLE (WM_USER + 1003)
#define WM_USER_A_CREATERTEXT (WM_USER + 1004)
#define WM_USER_A_CREATELAYER (WM_USER + 1005)
#define WM_USER_A_LISTLAYERS (WM_USER + 1006)
#define WM_USER_A_CREATEPOLYLINE (WM_USER + 1007)
#define WM_USER_A_CREATEELLIPSE (WM_USER + 1008)
#define WM_USER_A_CREATESPLINE (WM_USER + 1009)
#define WM_USER_A_DELETEENTITIES (WM_USER + 1010)


#define WM_USER_A_REGEN (WM_USER + 3000)

#define WM_USER_CHAT (WM_USER + 4000)
#define WM_USER_CHAT_SEND_ERROR (WM_USER + 4001)

class cJSON;
// Add this struct definition in AiApiRequest.h or a common header
// In AiApiRequest.h or where the struct is defined
struct AiToolCommandData {
    cJSON* jsonRoot;      // Input JSON from HTTP request
    HANDLE hEvent;        // Sync event
	int ret;
    cJSON* resultJson;    // Output JSON result from CAD (can be NULL)
    
    // Constructor to initialize members
    AiToolCommandData() : jsonRoot(NULL), hEvent(NULL), ret(0), resultJson(NULL) {}
};