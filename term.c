#define _XOPEN_SOURCE_EXTENDED

#include <locale.h>
#include <stdbool.h>
#include <unistd.h>

#ifndef __WATCOMC__
  #include <ncursesw/curses.h>
  #include <ncursesw/term.h>
#else
  #define _WCHAR_T
  #define _WINT_T
  #include <curses.h>
  #include <term.h>
#endif

int main(int argc, char * argv[]) {
  int errret = 0;

  setlocale(LC_ALL, "en_GB.UTF8");

  if(
    isatty(STDOUT_FILENO) &&
    setupterm(0, STDOUT_FILENO, &errret) == OK
  ) {
    putp(cursor_home);
  }

  return 0;
}