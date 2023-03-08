// SKIFsvc.cpp : Defines the entry point for the application.
//

#include <Windows.h>
#include <shlwapi.h>
#include <string>
#include <memory>

BOOL FileExists (LPCTSTR szPath)
{
  DWORD    dwAttrib  = GetFileAttributes (szPath);

  return  (dwAttrib != INVALID_FILE_ATTRIBUTES && 
         !(dwAttrib  & FILE_ATTRIBUTE_DIRECTORY));
}

void
ShowErrorMessage (DWORD lastError, std::wstring preMsg = L"", std::wstring winTitle = L"")
{
  LPWSTR messageBuffer = nullptr;

  size_t size = FormatMessageW (FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                NULL, lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&messageBuffer, 0, NULL);

  std::wstring message (messageBuffer, size);
  LocalFree (messageBuffer);

  message.erase (std::remove (message.begin(), message.end(), '\n'), message.end());

  if (! preMsg.empty())
    preMsg += L"\n\n";

  MessageBox (NULL, (preMsg + L"[" +std::to_wstring(lastError) + L"] " + message).c_str(),
                     winTitle.c_str(), MB_OK | MB_ICONERROR);
}

// Suppress warnings about _vsnwprintf
#pragma warning(disable:4996)
std::wstring
__cdecl
SK_FormatStringW (wchar_t const* const _Format, ...)
{
  size_t len = 0;

  va_list   _ArgList;
  va_start (_ArgList, _Format);
  {
    len =
      _vsnwprintf ( nullptr, 0, _Format, _ArgList ) + 1ui64;
  }
  va_end   (_ArgList);

  size_t alloc_size =
    sizeof (wchar_t) * (len + 2);

  std::unique_ptr <wchar_t []> pData =
    std::make_unique <wchar_t []> (alloc_size);

  if (! pData)
    return std::wstring ();

  va_start (_ArgList, _Format);
  {
    len =
      _vsnwprintf ( (wchar_t *)pData.get (), len + 1, _Format, _ArgList );
  }
  va_end   (_ArgList);

  return
    pData.get ();
}

using DLL_t = void (WINAPI *)(HWND hwnd, HINSTANCE hInst, LPCSTR lpszCmdLine, int nCmdShow);

int APIENTRY wWinMain(_In_     HINSTANCE hInstance,
                      _In_opt_ HINSTANCE hPrevInstance,
                      _In_     LPWSTR    lpCmdLine,
                      _In_     int       nCmdShow)
{
  UNREFERENCED_PARAMETER (hInstance);
  UNREFERENCED_PARAMETER (hPrevInstance);
  UNREFERENCED_PARAMETER (nCmdShow);

  struct SKIFsvc_Signals {
    BOOL Stop          = FALSE;
    BOOL Start         = FALSE;
#ifndef _WIN64
    BOOL Proxy64       = FALSE;
#endif
  } _Signal;

  _Signal.Stop =
    StrStrIW (lpCmdLine, L"Stop")     != NULL;

  _Signal.Start =
    StrStrIW (lpCmdLine, L"Start")    != NULL;
#ifndef _WIN64
  _Signal.Proxy64 =
    StrStrIW (lpCmdLine, L"Proxy64")  != NULL;
#endif

  // Autostarting SKIFsvc through the registry autorun method 
  //   defaults the working directory to C:\WINDOWS\system32.
  wchar_t wszCurrentPath[MAX_PATH + 2] = { };
  GetCurrentDirectory   (MAX_PATH, wszCurrentPath);

  // Executable path
  wchar_t wszExeFolder[MAX_PATH + 2] = { };
  HMODULE hModSelf = GetModuleHandleW (nullptr);
  GetModuleFileNameW  (hModSelf, wszExeFolder, MAX_PATH);
  PathRemoveFileSpecW (wszExeFolder);

  // We only change if \Windows\sys is discovered to allow users 
  //   weird setups where the service hosts are located elsewhere.
  if (StrStrIW (wszCurrentPath, LR"(\Windows\sys)"))
    SetCurrentDirectory (wszExeFolder);

  // Now we have to try and locate the DLL file...
#if _WIN64
  std::wstring wsDllFile = L"SpecialK64.dll";
#else
  std::wstring wsDllFile = L"SpecialK32.dll";
#endif

  std::wstring wsDllPath;
  std::wstring wsTestPaths[] = { 
                                      wsDllFile,
                          LR"(..\)" + wsDllFile,
    SK_FormatStringW (LR"(%ws\%ws)",    wszExeFolder, wsDllFile.c_str()),
    SK_FormatStringW (LR"(%ws\..\%ws)", wszExeFolder, wsDllFile.c_str()),
  };

  for (auto& path : wsTestPaths)
  {
    if (FileExists (path.c_str()))
    {
      wsDllPath = path;
      break;
    }
  }

  // At this point we should have an idea of where the DLL file is located

#ifndef _WIN64
  // For newer versions we allow proxying the call to our 64-bit sibling
  if (_Signal.Proxy64)
  {
    SHELLEXECUTEINFOW
    sexi              = { };
    sexi.cbSize       = sizeof (SHELLEXECUTEINFOW);
    sexi.lpVerb       = L"OPEN";
    sexi.lpFile       = L"SKIFsvc64.exe";
    sexi.lpParameters = lpCmdLine;
  //sexi.lpDirectory  = L"Servlet"; // Be sure to proxy our own working directory to SKIFsvc64
    sexi.nShow        = SW_HIDE;
    sexi.fMask        = SEE_MASK_FLAG_NO_UI | /* SEE_MASK_NOCLOSEPROCESS | */
                        SEE_MASK_NOASYNC    | SEE_MASK_NOZONECHECKS;

    ShellExecuteExW (&sexi);

    Sleep (50);
  }
#endif

  DWORD lastError = NO_ERROR;
  SetLastError (    NO_ERROR);

  auto SKModule = LoadLibrary (wsDllPath.c_str());

  if (SKModule != NULL)
  {
    auto RunDLL_InjectionManager = (DLL_t)GetProcAddress (SKModule, "RunDLL_InjectionManager");

    if (_Signal.Stop)
      RunDLL_InjectionManager (0, 0, "Remove",  SW_HIDE);

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
        RunDLL_InjectionManager (0, 0, "Install", SW_HIDE);
      }
    }

    Sleep (50);

    FreeLibrary (SKModule);
  }
  else
  {
    lastError = GetLastError ( );
    ShowErrorMessage (lastError, (L"There was a problem starting " + wsDllFile), L"SKIFsvc");
  }

  return lastError;
}