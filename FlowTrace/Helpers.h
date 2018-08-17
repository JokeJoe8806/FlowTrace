#pragma once

extern HWND hwndMain;
#define WM_UPDATE_FILTER               WM_USER + 1000
#define WM_INPORT_TASK                 WM_USER + 1001
#define WM_MOVE_SELECTION_TO_END       WM_USER + 1002
#define WM_SHOW_NGS                    WM_USER + 1003
#define WM_UPDATE_BACK_TRACE           WM_USER + 1004
struct LOG_NODE;
static const int ICON_LEN = 16;
static const int ICON_OFFSET = 16 + 4;
enum MENU_ICON { MENU_ICON_SYNC, MENU_ICON_FUNC_IN_ECLIPSE, MENU_ICON_CALL_IN_ECLIPSE, MENU_ICON_MAX };
namespace Helpers
{
	void CopyToClipboard(HWND hWnd, char* szText, int cbText);
	void SysErrMessageBox(CHAR* lpFormat, ...);
	void ErrMessageBox(CHAR* lpFormat, ...);
	CHAR* find_str(const CHAR *phaystack, const CHAR *pneedle, int bMatchCase);
	CHAR* str_format_int_grouped(__int64 num);
	void GetTime(DWORD &sec, DWORD& msec);
	void SetMenuIcon(HMENU hMenu, UINT item, MENU_ICON icon);
	void AddCommonMenu(LOG_NODE* pNode, HMENU hMenu, int& cMenu);
	void OnLButtonDoun(WPARAM wParam, LPARAM lParam);
	void OnLButtonUp(LOG_NODE* pNode, WPARAM wParam, LPARAM lParam);
	void UpdateStatusBar();
	void ShowInIDE(LOG_NODE* pNode, bool bShowCallSite);
	void ShowInIDE(char* src, int line, bool IsAndroidLog);
	void ShowNodeDetails(LOG_NODE* pNode);
};