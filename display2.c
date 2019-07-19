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


#define freeAndZero(p) { free(p); p = 0; }

struct termios oldTerm;
volatile int termResized;
int maxLength = 0;
char *sequence = NULL;

struct entry {
  int value;
  int length;
  char* text;
};

struct entry sequences[8];

struct offsets {
  long value;
  int number;
  struct offsets * previous;
  struct offsets * next;
};

struct offsets * topLineOffset = NULL;
struct offsets * lastLineOffset = NULL;
int width = 0;
int height = 0;
char * line = NULL;
int lineLength = 0;
char * lastWord = NULL;
int lastWordLength = 0;

void reallocMsg(void **mem, size_t size) {
  void *temp;

  temp = *mem;

  if(mem != NULL) {
    if(size) {
      if(temp == NULL) {
        temp = malloc(size);
      }
      else {
        temp = realloc(temp, size);
      }

      if(temp == NULL) {
        fputs("malloc failed\n", stderr);
        exit(EXIT_FAILURE);
      }

      *mem = temp;
    }
    else {
      free(*mem);
      *mem = NULL;
    }
  }
  else {
    fputs("invalid realloc\n", stderr);
    exit(EXIT_FAILURE);
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
      /* continue to use realloc directly here as reallocMsg can
      call strAppend itself indirectly */
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

int setupTermios() {
  struct termios newTerm;

  tcgetattr(STDIN_FILENO, &oldTerm);
  newTerm = oldTerm;

  newTerm.c_lflag &= ~(ICANON | ECHO);

  tcsetattr(STDIN_FILENO, TCSANOW, &newTerm);

  return TRUE;
}

void sigwinchHandler(int sig) {
  termResized = TRUE;
}

void exitTermios(void) {
  tcsetattr(STDIN_FILENO, TCSANOW, &oldTerm);
  tcflush(STDIN_FILENO, TCIFLUSH);

  free(sequence);

  fputc('\n', stdout);
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

#define SETKEY(n, code, string) sequences[n].value = code; \
    sequences[n].text = string; \
    sequences[n].length = strlen(string)

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

int kbhit(void) {
  int byteswaiting;
  ioctl(0, FIONREAD, &byteswaiting);
  return byteswaiting > 0;
}

void moveCursor(int x, int y) {
  tputs(tparm(cursor_address, y, x), 1, putchar);
}

int getWindowSize() {
  struct winsize ws;

  if(ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) == -1) {
    return FALSE;
  }

  width = ws.ws_col;
  height = ws.ws_row;

  return TRUE;
}

int mygetch() {
  int temp;
  int i = 0;
  int currentItem = 0;

  do {
    while(!kbhit()) {
      if(termResized) {
        termResized = FALSE;

        return KEY_RESIZE;
      }
    }

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
          /* consume the rest of the input buffer */
          while(kbhit()) {
            fgetc(stdin);
          }

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

void maintainLineOffsets(
    FILE * input,
    int lineNumber,
    int virtualWidth
) {
  struct offsets * temp = NULL;

  long current = ftell(input);

  if(lastWordLength) {
    if(lastWordLength <= virtualWidth && lastWord[lastWordLength-1] == '\n') {
      current -= lastWordLength;
    }
    else {
      current -= lastWordLength + 1;
    }
  }

  if(lastLineOffset == NULL || lastLineOffset->value < current) {
    reallocMsg((void **)&temp, sizeof(struct offsets));
    temp->value = current;
    temp->previous = lastLineOffset;
    temp->number = lineNumber;
    temp->next = NULL;

    if(lastLineOffset) {
      lastLineOffset->next = temp;
    }

    lastLineOffset = temp;
  }
}

int getLine(
    FILE * input,
    int virtualWidth
) {
  int gotch = 0;
  int notFirstWordOnLine = 0;

  if(line == NULL) {
    reallocMsg((void**)&line, virtualWidth + 1);
  }

  lineLength = 0;

  /* add the characters from last word if there are any,
  break very long words */
  if(lastWordLength != 0 && lastWord[0] != '\n') {
    if(lastWordLength <= virtualWidth) {
      if(lastWord[lastWordLength - 1] == '\n') {
        notFirstWordOnLine = 1;
        lastWordLength = lastWordLength - 1;
      }

      reallocMsg((void**)&line, lastWordLength+1);
      memcpy(line, lastWord, lastWordLength);
      line[lastWordLength] = '\0';
      lineLength = lineLength + lastWordLength;

      freeAndZero(lastWord);
      lastWordLength = 0;

      if(notFirstWordOnLine == 1) {
        return TRUE;
      }

      notFirstWordOnLine = 1;
    }
    else {
      reallocMsg((void**)&line, virtualWidth+1);
      memcpy(line, lastWord, virtualWidth);
      line[virtualWidth] = '\0';
      lineLength = virtualWidth;

      /* memmove the characters down */
      memmove(lastWord, lastWord+virtualWidth, lastWordLength+1-virtualWidth);
      lastWordLength -= virtualWidth;

      if(lastWordLength && lastLineOffset->next == NULL) {
        maintainLineOffsets(
            input,
            lastLineOffset->number+1,
            virtualWidth
          );
      }

      return TRUE;
    }
  }
  else {
    /* suppress just a new line character that wrapped
    from the end of a long line */
    freeAndZero(lastWord);
    lastWordLength = 0;
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
      case '\0':
      case '\x1a': /* CP/M end of file character */

      case ' ': {
        /* if the lastWord fits then add it */
        if((lineLength + 1 /* for space char */ + lastWordLength) <= virtualWidth) {
          reallocMsg(
              (void**)&line,
              lineLength + lastWordLength +
              notFirstWordOnLine /* for space char*/ +
              1 /* for null termination */
            );

          if(notFirstWordOnLine) {
            line[lineLength] = ' ';
          }

          line[lineLength + lastWordLength + notFirstWordOnLine] = '\0';

          if(lastWordLength) {
            memcpy(
                &(line[lineLength+notFirstWordOnLine]),
                lastWord,
                lastWordLength
              );
          }

          lineLength = lineLength + lastWordLength +
              notFirstWordOnLine;

          notFirstWordOnLine = 1;

          /* set the lastWordLength to 0 */
          freeAndZero(lastWord);
          lastWordLength = 0;
        }

        /* if it doesn't fit then return the line */
        else {
          /* if the last word has a newline after it, we need to
          add a marker meaning we should exit immediately after it
          the next time this function is called */
          if(gotch == '\n') {
            strAppend(gotch, &lastWord, &lastWordLength);
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

        /* Displaying utf-8 properly is hard (would probably involve converting
        the data type of our internal string variables to char32_t) and isn't
        necessary for us right now, so we don't bother. If any utf-8 does occur,
        just output ? instead */
        if(gotch > 126) {
          while((gotch = fgetc(input)) > 126 && gotch < 192) {}
          ungetc(gotch, input);
          gotch = '?';
        }

        /* Append the character to the new word */
        strAppend(gotch, &lastWord, &lastWordLength);
      } break;
    }
  } while(1);
}

int drawScreen(
    FILE* input,
    int startLine,
    int previousLine,
    int * maxLine,
    int virtualWidth,
    int leftOffset
) {
  int currentLine = 0;
  int notLastLine = TRUE;
  int leftMargin = 0;
  int i;
  int doTparm = FALSE;

  if(startLine == previousLine + 1) {
    putchar('\r');

    /* scrolling thru the file normally, just display one line */
    currentLine = height - 2;

    if(topLineOffset && topLineOffset->next) {
      topLineOffset = topLineOffset->next;
    }
  }
  else if(topLineOffset) {
    if(startLine != previousLine && topLineOffset->previous) {
      /* scroll up */
      topLineOffset = topLineOffset->previous;
      fseek(input, topLineOffset->value, SEEK_SET);

      {
        /* if the terminal has the insert line capability then use it */

        /* clear the last but 1 line. This will become the bottom
        line after we insert a new line. We'll print the 'move with arrow
        keys' message over the top of this */
        moveCursor(0, height - 2);

        for(i = 0; i < width; i++) {
          putchar(' ');
        }

        putp(cursor_home);
        putp(insert_line);

        doTparm = TRUE;
      }

      putp(cursor_home);

      freeAndZero(lastWord);
      lastWordLength = 0;
    }
    else {
      /* The screen was resized. just redraw it in the new size */
      fseek(input, topLineOffset->value, SEEK_SET);
      putp(cursor_home);

      freeAndZero(lastWord);
      lastWordLength = 0;
    }
  }

  /* The terminal screen is wider than the virtual width.
  Add a left margin to make the text centered */
  if(virtualWidth < width) {
    leftMargin = ((width - virtualWidth) >> 1) - 1;

    if(leftMargin < 0) {
      leftMargin = 0;
    }
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
          input,
          startLine+currentLine,
          virtualWidth
        );

      notLastLine = getLine(
          input,
          virtualWidth
        );

      if(!doTparm || currentLine == 0) {
        /*if the first line starts with a very long word print it anyway */
        if(doTparm && lineLength == 0 && lastWordLength > width) {
          getLine(
            input,
            virtualWidth
          );

          if(lastWordLength) {
            currentLine--;
          }
        }

        for(i = 0; i < width + leftOffset - leftMargin; i++) {
          if(i == 0 && leftMargin) {
            for(; i < leftMargin; i++) {
              fputc(' ', stdout);
            }

            i = 0;
          }

          if(i >= leftOffset) {
            if(i < lineLength) {
              fputc(line[i], stdout);
            }
            else {
              fputc(' ', stdout);
            }
          }
        }
      }
    }
  }

  /* if the topLineOffset has never been set then set it */
  if(topLineOffset == NULL) {
    topLineOffset = lastLineOffset;

    while(topLineOffset->previous) {
      topLineOffset = topLineOffset->previous;
    }
  }

  /* Did we just only repaint the top line(s) of the screen?
  If so, just skip back to the bottom line */
  if(doTparm) {
    moveCursor(0, height - 1);
  }

  /* redisplay the navigation message (in bold) */
  /* putp(enter_reverse_mode); */
  printf("Move with arrow keys.q exits");
  /* putp(exit_attribute_mode); */
  fflush(stdout);

  if(notLastLine && startLine == *maxLine) {
    (*maxLine)++;
  }

  return 0;
}

int main(int argc, char * argv[]) {
  FILE * input;

  int leftOffset = 0;

  int temp = 0;

  int virtualWidth = 80;

  int currentLine = 0;
  int previousLine = 0;
  int maxLine = 0;
  int firstTime = TRUE;

  int notLastLine = TRUE;
  int i, y;

  int errret = 0;

  setlocale(LC_ALL, "en_GB.UTF8");
  setvbuf(stdin, NULL, _IONBF, 0);

  if(
    !isatty(STDOUT_FILENO) ||
    setupterm(0, STDOUT_FILENO, &errret) != OK ||
    !setupTermios()
  ) {
    return EXIT_SUCCESS;
  }

  atexit(exitTermios);

  if(!setupSignals()) {
    return EXIT_SUCCESS;
  }

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

  getWindowSize();

  /* Use dumb terminal mode if height == 0 */
  if(height == 0) {
    printf("Press 'q' to quit or SPACE to continue\n");

    do {
      notLastLine = getLine(
          input,
          width
        );

      for(i = 0; i < width; i++) {
        if(i < lineLength) {
          fputc(line[i], stdout);
        }
        else {
          fputc(' ', stdout);
        }
      }

      temp = mygetch();
    } while (temp != 'q' && temp != 'Q' && notLastLine);
  }
  else {
    if(width < 80) {
      virtualWidth = width;
    }

    do {
      if(currentLine != previousLine || firstTime) {
        drawScreen(
            input,
            currentLine,
            previousLine,
            &maxLine,
            virtualWidth,
            leftOffset
          );

        previousLine = currentLine;

        firstTime = FALSE;
      }

      temp = mygetch();

      switch(temp) {
        case KEY_RESIZE: {
          leftOffset = 0;
          firstTime = TRUE; /* redraw the whole screen */
          getWindowSize();

          /* recalculate the max line */
          for(y = height - 1; y > 1; y--) {
            if(lastLineOffset->previous) {
              lastLineOffset = lastLineOffset->previous;
            }
          }

          maxLine = lastLineOffset->number;

          while(currentLine > maxLine) {
            topLineOffset = topLineOffset->previous;
            currentLine--;
          }

          previousLine = currentLine;

          while(lastLineOffset->next) {
            lastLineOffset = lastLineOffset->next;
          }
        } break;

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

        case KEY_LEFT: {
          if(leftOffset) {
            leftOffset--;
            firstTime = TRUE; /* redraw the whole screen */
          }
        } break;

        case KEY_RIGHT: {
          if(width < virtualWidth && leftOffset < virtualWidth - width) {
            leftOffset++;
            firstTime = TRUE; /* redraw the whole screen */
          }
        } break;
      }
    } while(temp != 'q' && temp != 'Q');
  }

  free(line);

  fclose(input);

  putchar('\n');

  return EXIT_SUCCESS;
}
