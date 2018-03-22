#include <Windows.h>
#include <CommCtrl.h>
#include <Wtsapi32.h>
#include <Strsafe.h>
#include <tchar.h>
#include <time.h>
#pragma comment(lib,"comctl32.lib")
#pragma comment(lib,"Wtsapi32.lib")
#include "resource.h"

// TODO: Write logs to file
// TODO: Coluna calculando a diferença entre timestamps
// TODO: Events linked list
// TODO: Time since previous event
// TODO: Shell_NotifyIcon
// TODO: Message Cracker Macros
// TODO: Resizeable / MoveWndow / GetParent
// TODO: Change Icons
// TODO: Show message ballon on events
// TODO: Show summary message ballon on events
// TODO: Summary message when icon clicked

HINSTANCE g_hinst;
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

void Log(HWND hWndListView, LPTSTR lpLogMsg, int nEvent)
{
	static time_t first_event = 0;

	time_t now;
	time(&now);

	if (!first_event) {
		first_event = now;
	}

	TCHAR elapsedstr[512];
	int interval = (int)difftime(now, first_event);
	_sntprintf_s(elapsedstr, ARRAYSIZE(elapsedstr), ARRAYSIZE(elapsedstr)-1,
		TEXT("%d hours %d minutes"),
		(int)interval/3600,
		(int)(interval%3600)/60);

	LVITEM item = { 0 };
	item.mask = LVIF_TEXT | LVIF_IMAGE;
	item.iImage = nEvent;
	item.pszText = lpLogMsg;
	ListView_InsertItem(hWndListView, &item);
	ListView_SetItemText(hWndListView, 0, 1, _tctime(&now));
	ListView_SetItemText(hWndListView, 0, 2, elapsedstr);
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

	RegSetValueEx(hKey, TEXT("Ponto"), 0, REG_SZ, (CONST BYTE*)szRegDataValue, nRegDataLen);

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

INT_PTR CALLBACK PontoDlgProc(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static HWND hWndListView = NULL;

	switch (uMsg)
	{
	case WM_INITDIALOG:
		hWndListView = GetDlgItem(hwndDlg, IDC_LIST);
		InitListViewImageLists(hWndListView);
		InitListViewColumns(hWndListView);
		WTSRegisterSessionNotification(hwndDlg, NOTIFY_FOR_THIS_SESSION);
		RegisterAutorun();
		ShellNotifyIconAdd(hwndDlg);
		SetTimer(hwndDlg, 0, 600000, NULL);
		Arrive(hWndListView, TEXT("Logon"));
		return TRUE;
		break;
	case WM_TIMER:
	{
		time_t rawtime;
		struct tm * timeinfo;
		time(&rawtime);
		timeinfo = localtime(&rawtime);
		if (!timeinfo->tm_hour) {
			// TODO: Clear events by midnight
		}
	}
		break;
	case WMAPP_NOTIFYCALLBACK:
		switch (LOWORD(lParam))
		{
		case WM_LBUTTONUP:
			// MessageBox(NULL, TEXT("WM_LBUTTONUP"), TEXT("WMAPP_NOTIFYCALLBACK"), MB_OK | MB_ICONINFORMATION);
		case WM_RBUTTONUP:
			// MessageBox(NULL, TEXT("WM_RBUTTONUP"), TEXT("WMAPP_NOTIFYCALLBACK"), MB_OK | MB_ICONINFORMATION);
			ShowWindow(hwndDlg, IsWindowVisible(hwndDlg)?SW_HIDE:SW_SHOW);
			break;
		}
		break;
	case WM_QUERYENDSESSION:
		Depart(hWndListView, TEXT("Logoff"));
		break;
	case WM_WTSSESSION_CHANGE:
		switch (wParam)
		{
		case WTS_SESSION_LOCK:
			Depart(hWndListView, TEXT("WTS_SESSION_LOCK"));
			break;
		case WTS_SESSION_UNLOCK:
			Arrive(hWndListView, TEXT("WTS_SESSION_UNLOCK"));
			break;
		}
		break;
	case WM_CLOSE:
		ShellNotifyIconDel(hwndDlg);
		DestroyWindow(hwndDlg);
		return TRUE;
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
