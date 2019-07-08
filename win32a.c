#include <windows.h>
#include <stdio.h>
#include <conio.h>

#define CSI "\033["
#ifndef TRUE
  #define FALSE 0
  #define TRUE !FALSE
#endif

#define KEY_UP 294
#define KEY_DOWN 296
#define KEY_LEFT 293
#define KEY_RIGHT 295
#define KEY_RESIZE 800
int width = 0;
int height = 0;
int gotch = 0;
int smart;

#define cursor_home 0
#define insert_line 1
#define enter_reverse_mode 2
#define exit_attribute_mode 3
#define clr_eos 4
#define clr_eol 5

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
  COORD home;
  DWORD nc, ncw;
  int retval = FALSE;

  GetConsoleScreenBufferInfo(outputHandle, &csbi);
  columns = csbi.srWindow.Right - csbi.srWindow.Left + 1;
  rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

  if(width != 0) {
    retval = TRUE;
  }

  if(width == 0 || columns != width || rows != height) {
    width = columns;
    height = rows;

    nc = csbi.dwSize.X * csbi.dwSize.Y;

    home.X = columns+1;
    home.Y = rows+1;

    SetConsoleScreenBufferSize(
     outputHandle,
     home
    );

    home.X = home.Y = 0;

    FillConsoleOutputAttribute(outputHandle, csbi.wAttributes, nc, home, &ncw);
    FillConsoleOutputCharacter(outputHandle, ' ', nc, home, &ncw);

    return retval;
  }

  return FALSE;
}

int getWindowSize(int * x, int * y) {
  if(width == 0) {
    pollWindowSize();
  }

  *y = height;
  *x = width;

  return TRUE;
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

void moveCursor(int x, int y) {
  COORD cursorPosition;

  cursorPosition.X = x;
  cursorPosition.Y = y;

  SetConsoleCursorPosition(outputHandle, cursorPosition);
}

void scrollDown(int width, int height) {
  SMALL_RECT srctScrollRect, srctClipRect;
  CHAR_INFO chiFill;
  COORD coordDest;

  srctScrollRect.Top = 1;
  srctScrollRect.Bottom = 2;
  srctScrollRect.Left = 0;
  srctScrollRect.Right = width-1;

  chiFill.Attributes = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
  chiFill.Char.AsciiChar = ' ';

  coordDest.X = 0;
  coordDest.Y = 1;

  srctClipRect = srctScrollRect;

  ScrollConsoleScreenBuffer(
      outputHandle,         // screen buffer handle
      &srctScrollRect, // scrolling rectangle
      NULL,   // clipping rectangle
      coordDest,       // top left destination cell
      &chiFill
    );
}

void putp(int code) {
  CONSOLE_SCREEN_BUFFER_INFO csbi;
  COORD home;
  DWORD nc, ncw;

  switch(code) {
    case insert_line: {
      GetConsoleScreenBufferInfo(outputHandle, &csbi);
      home.X = 0;
      home.Y = 0;
      FillConsoleOutputCharacter(outputHandle, ' ', csbi.dwSize.X, home, &ncw);
      printf("\n");
      fflush(stdout);
    } break;

    case cursor_home: {
      moveCursor(0, 1);
    } break;

    case enter_reverse_mode: {
      printf(CSI "p");
      fflush(stdout);
    } break;

    case exit_attribute_mode: {
      printf(CSI "q");
      fflush(stdout);
    } break;

    case clr_eos:{
      printf(CSI "J");
      fflush(stdout);
    } break;

    case clr_eol: {
      GetConsoleScreenBufferInfo(outputHandle, &csbi);
      nc = csbi.dwSize.X - csbi.dwCursorPosition.X;
      FillConsoleOutputCharacter(outputHandle, ' ', nc, csbi.dwCursorPosition, &ncw);
      fflush(stdout);
    } break;
  }
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
    reallocMsg((void**)line, width + 1);
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

      reallocMsg((void**)line, (*lastWordLength)+1);
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
      reallocMsg((void**)line, width+1);
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
              (void**)line,
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
  int i,j;
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
      putp(cursor_home);

      /* if the terminal has the insert line capability then use it */
      putp(insert_line);
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
printf("\n", stdout);
        //
      }
    }
  }

  fflush(stdout);


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
  putp(clr_eol);

  if(notLastLine && startLine == *maxLine) {
    (*maxLine)++;
  }

  return 0;
}

int main(char argc, char* argv[]) {
  FILE * input;

  int width = 80;
  int height = 24;

  int leftOffset = 0;

  int errret = 0;
  int temp = 0;
  int x;
  int y;

  int virtualWidth = 80;

  int currentLine = 0;
  int previousLine = 0;
  int maxLine = 0;
  int firstTime = TRUE;


  inputHandle = GetStdHandle(STD_INPUT_HANDLE);
  outputHandle = GetStdHandle(STD_OUTPUT_HANDLE);
  SetConsoleMode(inputHandle, 0);
  smart = isNotWine();

  input = fopen("testfile.txt", "rb");

  if(input == NULL) {
    fputs("file couldn't be opened\n", stdout);
    return 0;
  }

  getWindowSize(&width, &height);

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
      case KEY_RESIZE: {
        leftOffset = 0;
        firstTime = TRUE; /* redraw the whole screen */
        getWindowSize(&width, &height);

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
  } while(temp != 'q');

  free(line);

  fclose(input);

  return 0;
}
