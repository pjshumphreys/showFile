#define _XOPEN_SOURCE_EXTENDED

#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <stdbool.h>
#include <unistd.h>
#include <signal.h>
#include <termios.h>
#include <sys/ioctl.h>

#ifndef __WATCOMC__
  #include <ncursesw/curses.h>
  #include <ncursesw/term.h>
#else
  #define _WCHAR_T
  #define _WINT_T
  #include <curses.h>
  #include <term.h>
#endif

#ifndef TRUE
  #define FALSE 0
  #define TRUE !FALSE
#endif

struct termios oldterm;
volatile int termResized;
int maxLength = 0;
char *sequence = NULL;

struct entry {
  int value;
  int length;
  char* text;
};

struct entry sequences[8];

int setupTermios() {
  struct termios newterm;

  tcgetattr(STDIN_FILENO, &oldterm);
  newterm = oldterm;

  newterm.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(0, TCSANOW, &newterm);

  return TRUE;
}

void sigwinchHandler(int sig) {
  termResized = TRUE;
}

void exitTermios(void) {
  tcsetattr(0, TCSANOW, &oldterm);
  tcflush(0, TCIFLUSH);

  free(sequence);
}

void intHandler(int dummy) {
  exit(0);
}

int setupSignals(void) {
  struct sigaction sa;
  struct sigaction sa2;

  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = sigwinchHandler;

  if(sigaction(SIGWINCH, &sa, NULL) == -1) {
    return FALSE;
  }

  sigemptyset(&sa2.sa_mask);
  sa2.sa_flags = 0;
  sa.sa_handler = intHandler;

  if(sigaction(SIGINT, &sa, NULL) == -1) {
    return FALSE;
  }

  return TRUE;
}

int getWindowSize(int * x, int * y) {
  struct winsize ws;

  if(ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
    return FALSE;
  }

  *y = ws.ws_row;
  *x = ws.ws_col;

  return TRUE;
}

#define QWERTYJFFGH(n, code, string) sequences[n].value = code; \
    sequences[n].text = string; \
    sequences[n].length = strlen(string)

int kbhit(void) {
  int byteswaiting;
  ioctl(0, FIONREAD, &byteswaiting);
  return byteswaiting > 0;
}

/* only returns resize pseudo key, arrow keys, home, end, page-up, page-down or escape */
int mygetch() {
  int temp;
  int i = 0;
  int currentItem = 0;

  do {
    if(termResized) {
      termResized = FALSE;

      return KEY_RESIZE;
    }

    temp = fgetc(stdin);

    if(temp > 31 || temp == 127 || temp == 8 || temp == 9 || temp == 10) {
      if(temp == 127) {
        temp = 8;
      }

      return temp;
    }

    if(!kbhit() && temp == 27) {
      return 27;
    }

    i = 1;
    sequence[0] = temp;
    sequence[1] = '\0';
    currentItem = 0;

    do {
      /* if matched length is the same as the current potential key length */
      while(i == sequences[currentItem].length) {
        /* bodge around incorrectly set terminfo settings */
        if(i > 2) {
          sequence[1] = sequences[currentItem].text[1];
        }

        /* compare what we've captured to the keys defined
        in the terminfo description that we want */
        if(strcmp(sequences[currentItem].text, sequence) == 0) {
          return sequences[currentItem].value;
        }

        /* didn't match. check the next key */
        currentItem++;
      }

      if(!kbhit()) {
        break;
      }

      temp = fgetc(stdin);

      if(i < maxLength) {
        /* add another character to the end and check again */
        sequence[i] = temp;
        sequence[++i] = '\0';
      }
    } while(1);
  } while(1);
}

int cmpfunc(const void * a, const void * b) {
  int temp = ((struct entry *)a)->length -
      ((struct entry *)b)->length;

  if(((struct entry *)a)->length > maxLength) {
    maxLength = ((struct entry *)a)->length;
  }

  if(((struct entry *)b)->length > maxLength) {
    maxLength = ((struct entry *)b)->length;
  }

  return temp;
}

int main(int argc, char * argv[]) {
  int errret = 0;
  int temp;
  int x;
  int y;

  setlocale(LC_ALL, "en_GB.UTF8");

  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  if(
    isatty(STDOUT_FILENO) &&
    setupterm(0, STDOUT_FILENO, &errret) == OK &&
    setupTermios() &&
    setupSignals()
  ) {
    atexit(exitTermios);

    /* populate the lookup table with the codes we want */
    QWERTYJFFGH(0, KEY_HOME, key_home);
    QWERTYJFFGH(1, KEY_END, key_end);
    QWERTYJFFGH(2, KEY_NPAGE, key_npage);
    QWERTYJFFGH(3, KEY_PPAGE, key_ppage);
    QWERTYJFFGH(4, KEY_UP, key_up);
    QWERTYJFFGH(5, KEY_DOWN, key_down);
    QWERTYJFFGH(6, KEY_LEFT, key_left);
    QWERTYJFFGH(7, KEY_RIGHT, key_right);

    /* sort the table by length */
    qsort(&sequences, 8, sizeof(struct entry), cmpfunc);

    sequence = malloc(maxLength+1);

    do {
      temp = mygetch();

      switch(temp) {
        case KEY_RESIZE: {
          getWindowSize(&x, &y);

          printf("window size %d %d\n", x, y);
        } break;

        default: {
          printf("%d\n", temp);
        } break;
      }
    } while(temp != 'q');
  }

  return 0;
}

/* tputs(tparm(cursor_address, 18, 40), 1, my_putchar); * /

#include <stdio.h>
#include <conio.h>

int main(int argc, char * argv[]) {
  int temp;

  while((temp = getch()) != 'q') {
    if(!kbhit() && temp == 27) {
      printf("ESC\n");
    }
    else {
      printf("%d\n", temp);
    }
  }

  return 0;
}
*/