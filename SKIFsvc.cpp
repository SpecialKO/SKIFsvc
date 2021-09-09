// SKIFsvc.cpp : Defines the entry point for the application.
//

#include <Windows.h>
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
    UNREFERENCED_PARAMETER(lpCmdLine);
    UNREFERENCED_PARAMETER(nCmdShow);

#if _WIN64
    std::wstring DllPath = L"SpecialK64.dll";
#else
    std::wstring DllPath = L"SpecialK32.dll";
#endif

    if ( FileExists( (LR"(..\)"    + DllPath).c_str() ) )
      DllPath       = LR"(..\)"    + DllPath;
    
    auto SKModule = LoadLibrary( DllPath.c_str() );

    if (SKModule != NULL)
    {
      auto RunDLL_InjectionManager = (DLL_t)GetProcAddress(SKModule, "RunDLL_InjectionManager");
      RunDLL_InjectionManager(0, 0, "Install", 0);
    }

    return GetLastError();
}