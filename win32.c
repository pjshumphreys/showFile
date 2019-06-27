#include <windows.h>
#include <stdio.h>
#include <conio.h>

#ifndef TRUE
  #define FALSE 0
  #define TRUE !FALSE
#endif

#define KEY_RESIZE 800
int width = 0;
int height = 0;
int gotch = 0;
int smart;

HANDLE inputHandle;
HANDLE outputHandle;
INPUT_RECORD record[500];
DWORD numRead;

int isNotWine() {
  static const char *(CDECL *pwine_get_version)(void);
  HMODULE hntdll = GetModuleHandle("ntdll.dll");

  if(!hntdll) {
    return TRUE;
  }

  pwine_get_version = (void *)GetProcAddress(hntdll, "wine_get_version");

  if(pwine_get_version) {
    return FALSE;
  }

  return TRUE;
}

int pollWindowSize() {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  int columns, rows;

  GetConsoleScreenBufferInfo(outputHandle, &csbi);
  columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

  if(width == 0) {
    width = columns;
    height = rows;

    return FALSE;
  }

  if(columns != width || rows != height) {
    width = columns;
    height = rows;

    return TRUE;
  }

  return FALSE;
}

int ProcessStdin(void) {
  int blah;

  if(!ReadConsoleInput(inputHandle, &record, smart?1:500, &numRead)) {
    // hmm handle this error somehow...
    return 0;
  }

  if(record[0].EventType != KEY_EVENT) {
    // don't care about other console events
    return 0;
  }

  if(!record[0].Event.KeyEvent.bKeyDown) {
    // really only care about keydown
    return 0;
  }

  blah = record[0].Event.KeyEvent.uChar.AsciiChar;

  if(blah > 31) {
    return blah;
  }

  blah = record[0].Event.KeyEvent.wVirtualKeyCode;

  switch(blah) {
    case VK_BACK:
    case VK_TAB:
    case VK_ESCAPE:
    case VK_PAUSE:
      return blah;

    case VK_NUMLOCK:
    case VK_SCROLL:
    case VK_LSHIFT:
    case VK_RSHIFT:
    case VK_LCONTROL:
    case VK_RCONTROL:
    case VK_LMENU:
    case VK_RMENU:
    case 0xe3:
    case 0:
      return 0;

    default:
      if(blah > 31) {
        return blah+256;
      }

      return 0;
  }
}

int mygetch() {
  int temp;

  if(gotch) {
    temp = gotch;
    gotch = 0;
    return temp;
  }

  do {
    switch(WaitForSingleObject(inputHandle, 300)) {
      case WAIT_TIMEOUT: { /* timeout */
        if(pollWindowSize()) {
          return KEY_RESIZE;
        }

        if(!smart) {
          temp = ProcessStdin();

          if(temp) {
            if(pollWindowSize()) {
              gotch = temp;
              return KEY_RESIZE;
            }

            return temp;
          }
        }
      } break;

      case WAIT_OBJECT_0: { /* stdin at array index 0 */
        smart = TRUE;

        temp = ProcessStdin();

        if(temp) {
          if(pollWindowSize()) {
            gotch = temp;
            return KEY_RESIZE;
          }

          return temp;
        }
      } break;
    }
  } while(1);
}

int main(char argc, char* argv[]) {
  int temp = 0;

  inputHandle = GetStdHandle(STD_INPUT_HANDLE);
  outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleMode(inputHandle, 0);
  smart = isNotWine();

  do {
    temp = mygetch();

    printf("%d\n", temp);
  } while(temp != 'q');

  return 0;
}
