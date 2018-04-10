#include <Windows.h>
#include <windowsx.h>
#include <CommCtrl.h>
#include <Wtsapi32.h>
#include <Strsafe.h>
#include <tchar.h>
#include <time.h>
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"Wtsapi32.lib")
#include "resource.h"

// TODO: Coluna calculando a diferença entre timestamps
// TODO: Events linked list
// TODO: Time since previous event
// TODO: Shell_NotifyIcon
// TODO: Change Icons
// TODO: Show message ballon on events
// TODO: Show summary message ballon on events
// TODO: Summary message when icon clicked

HINSTANCE g_hinst;
time_t g_startup_time;
#define LOF_FILE_NAME "ponto.log"
#define TIMESTAMP_SIZE 64
#define C_COLUMNS 3
#define EVENT_INCOMING 0
#define EVENT_OUTGOING 1
#define WMAPP_NOTIFYCALLBACK WM_APP + 1

int GetTextWidth(LPCTSTR szText)
{
	HDC hDC = GetDC(NULL);
	RECT rect = { 0, 0, 0, 0 };
	DrawText(hDC, szText, lstrlen(szText), &rect, DT_CALCRECT);

	return rect.right;
}

LPCTSTR GetRunningTimeStr()
{
	static TCHAR elapsedstr[512];

	time_t now;
	time(&now);

	if (!g_startup_time) {
		g_startup_time = now;
	}

	int interval = (int)difftime(now, g_startup_time);
	_sntprintf_s(elapsedstr, ARRAYSIZE(elapsedstr), ARRAYSIZE(elapsedstr) - 1,
		TEXT("%d hours %d minutes"),
		(int)interval / 3600,
		(int)(interval % 3600) / 60);

	return elapsedstr;
}

LPCTSTR GetTimestampStr()
{
	time_t now;
	time(&now);

	static TCHAR Timestamp[TIMESTAMP_SIZE];
	_tctime_s(Timestamp, TIMESTAMP_SIZE, &now);

	for (int i = 0; Timestamp[i]; i++)
		if (Timestamp[i] == '\n')
			Timestamp[i] = '\0';

	return Timestamp;
}

BOOL LogFileAppend(LPCTSTR Timestamp, LPCTSTR RunningTime, LPCTSTR lpLogMsg)
{
	FILE* LogFile = NULL;
	if (fopen_s(&LogFile, LOF_FILE_NAME, "a+") != 0)
		return FALSE;

	_ftprintf(LogFile, TEXT("[%s] %s %s\n"), Timestamp, RunningTime, lpLogMsg);

	fclose(LogFile);

	return TRUE;
}

void Log(HWND hWndListView, LPTSTR lpLogMsg, int nEvent)
{
	LPCTSTR Timestamp = GetTimestampStr();
	LPCTSTR RunningTime = GetRunningTimeStr();
	LVITEM item = { 0 };
	item.mask = LVIF_TEXT | LVIF_IMAGE;
	item.iImage = nEvent;
	item.pszText = lpLogMsg;
	ListView_InsertItem(hWndListView, &item);
	ListView_SetItemText(hWndListView, 0, 1, (LPTSTR)Timestamp);
	ListView_SetItemText(hWndListView, 0, 2, (LPTSTR)RunningTime);

	LogFileAppend(Timestamp, RunningTime, lpLogMsg);
}

void Arrive(HWND hWndListView, LPTSTR lpLogMsg)
{
	Log(hWndListView, lpLogMsg, EVENT_INCOMING);
}

void Depart(HWND hWndListView, LPTSTR lpLogMsg)
{
	Log(hWndListView, lpLogMsg, EVENT_OUTGOING);
}

BOOL InitListViewColumns(HWND hWndListView)
{
	TCHAR szColumnLabel[256];
	LVCOLUMN lvc;

	lvc.mask = LVCF_FMT | LVCF_WIDTH | LVCF_MINWIDTH | LVCF_TEXT | LVCF_SUBITEM;
	lvc.fmt = LVCFMT_LEFT;
	lvc.pszText = szColumnLabel;

	for (int i = 0; i < C_COLUMNS; i++) {
		lvc.iSubItem = i;
		LoadString(g_hinst, IDS_FIRSTCOLUMN + i, szColumnLabel,
			sizeof(szColumnLabel) / sizeof(szColumnLabel[0]));
		lvc.cx = 3 * GetTextWidth(szColumnLabel);
		lvc.cxMin = lvc.cx;

		if (ListView_InsertColumn(hWndListView, i, &lvc) == -1) {
			return FALSE;
		}
	}

	return TRUE;
}

BOOL InitListViewImageLists(HWND hWndListView)
{
	HICON hIconItem;
	HIMAGELIST hSmall;
	int i = 0;
	hSmall = ImageList_Create(GetSystemMetrics(SM_CXSMICON),
		GetSystemMetrics(SM_CYSMICON),
		ILC_MASK, 1, 1);

	int icons[] = {
		IDI_IN,
		IDI_OUT,
		0
	};

	for (i = 0; icons[i]; i++) {
		hIconItem = LoadIcon(g_hinst, MAKEINTRESOURCE(icons[i]));
		ImageList_AddIcon(hSmall, hIconItem);
		DestroyIcon(hIconItem);
	}

	ListView_SetImageList(hWndListView, hSmall, LVSIL_SMALL);

	return TRUE;
}

BOOL RegisterAutorun()
{
	TCHAR szCurrModuleFile[MAX_PATH + 1] = { 0 };
	TCHAR szRegDataValue[MAX_PATH + 1] = { 0 };
	GetModuleFileName(NULL, szCurrModuleFile, MAX_PATH);
	// OutputDebugString(szCurrModuleFile);
	_sntprintf_s(szRegDataValue, MAX_PATH, MAX_PATH, TEXT("\"%s\""), szCurrModuleFile);

	HKEY hKey;
	size_t nRegDataLen = _tcslen(szRegDataValue) * sizeof(TCHAR);
	if (RegOpenKeyEx(HKEY_CURRENT_USER,
		TEXT("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Run"),
		0,
		KEY_SET_VALUE,
		&hKey) != ERROR_SUCCESS)
	{
		return FALSE;
	}

	RegSetValueEx(hKey, TEXT("Ponto"), 0, REG_SZ, (CONST BYTE*)szRegDataValue, (DWORD)nRegDataLen);

	RegCloseKey(hKey);

	return TRUE;
}

void ShellNotifyIconAdd(HWND hWnd)
{
	NOTIFYICONDATA nid = { 0 };
	nid.cbSize = sizeof(nid);
	nid.hWnd = hWnd;
	nid.uVersion = NOTIFYICON_VERSION_4;
	// NIF_MESSAGE uCallbackMessage 
	// NIF_ICON hIcon 
	// NIF_TIP szTip 
	// NIF_STATE dwState dwStateMask 
	// NIF_INFO szInfo, szInfoTitle, dwInfoFlags
	// NIF_GUID guidItem 
	StringCchCopy(nid.szTip, ARRAYSIZE(nid.szTip), TEXT("nid.szTip"));
	// StringCchCopy(nid.szInfo, ARRAYSIZE(nid.szInfo), L"Test application");
	// StringCchCopy(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), L"Test application");
	// dwInfoFlags.dwInfoFlags = NIIF_USER; // hIcon hBalloonIcon 
	nid.uFlags |= NIF_TIP;
	nid.uFlags |= NIF_ICON;
	nid.hIcon = LoadIcon(g_hinst, MAKEINTRESOURCE(IDI_IN));
	nid.uFlags |= NIF_MESSAGE;
	nid.uCallbackMessage = WMAPP_NOTIFYCALLBACK;

	Shell_NotifyIcon(NIM_ADD, &nid);
}

void ShellNotifyIconMsg(HWND hWnd)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = hWnd;
	StringCchCopy(nid.szInfo, ARRAYSIZE(nid.szInfo), TEXT("nid.szInfo"));
	StringCchCopy(nid.szInfoTitle, ARRAYSIZE(nid.szInfoTitle), TEXT("nid.szInfoTitle"));
	nid.uFlags |= NIF_INFO;
	nid.dwInfoFlags = NIIF_USER | NIIF_LARGE_ICON;
	nid.hIcon = nid.hBalloonIcon = LoadIcon(g_hinst, MAKEINTRESOURCE(IDI_OUT));
	Shell_NotifyIcon(NIM_MODIFY, &nid);
}

void ShellNotifyIconDel(HWND hWnd)
{
	NOTIFYICONDATA nid = { sizeof(nid) };
	nid.hWnd = hWnd;
	Shell_NotifyIcon(NIM_DELETE, &nid);
}

HWND InitStatusBar(HWND hwndParent, HMENU idStatus, HINSTANCE hinst)
{
	HWND hwndStatus;
	INT nParts = -1;

	// Create the status bar.
	hwndStatus = CreateWindowEx(
		0,                       // no extended styles
		STATUSCLASSNAME,         // name of status bar class
		(PCTSTR)NULL,            // no text when first created
		SBARS_SIZEGRIP |         // includes a sizing grip
		WS_CHILD | WS_VISIBLE,   // creates a visible child window
		0, 0, 0, 0,              // ignores size and position
		hwndParent,              // handle to parent window
		idStatus,                // child window identifier
		hinst,                   // handle to application instance
		NULL);                   // no window creation data

	// Tell the status bar to create the window parts.
	SendMessage(hwndStatus, SB_SETPARTS, 1, (LPARAM)&nParts);

	return hwndStatus;
}

void OnSize(HWND hWnd, UINT state, int cx, int cy)
{
	// TODO: remove GetClientRect and use cx,cy
	RECT rcStatus;
	RECT rcClient;
	HWND hStatus = GetDlgItem(hWnd, IDC_STATUS);
	SendMessage(hStatus, WM_SIZE, 0, 0);
	GetWindowRect(hStatus, &rcStatus);
	GetClientRect(hWnd, &rcClient);
	SetWindowPos(GetDlgItem(hWnd, IDC_LIST), NULL, 0, 0, rcClient.right,
		rcClient.bottom - (rcStatus.bottom - rcStatus.top), SWP_NOZORDER);
}

void OnTimer(HWND hWnd, UINT id)
{
	time_t rawtime;
	struct tm timeinfo;
	time(&rawtime);
	localtime_s(&timeinfo, &rawtime);
	if (!timeinfo.tm_hour) {
		g_startup_time = 0;
	}

	SendDlgItemMessage(hWnd, IDC_STATUS, SB_SETTEXT, MAKEWPARAM(0, 0),
		(LPARAM)GetRunningTimeStr());
}

void OnClose(HWND hWnd)
{
	Depart(GetDlgItem(hWnd, IDC_LIST), TEXT("WM_CLOSE"));
	ShellNotifyIconDel(hWnd);
	DestroyWindow(hWnd);
}

BOOL OnQueryEndSession(HWND hWnd)
{
	Depart(GetDlgItem(hWnd, IDC_LIST), TEXT("Logoff"));
	return TRUE;
}

BOOL OnInitDialog(HWND hWnd, HWND hwndFocus, LPARAM lParam)
{
	HWND hWndListView = GetDlgItem(hWnd, IDC_LIST);
	InitListViewImageLists(hWndListView);
	InitListViewColumns(hWndListView);
	InitStatusBar(hWnd, (HMENU)IDC_STATUS, g_hinst);
	OnSize(hWnd, SIZE_RESTORED, 0, 0);
	WTSRegisterSessionNotification(hWnd, NOTIFY_FOR_THIS_SESSION);
	RegisterAutorun();
	ShellNotifyIconAdd(hWnd);
	SetTimer(hWnd, 0, 60000, NULL);
	Arrive(hWndListView, TEXT("Logon"));

	return TRUE;
}

#define HANDLE_WM_WTSSESSION_CHANGE(hWnd, wParam, lParam, fn) \
    ((fn)((hWnd), (UINT)(wParam), (LONG_PTR)(lParam)), 0L)

void OnWTSSessionChange(HWND hWnd, UINT_PTR reason, LONG_PTR nSssionId)
{
	switch (reason)
	{
	case WTS_SESSION_LOCK:
		Depart(GetDlgItem(hWnd, IDC_LIST), TEXT("WTS_SESSION_LOCK"));
		break;
	case WTS_SESSION_UNLOCK:
		Arrive(GetDlgItem(hWnd, IDC_LIST), TEXT("WTS_SESSION_UNLOCK"));
		break;
	}
}

void OnCommand(HWND hWnd, int id, HWND hwndCtl, UINT codeNotify)
{
	switch (id) {
	case ID_HELP_ABOUT:
		MessageBox(hWnd, TEXT("Ponto about box."), TEXT("Ponto"), MB_OK | MB_ICONINFORMATION);
		break;
	case ID_FILE_EXIT:
		SendMessage(hWnd, WM_CLOSE, 0, 0);
		break;
	}
}

INT_PTR CALLBACK PontoDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	HANDLE_MSG(hWnd, WM_INITDIALOG, OnInitDialog);
	HANDLE_MSG(hWnd, WM_SIZE, OnSize);
	HANDLE_MSG(hWnd, WM_TIMER, OnTimer);
	HANDLE_MSG(hWnd, WM_QUERYENDSESSION, OnQueryEndSession);
	HANDLE_MSG(hWnd, WM_CLOSE, OnClose);
	HANDLE_MSG(hWnd, WM_COMMAND, OnCommand);
	HANDLE_MSG(hWnd, WM_WTSSESSION_CHANGE, OnWTSSessionChange);
	case WMAPP_NOTIFYCALLBACK:
		switch (LOWORD(lParam))
		{
		case WM_LBUTTONUP:
			// MessageBox(NULL, TEXT("WM_LBUTTONUP"), TEXT("WMAPP_NOTIFYCALLBACK"), MB_OK | MB_ICONINFORMATION);
			ShowWindow(hWnd, IsWindowVisible(hWnd) ? SW_HIDE : SW_SHOW);
			break;
		case WM_RBUTTONUP:
			// MessageBox(NULL, TEXT("WM_RBUTTONUP"), TEXT("WMAPP_NOTIFYCALLBACK"), MB_OK | MB_ICONINFORMATION);
			ShowWindow(hWnd, IsWindowVisible(hWnd)?SW_HIDE:SW_SHOW);
			break;
		}
		break;
	}

	return FALSE;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
	MSG msg;
	g_hinst = hInstance;
	HWND hDlg = CreateDialog(hInstance, MAKEINTRESOURCE(IDD_PONTO), NULL, PontoDlgProc);
	if (!hDlg) {
		return 1;
	}

	ShowWindow(hDlg, SW_HIDE);

	while (GetMessage(&msg, hDlg, 0, 0) > 0) {
		if (!IsWindow(hDlg) || !IsDialogMessage(hDlg, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	return 0;
}
