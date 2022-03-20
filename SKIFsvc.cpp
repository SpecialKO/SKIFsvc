// SKIFsvc.cpp : Defines the entry point for the application.
//

#include <Windows.h>
#include <shlwapi.h>
#include <string>

BOOL FileExists(LPCTSTR szPath)
{
  DWORD dwAttrib = GetFileAttributes(szPath);

  return  (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         !(dwAttrib  & FILE_ATTRIBUTE_DIRECTORY));
}

using DLL_t = void (WINAPI *)(HWND hwnd, HINSTANCE hInst, LPCSTR lpszCmdLine, int nCmdShow);

int APIENTRY wWinMain(_In_     HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_     LPWSTR    lpCmdLine,
                      _In_     int       nCmdShow)
{
  UNREFERENCED_PARAMETER(hInstance);
  UNREFERENCED_PARAMETER(hPrevInstance);
  UNREFERENCED_PARAMETER(nCmdShow);

  struct SKIFsvc_Signals {
    BOOL Stop          = FALSE;
    BOOL Start         = FALSE;
  } _Signal;

  _Signal.Stop =
    StrStrIW (lpCmdLine, L"Stop")     != NULL;

  _Signal.Start =
    StrStrIW (lpCmdLine, L"Start")    != NULL;

  // Autostarting SKIFsvc through the registry autorun method 
  //   defaults the working directory to C:\WINDOWS\system32.
  wchar_t wszCurrentPath[MAX_PATH + 2] = { };
  GetCurrentDirectory (MAX_PATH, wszCurrentPath);

  // We only change if \Windows\sys is discovered to allow users 
  //   weird setups where the service hosts are located elsewhere.
  if (StrStrIW (wszCurrentPath, LR"(\Windows\sys)"))
  {
    wchar_t wszCorrectPath[MAX_PATH + 2] = { };
    HMODULE hModSelf = GetModuleHandleW (nullptr);
    GetModuleFileNameW  (hModSelf, wszCorrectPath, MAX_PATH);
    PathRemoveFileSpecW (wszCorrectPath);
    SetCurrentDirectory (wszCorrectPath);
  }

  // Past this point we can assume to be in the proper working directory.

#if _WIN64
  PCWSTR wszDllPath  = L"SpecialK64.dll";

  if (FileExists (LR"(..\SpecialK64.dll)"))
    wszDllPath =  LR"(..\SpecialK64.dll)";
#else
  PCWSTR wszDllPath  = L"SpecialK32.dll";

  if (FileExists (LR"(..\SpecialK32.dll)"))
    wszDllPath =  LR"(..\SpecialK32.dll)";
#endif

  DWORD lastError = 0;
  SetLastError (NO_ERROR);

  auto SKModule = LoadLibrary (wszDllPath);

  if (SKModule != NULL)
  {
    auto RunDLL_InjectionManager = (DLL_t)GetProcAddress (SKModule, "RunDLL_InjectionManager");

    if (_Signal.Stop)
      RunDLL_InjectionManager (0, 0, "Remove", SW_HIDE);

    else if (_Signal.Start)
      RunDLL_InjectionManager (0, 0, "Install", SW_HIDE);

    else
    {
#if _WIN64
      HANDLE hService = OpenEvent (EVENT_MODIFY_STATE, FALSE, LR"(Local\SK_GlobalHookTeardown64)");
#else
      HANDLE hService = OpenEvent (EVENT_MODIFY_STATE, FALSE, LR"(Local\SK_GlobalHookTeardown32)");
#endif

      if (hService != NULL)
      {
        SetEvent    (hService);
        CloseHandle (hService);
      }

      else
      {
        RunDLL_InjectionManager (0, 0, "Install", 0);
      }
    }

    FreeLibrary (SKModule);
  }
  else
  {
    lastError = GetLastError ( );
    wchar_t wsLastError[256];
    
    FormatMessageW (
      FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
      NULL, lastError, MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
      wsLastError, (sizeof (wsLastError) / sizeof (wchar_t)), NULL
    );

    MessageBox (NULL, (L"There was a problem starting " + std::wstring(wszDllPath) + L"\n\n" + wsLastError).c_str(), L"SKIFsvc", MB_OK | MB_ICONERROR);
  }

  return lastError;
}