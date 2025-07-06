#include <iostream>
#include <Windows.h>
#include <string>
#include <vector>
#include <io.h>
#include <iomanip>
#include <thread>
#include <Shlobj.h>
#pragma comment(lib, "Shell32.lib")
#pragma comment(lib, "shlwapi.lib")


bool IsRunningAsAdmin()
{
	BOOL isAdmin = FALSE;
	PSID adminGroup = NULL;

	SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
	if (AllocateAndInitializeSid(&ntAuthority, 2, SECURITY_BUILTIN_DOMAIN_RID,
		DOMAIN_ALIAS_RID_ADMINS, 0, 0, 0, 0, 0, 0, &adminGroup))
	{
		CheckTokenMembership(NULL, adminGroup, &isAdmin);
		FreeSid(adminGroup);
	}

	return isAdmin;
}
enum ConsoleColor
{
    DEFAULT = 7,
    GREEN = 10,
	YELLOW = 14,
    RED = 12,
	MAGENTA = 13,
};

void SetConsoleColor(ConsoleColor color)
{
	HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
	SetConsoleTextAttribute(hConsole, color);
}

bool FileExists(const std::wstring& path)
{
	return _waccess(path.c_str(), 0) == 0;
}

bool DeleteFileOrDirectory(const std::wstring& path)
{
	if (!FileExists(path)) { return false; }

	DWORD attrs = GetFileAttributesW(path.c_str());

	if (attrs == INVALID_FILE_ATTRIBUTES) { return false; }

	if (attrs & FILE_ATTRIBUTE_DIRECTORY) 
	{ 
		return RemoveDirectoryW(path.c_str()) != 0; 
	}
	else
	{
		return DeleteFileW(path.c_str()) != 0;
	}


}

bool DeleteRegistryKey(const std::wstring& keyPath)
{
	size_t pos = keyPath.find(L'\\');
	if (pos == std::wstring::npos) { return false; }

	std::wstring root = keyPath.substr(0, pos);
	std::wstring subKey = keyPath.substr(pos + 1);

	HKEY hRoot;
	if (root == L"HKEY_CURRENT_USER") hRoot = HKEY_CURRENT_USER;
	else if (root == L"HKEY_LOCAL_MACHINE") hRoot = HKEY_LOCAL_MACHINE;
	else return false;

	LSTATUS status = RegDeleteTreeW(hRoot, subKey.c_str());
	if (status == ERROR_FILE_NOT_FOUND)
	{
		return false;
	}
	return status == ERROR_SUCCESS;
}

std::wstring ExpandUserPath(const std::wstring& path)
{
	std::wstring expanded = path;
	size_t pos;

	pos = expanded.find(L"{User}");
	if (pos != std::wstring::npos)
	{
		wchar_t username[256];
		DWORD size = sizeof(username) / sizeof(username[0]);
		GetUserNameW(username, &size);
		expanded.replace(pos, 6, username);
	}


	pos = expanded.find(L"%USERPROFILE%");
	if (pos != std::wstring::npos)
	{
		wchar_t profilePath[MAX_PATH];
		if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_PROFILE, NULL, 0, profilePath)))
		{
			expanded.replace(pos, 13, profilePath);
		}
	}


	pos = expanded.find(L"$@\"");
	if (pos != std::wstring::npos)
	{
		expanded.replace(pos, 3, L"");
	}

	if (!expanded.empty() && expanded.front() == L'"')
	{
		expanded.erase(0, 1);
	}

	if (!expanded.empty() && expanded.back() == L'"')
	{
		expanded.pop_back();
	}

	return expanded;
}

void ProcessFiles(const std::vector<std::wstring>& filePaths)
{
	std::wcout << L"Processing files and directories:\n";
	std::wcout << L"--------------------------------\n";

	for (const auto& path : filePaths)
	{
		std::wstring expandedPath = ExpandUserPath(path);

		if (!FileExists(expandedPath))
		{
			SetConsoleColor(YELLOW);
			std::wcout << L"[Not Found]  " << expandedPath << L"\n";
			SetConsoleColor(DEFAULT);
			continue;
		}

		if (DeleteFileOrDirectory(expandedPath))
		{
			SetConsoleColor(GREEN);
			std::wcout << L"[Deleted]  " << expandedPath << L"\n";
			SetConsoleColor(DEFAULT);
		}
		else
		{
			SetConsoleColor(RED);
			std::wcout << L"[Failed]  " << expandedPath << L"\n";
			SetConsoleColor(DEFAULT);
		}
	}
}

void ProcessRegistryKeys(const std::vector<std::wstring>& regKeys)
{
	std::wcout << L"\nProcessing Registry Keys:\n";
	std::wcout << L"--------------------------\n";

	for (const auto& key : regKeys)
	{
		std::wstring expandedKey = ExpandUserPath(key);

		if (DeleteRegistryKey(expandedKey))
		{
			SetConsoleColor(GREEN);
			std::wcout << L"[Deleted]  " << expandedKey << L"\n";
			SetConsoleColor(DEFAULT);
		}
		else
		{
			DWORD err = GetLastError();
			if (err == ERROR_FILE_NOT_FOUND)
			{
				SetConsoleColor(YELLOW);
				std::wcout << L"[Not Found]  " << expandedKey << L"\n";
				SetConsoleColor(DEFAULT);
			}
			else
			{
				SetConsoleColor(RED);
				std::wcout << L"[Failed]  " << expandedKey << L" (Error: " << err << L")\n";
				SetConsoleColor(DEFAULT);
			}

		}

	}
}
int main()
{
	if (!IsRunningAsAdmin())
	{
		// not running as admin? Force it LMFAO
		wchar_t exePath[MAX_PATH];
		GetModuleFileNameW(NULL, exePath, MAX_PATH);

		SHELLEXECUTEINFOW sei = { sizeof(sei) };
		sei.lpVerb = L"runas"; // UAC prompt we love the good old UAC ghetto method
		sei.lpFile = exePath;
		sei.hwnd = NULL;
		sei.nShow = SW_SHOWNORMAL;

		if (!ShellExecuteExW(&sei))
		{
			DWORD err = GetLastError();
			if (err == ERROR_CANCELLED)
				MessageBoxW(NULL, L"Start as admin.", L"Warning", MB_ICONWARNING);
			else
				MessageBoxW(NULL, L"Failed to get Admin privileges.", L"Error", MB_ICONERROR);
			return 1;
		}

		return 0; // Exit not admin procces yes
	}
	HWND hwnd = GetConsoleWindow();
	SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
	SetLayeredWindowAttributes(hwnd, 0, 225, LWA_ALPHA);
	SetConsoleTitleA("Flat Patcher For Rec Room - Written By Pxcket :D"); //if you skid this ur genuinely a Loser

	SetConsoleOutputCP(CP_UTF8);

	std::vector<std::wstring> filePaths = {
	L"$@\"C:\\Users\\{User}\\AppData\\LocalLow\\Against Gravity\"",
	L"$@\"C:\\Users\\{User}\\AppData\\Local\\Temp\\RecRoom\"",
	L"$@\"C:\\Users\\{User}\\AppData\\Roaming\\Microsoft\\Windows\\Recent\\RecRoom.lnk\"",
	L"$@\"C:\\Users\\{User}\\AppData\\Roaming\\Microsoft\\Windows\\Start Menu\\Programs\\Steam\\Rec Room.url\"",
	L"\"C:\\Windows\\Prefetch\\RECROOM.EXE-BEC42EED.pf\"",
	L"\"C:\\Windows\\Prefetch\\RECROOM_RELEASE.EXE-35556F3D.pf\"",
	L"$@\"{User}\\AppData\\Local\\Temp\\Against Gravity\"",
	L"$@\"{User}\\AppData\\LocalLow\\Against Gravity\"",
	L"$@\"{User}\\AppData\\Roaming\\recroom-launcher\"",
	L"$@\"{User}\\AppData\\Local\\Temp\\RecRoom\"" // no idea if these even work but fuck it 
	};


	std::vector<std::wstring> regKeys = {
	L"HKEY_CURRENT_USER\\SOFTWARE\\Against Gravity",
	L"HKEY_CURRENT_USER\\SOFTWARE\\Classes\\recroom",
	L"HKEY_CURRENT_USER\\SOFTWARE\\Valve\\Steam\\Apps\\471710",
	L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Steam App 471710",
	L"HKEY_CURRENT_USER\\SOFTWARE\\Valve\\Steam\\Apps\\471710",
	L"HKEY_LOCAL_MACHINE\\SYSTEM\\CurrentControlSet\\Services\\SharedAccess\\Parameters\\FirewallPolicy\\FirewallRules"
	};

	ProcessFiles(filePaths);
	ProcessRegistryKeys(regKeys);
	std::wcout << L"--------------------------\n";


	ShellExecuteW(NULL, L"open", L"https://github.com/Pxcket/", 0, 0, SW_SHOWNORMAL);

	SetConsoleColor(MAGENTA);
	std::wcout << L"Please Leave Some Stars On Github :D\n\n\n";  
	SetConsoleColor(DEFAULT);


	std::wcout << L"Patching completed. Closing in: 5";   // I Hate This Method But it works ig LMFAO
	for (int i = 4; i > 0; --i) {   
		Sleep(1000); 
		std::wcout << L"\rPatching completed. Closing in: " << i << "  ";
		std::wcout.flush(); 
	}
	Sleep(1000);
	std::wcout << L"\rPatching completed. Done.      \n"; 
	return 0;
}

