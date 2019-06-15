#include <ncurses.h>

int
main()
{
    int temp;

    initscr();
    cbreak();
    noecho();
    scrollok(stdscr, TRUE);
    timeout(1);
    keypad(stdscr, TRUE);

    while ((temp = wgetch(stdscr)) != 'g') {
      if(temp != -1) {
        printw("%d\n", temp);
      }
    }
}