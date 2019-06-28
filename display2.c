#define _XOPEN_SOURCE_EXTENDED

#include <stdio.h>
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

  tcsetattr(STDIN_FILENO, TCSANOW, &newterm);

  return TRUE;
}

void sigwinchHandler(int sig) {
  termResized = TRUE;
}

void exitTermios(void) {
  tcsetattr(STDIN_FILENO, TCSANOW, &oldterm);
  tcflush(STDIN_FILENO, TCIFLUSH);

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

#define SETKEY(n, code, string) sequences[n].value = code; \
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

  if(termResized) {
    termResized = FALSE;

    return KEY_RESIZE;
  }

  do {
    temp = fgetc(stdin);

    if(temp > 31 || temp == 8 || temp == 9 || temp == 10) {
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

#define freeAndZero(p) { free(p); p = 0; }

struct offsets {
  long value;
  struct offsets * previous;
  struct offsets * next;
};

void reallocMsg(void **mem, size_t size) {
  void *temp;

  if(mem != NULL) {
    if(size) {
      temp = realloc(*mem, size);

      if(temp == NULL) {
        fputs("malloc failed", stderr);
        exit(-1);
      }

      *mem = temp;
    }
    else {
      free(*mem);
      *mem = NULL;
    }
  }
  else {
    fputs("invalid realloc", stderr);
    exit(-1);
  }
}

/* append a character into a string with a given length, using realloc */
int strAppend(char c, char **value, int *strSize) {
  char *temp;

  /* validate inputs */
  /* increase value length by 1 character */

  /* update the string pointer */
  /* increment strSize */
  if(strSize != NULL) {
    if(value != NULL) {
      /* continue to use realloc directly here as reallocMsg can call strAppend itself indirectly */
      if((temp = realloc(*value, (*strSize)+1)) != NULL) {
        *value = temp;

        /* store the additional character */
        (*value)[*strSize] = c;
      }
      else {
        return FALSE;
      }
    }

    (*strSize)++;
  }

  return TRUE;
}

int getLine(
    FILE * input,
    int width,
    char ** line,
    int * lineLength,
    char ** lastWord,
    int * lastWordLength
) {
  int gotch = 0;
  int notFirstWordOnLine = 0;

  if(*line == NULL) {
    reallocMsg(line, width + 1);
  }

  *lineLength = 0;

  /* add the characters from last word if there are any,
  break very long words */
  if(*lastWordLength != 0) {
    if((*lastWord)[(*lastWordLength) - 1] == '\n') {
      notFirstWordOnLine = 1;
      (*lastWordLength) = (*lastWordLength) - 1;
    }

    if(*lastWordLength <= width) {
      reallocMsg(line, (*lastWordLength)+1);
      memcpy(*line, *lastWord, *lastWordLength);
      (*line)[*lastWordLength] = '\0';
      *lineLength = *lineLength + *lastWordLength;

      freeAndZero(*lastWord);
      *lastWordLength = 0;

      if(notFirstWordOnLine == 1) {
        return TRUE;
      }

      notFirstWordOnLine = 1;
    }
    else {
      /* memmove the characters down */
      return TRUE;
    }
  }

  /* read characters into the lastword buffer.
  each time a space or newline character is read see if the lastword fits.

  if it does then continue, otherwise return the text */
  do {
    gotch = fgetc(input);

    switch(gotch) {
      case '\x0d': {
        gotch = fgetc(input);

        /* only skip \n after \r */
        if(gotch != '\n') {
          ungetc(gotch, input);
          gotch = '\n';
        }
      } /* fall thru */

      case '\n':

      case EOF:
      case '\x1a': /* CP/M end of file character */

      case ' ': {
        /* if the lastWord fits then add it */
        if((*lineLength + 1 /* for space char */ + *lastWordLength) <= width) {
          reallocMsg(
              line,
              *lineLength + *lastWordLength +
              notFirstWordOnLine /* for space char*/ +
              1 /* for null termination */
            );

          if(notFirstWordOnLine) {
            (*line)[*lineLength] = ' ';
          }

          (*line)[*lineLength + *lastWordLength + notFirstWordOnLine] = '\0';

          if(*lastWordLength) {
            memcpy(
                &((*line)[*lineLength+notFirstWordOnLine]),
                *lastWord,
                *lastWordLength
              );
          }

          *lineLength = *lineLength+ *lastWordLength +
              notFirstWordOnLine;

          notFirstWordOnLine = 1;

          /* set the lastWordLength to 0 */
          freeAndZero(*lastWord);
          *lastWordLength = 0;
        }

        /* if it doesn't fit then return the line */
        else {
          /* if the last word has a newline after it, we'll need to
          add a marker to exit immediatly after it the next time this
          function is called */
          if(gotch == '\n') {
            strAppend(gotch, lastWord, lastWordLength);
          }

          /* there are more lines to come */
          return TRUE;
        }

        if(gotch != ' ') {
          return gotch == '\n';
        }
      } break;

      default: {
        if(gotch < 32) {
          gotch = '?';
        }

        /* Append the character to the new word */
        strAppend(gotch, lastWord, lastWordLength);
      } break;
    }
  } while(1);
}

struct offsets * topLineOffset = NULL;

struct offsets * lastLineOffset = NULL;
char * line = NULL;
int lineLength = 0;
char * lastWord = NULL;
int lastWordLength = 0;

void maintainLineOffsets(
    struct offsets ** lastLineOffset,
    FILE * input,
    int lastWordLength
) {
  struct offsets * temp = NULL;

  long current = ftell(input);

  if(lastWordLength) {
    if(lastWord[lastWordLength-1] == '\n') {
      current -= lastWordLength;
    }
    else {
      current -= lastWordLength + 1;
    }
  }

  if(*lastLineOffset == NULL || (*lastLineOffset)->value < current) {
    reallocMsg(&temp, sizeof(struct offsets));
    temp->value = current;
    temp->previous = *lastLineOffset;
    temp->next = NULL;

    if(*lastLineOffset) {
      (*lastLineOffset)->next = temp;
    }

    *lastLineOffset = temp;
  }
}



const char * navMessage =
" Navigate with arrow keys ('q' to quit)";
const char * navDelete  =
"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
"\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b";


int drawScreen(
    FILE* input,
    int startLine,
    int previousLine,
    int * maxLine,
    int virtualWidth,
    int leftOffset,
    int width,
    int height
) {
  int currentLine = 0;
  int notLastLine = TRUE;
  int i;

  if(startLine == previousLine + 1) {
    printf(navDelete);
    fflush(stdout);

    /* scrolling thru the file normally, just display one line */
    currentLine = height - 2;

    if(topLineOffset && topLineOffset->next) {
      topLineOffset = topLineOffset->next;
    }
  }
  else if(topLineOffset && topLineOffset->previous) {
    topLineOffset = topLineOffset->previous;
    fseek(input, topLineOffset->value, SEEK_SET);
    putp(cursor_home);

    freeAndZero(lastWord);
    lastWordLength = 0;
  }

  for( ; currentLine < height - 1; currentLine++) {
    if(!notLastLine && startLine == 0) {
      /* the file has fewer lines than the screen does.
      Pad with some blank ones */
      for(i = 0; i < width; i++) {
        fputc(' ', stdout);
      }
    }
    else {
      maintainLineOffsets(
          &lastLineOffset,
          input,
          lastWordLength
        );

      notLastLine = getLine(
          input,
          width,
          &line,
          &lineLength,
          &lastWord,
          &lastWordLength
        );

      for(i = 0; i < width; i++) {
        if(i < lineLength) {
          fputc(line[i], stdout);
        }
        else {
          fputc(' ', stdout);
        }
      }
      printf("\n");
    }
  }

  if(topLineOffset == NULL) {
    topLineOffset = lastLineOffset;

    while(topLineOffset->previous) {
      topLineOffset = topLineOffset->previous;
    }
  }

  printf(navMessage);
  fflush(stdout);

  if(notLastLine && startLine == *maxLine) {
    (*maxLine)++;
  }

  return 0;
}

int main(int argc, char * argv[]) {
  FILE * input;

  int width = 80;
  int height = 24;

  int errret = 0;
  int temp = 0;
  int x;
  int y;

  int currentLine = 0;
  int previousLine = 0;
  int maxLine = 0;
  int firstTime = TRUE;

  setlocale(LC_ALL, "en_GB.UTF8");

  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  if(
    isatty(STDOUT_FILENO) &&
    setupterm(0, STDOUT_FILENO, &errret) == OK &&
    setupTermios()
  ) {
    atexit(exitTermios);

    if(setupSignals()) {
      /* populate the lookup table with the codes we want */
      SETKEY(0, KEY_HOME, key_home);
      SETKEY(1, KEY_END, key_end);
      SETKEY(2, KEY_NPAGE, key_npage);
      SETKEY(3, KEY_PPAGE, key_ppage);
      SETKEY(4, KEY_UP, key_up);
      SETKEY(5, KEY_DOWN, key_down);
      SETKEY(6, KEY_LEFT, key_left);
      SETKEY(7, KEY_RIGHT, key_right);

      /* sort the table by length */
      qsort(&sequences, 8, sizeof(struct entry), cmpfunc);

      sequence = malloc(maxLength+1);

      input = fopen("testfile.txt", "rb");

      if(input == NULL) {
        fputs("file couldn't be opened\n", stdout);
        return 0;
      }

      do {
        if(currentLine != previousLine || firstTime) {
          drawScreen(
              input,
              currentLine,
              previousLine,
              &maxLine,
              width,
              0,
              width,
              height
            );

          previousLine = currentLine;

          firstTime = FALSE;
        }

        temp = mygetch();

        switch(temp) {
          case KEY_UP: {
            if(currentLine) {
              currentLine -= 1;
            }
          } break;

          case KEY_DOWN: {
            if(currentLine + 1 <= maxLine) {
              currentLine += 1;
            }
          } break;
        }
      } while(temp != 'q');

      fputc('\n', stdout);

      free(line);

      fclose(input);
    }
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


