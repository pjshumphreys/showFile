#include <conio.h>
#include <stdio.h>
#include <dos.h>
#include <stdlib.h>
#include <string.h>

#define CSI "\033["
#define FALSE 0
#define TRUE !FALSE

#define KEY_UP 328
#define KEY_DOWN 336
#define KEY_LEFT 331
#define KEY_RIGHT 333

int useAnsi = FALSE;

#define cursor_home 0

void moveCursor(int x, int y) {
  union REGS regs;

  if(useAnsi) {
    printf(CSI "%d;%dH", y+1, x+1);
    fflush(stdout);
  }
  else {
    regs.h.ah = 0x02;
    regs.h.bh = 0;
    regs.h.dh = y;
    regs.h.dl = x;
    int86(0x10, &regs, &regs);
  }
}

void scrollDown(int width, int height) {
  union REGS regs;

  regs.h.ah = 0x07;
  regs.h.al = 1;
  regs.h.ch = 0;
  regs.h.cl = 0;
  regs.h.dh = height-1;
  regs.h.dl = width-1;
  regs.h.bh = 7;

  int86(0x10, &regs, &regs);
}

void putp(int code) {
  union REGS regs;

  switch(code) {
    case cursor_home: {
      if(useAnsi) {
        printf(CSI "H");
        fflush(stdout);
      }
      else {
        regs.h.ah = 0x02;
        regs.h.bh = 0;
        regs.h.dh = 0;
        regs.h.dl = 0;
        int86(0x10, &regs, &regs);
      }
    } break;
  }
}

int mygetch() {
  int temp;
  int i = 0;
  int currentItem = 0;
  int retval = 0;

  do {
    temp = getch();

    if(temp) {
      if(temp > 31 || temp == 27 || temp == 8 || temp == 9 || temp == 13) {
        if(temp == 127) {
          temp = 8;
        }

        retval = temp;
      }
    }
    else {
      temp = getch();

      retval =  temp + 256;
    }
  } while(retval == 0);

  while(kbhit()) {
    getch();
  }

  return retval;
}

#define freeAndZero(p) { free(p); p = 0; }

struct offsets {
  long value;
  int number;
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

struct offsets * topLineOffset = NULL;

struct offsets * lastLineOffset = NULL;
char * line = NULL;
int lineLength = 0;
char * lastWord = NULL;
int lastWordLength = 0;

void maintainLineOffsets(
    struct offsets ** lastLineOffset,
    FILE * input,
    int lineNumber,
    int lastWordLength,
    int width
) {
  struct offsets * temp = NULL;

  long current = ftell(input);

  if(lastWordLength) {
    if(lastWordLength <= width && lastWord[lastWordLength-1] == '\n') {
      current -= lastWordLength;
    }
    else {
      current -= lastWordLength + 1;
    }
  }

  if(*lastLineOffset == NULL || (*lastLineOffset)->value < current) {
    reallocMsg((void **)&temp, sizeof(struct offsets));
    temp->value = current;
    temp->previous = *lastLineOffset;
    temp->number = lineNumber;
    temp->next = NULL;

    if(*lastLineOffset) {
      (*lastLineOffset)->next = temp;
    }

    *lastLineOffset = temp;
  }
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
    reallocMsg((void **)line, width + 1);
  }

  *lineLength = 0;

  /* add the characters from last word if there are any,
  break very long words */
  if(*lastWordLength != 0 && (*lastWord)[0] != '\n') {
    if(*lastWordLength <= width) {
      if((*lastWord)[(*lastWordLength) - 1] == '\n') {
        notFirstWordOnLine = 1;
        (*lastWordLength) = (*lastWordLength) - 1;
      }

      reallocMsg((void **)line, (*lastWordLength)+1);
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
      reallocMsg((void **)line, width+1);
      memcpy(*line, *lastWord, width);
      (*line)[width] = '\0';
      *lineLength = width;

      /* memmove the characters down */
      memmove(*lastWord, (*lastWord)+width, *lastWordLength+1-width);
      *lastWordLength -= width;

      if(*lastWordLength && lastLineOffset->next == NULL) {
        maintainLineOffsets(
            &lastLineOffset,
            input,
            lastLineOffset->number+1,
            *lastWordLength,
            width
          );
      }

      return TRUE;
    }
  }
  else {
    /* suppress just a new line character that wrapped
    from the end of a long line */
    freeAndZero(*lastWord);
    *lastWordLength = 0;
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
              (void **)line,
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
          /* if the last word has a newline after it, we need to
          add a marker meaning we should exit immediately after it
          the next time this function is called */
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
        strAppend(gotch, lastWord, lastWordLength);
      } break;
    }
  } while(1);
}



const char * navMessage =
"Navigate with arrow keys ('q' to quit)";
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
  int leftMargin = 0;
  int i;
  int doTparm = FALSE;

  if(startLine == previousLine + 1) {
    printf(navDelete);
    fflush(stdout);

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

      /* if the terminal has the insert line capability then use it */
      if(!useAnsi) {
        scrollDown(width, height-1);
        fflush(stdout);
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
          &lastLineOffset,
          input,
          startLine+currentLine,
          lastWordLength,
          virtualWidth
        );

      notLastLine = getLine(
          input,
          virtualWidth,
          &line,
          &lineLength,
          &lastWord,
          &lastWordLength
        );

      if(!doTparm || currentLine == 0) {
        /*if the first line starts with a very long word print it anyway */
        if(doTparm && lineLength == 0 && lastWordLength > width) {
          getLine(
            input,
            virtualWidth,
            &line,
            &lineLength,
            &lastWord,
            &lastWordLength
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
    fflush(stdout);
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
    moveCursor(0, height-1);
  }

  /* redisplay the navigation message (in bold) */
  fputc(' ', stdout);
  //putp(enter_reverse_mode);
  printf(navMessage);
  //putp(exit_attribute_mode);
  fflush(stdout);
  //putp(clr_eos);

  if(notLastLine && startLine == *maxLine) {
    (*maxLine)++;
  }

  return 0;
}

int parseCursorLocation(int *x, int * y) {
  union REGS regs;
  int temp, i, arr[2];

  /*stuff the keyboard buffer so we can reliably
  call getch the first time without blocking */
  regs.h.ah = 0x05;
  regs.h.ch = 0x1e;
  regs.h.cl = 0x61;
  int86(0x16, &regs, &regs);

  if(
    kbhit()  /* kbhit will return false on computers
    that don't have an IBM extended keyboard */
    && getch() == 27) {
    /* skip [*/
    getch();

    for(i = 0; i != 2; i++) {
      arr[i] = 0;

      do {
        temp = getch();

        if(temp > 47 && temp < 58) { /* quit if not a digit */
          arr[i] = arr[i] * 10 + temp - 48;
        }
        else {
          break;
        }
      } while (1);
    }

    /* consume everthing else on the input buffer */
    while(kbhit() && (getch() != 'a')) {}

    *x = arr[1];
    *y = arr[0];

    return TRUE;
  }

  return FALSE;
}

void noAnsi(int *x, int *y) {
  int page;
  int origy;
  int oldy;
  int tempy;
  union REGS regs;

  /* get the current video page */
  regs.h.ah = 0x0f;
  int86(0x10, &regs, &regs);

  *x = regs.h.ah; /* bios returns the column count directly */
  page = regs.h.bh; /* it also returns the active page number */

  /* get the current cursor position */
  regs.h.ah = 0x03;
  regs.h.bh = page;
  int86(0x10, &regs, &regs);

  oldy = origy = tempy = regs.h.dh;

  do {
    printf("\n");

    /* Read the current x and y. When the y value stops
    increasing, this is how big the screen is */
    regs.h.ah = 0x03;
    regs.h.bh = page;
    int86(0x10, &regs, &regs);

    tempy = regs.h.dh;

  } while(oldy != tempy && (oldy = tempy));

  *y = tempy + 1;

  regs.h.ah = 0x02;
  regs.h.dh = origy-2;
  regs.h.dl = 0;
  int86(0x10, &regs, &regs);
}

int main(int argc, char * argv[]) {
  int origx = 0;
  int origy = 0;
  int temp;

  FILE * input;

  int width = 80;
  int height = 24;

  int leftOffset = 0;

  int virtualWidth = 80;

  int currentLine = 0;
  int previousLine = 0;
  int maxLine = 0;
  int firstTime = TRUE;


  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  /* clear the keyboard buffer */
  while(kbhit()) {
    getch();
  }

  /* test for vt100 capability */
  printf("\n" CSI "6n\b\b\b\b");
  fflush(stdout);

  if(parseCursorLocation(&origx, &origy)) {
    /* The console supports vt100 control codes */

    /* attempt to move the cursor to 255, 255 then get the cursor position
    again. This will get limited to the terminal's actual size*/
    printf(CSI "255;255H" CSI "6n");
    fflush(stdout);

    parseCursorLocation(&width, &height);

    /* reposition the cursor */
    printf(CSI "%d;%dH", origy-1, origx);
    fflush(stdout);

    useAnsi = TRUE;
  }
  else {
    /* The console doesn't support vt100 control codes.
    Use interupt 10h services the control it instead */
    noAnsi(&width, &height);
  }

  input = fopen("testfile.txt", "rb");

  if(input == NULL) {
    fputs("file couldn't be opened\n", stdout);
    return 0;
  }

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
          leftOffset,
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
  } while(temp != 'q');

  free(line);

  fclose(input);

  return 0;
}