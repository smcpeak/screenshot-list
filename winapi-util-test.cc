// winapi-util-test.cc
// Tests for `winapi-util`.

// For the moment, these are just something I run manually.

#include "winapi-util.h"               // this module

#include <iostream>                    // std::{wcout, wcerr}
#include <vector>                      // std::vector

#include <windows.h>                   // Windows API


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                    PWSTR pCmdLine, int nCmdShow)
{
  int argc;
  wchar_t **argv = CommandLineToArgvW(pCmdLine, &argc);
  if (!argv) {
    winapiDie(L"CommandLineToArgvW");
  }

  // This array does *not* begin with the executable name.
  //
  // Except!  If `pCmdLine` is the empty string, then
  // `CommandLineToArgvW` will "helpfully" put the executable name as
  // the first argument!  WTF?  How broken can this API get?  So I check
  // for that below.
  //
  std::vector<std::wstring> args(argv, argv+argc);

  if (args.size() >= 1 && *pCmdLine != 0) {
    if (args[0] == L"createParents") {
      if (args.size() >= 2) {
        createParentDirectoriesOf(args[1]);
      }
      else {
        std::wcerr << L"createParents needs an argument\n";
        return 2;
      }
    }

    else {
      std::wcerr << L"unknown command: " << args[0] << L"\n";
    }
  }

  else {
    std::wcout << L"usage: winapi-util-test <command> <arg>\n";
  }

  return 0;
}


// EOF
