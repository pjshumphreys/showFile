#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define __Z88DK_R2L_CALLING_CONVENTION /* Makes varargs kinda work on Z88DK
as long as the function using them uses the __stdc calling convention */
#include <stdarg.h>

#include <conio.h>
#include <cpm.h>

#pragma output protect8080 /* Abort if the cpu isn't a z80 */

#define MSX_DOSVER 0x6f
#define ESC "\033"
#define CSI "\033["
#define FALSE 0
#define TRUE !FALSE

#define cursor_home 0
#define insert_line 1
#define enter_reverse_mode 2
#define exit_attribute_mode 3

static const char* envName = "ENVIRON";
static const char* envNameNew = "ENVIRON.NEW";
static struct envVar* firstEnv = NULL;
static struct envVar* lastEnv = NULL;

struct envVar {
  char* text;
  struct envVar* next;
};

long heap;

int mode;

#define MODE_DUMB 0
#define MODE_ADM3A 1
#define MODE_VT52 2
#define MODE_VT100 3

#define KEY_UP 256
#define KEY_DOWN 257
#define KEY_LEFT 258
#define KEY_RIGHT 259

#define freeAndZero(p) { free(p); p = 0; }

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
        fprintf(stderr, "malloc failed\n");
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
    fprintf(stderr, "invalid realloc\n");
    exit(EXIT_FAILURE);
  }
}

int d_fgets(char** ws, FILE* stream) {
  char buf[80];
  char* newWs = NULL;
  char* potentialNewWs = NULL;
  size_t totalLength = (size_t)0;
  size_t potentialTotalLength = (size_t)0;
  size_t bufferLength;
  int trail;

  /* check sanity of inputs */
  if(ws == NULL) {
    return FALSE;
  }

  /* try reading some text from the file into the buffer */
  while(fgets((char *)&(buf[0]), 80, stream) != NULL) {
    /* get the length of the string in the buffer */
    bufferLength = strlen((char *)&(buf[0]));

    /*
      add it to the potential new length.
      this might not become the actual new length if the realloc fails
    */
    potentialTotalLength += bufferLength;

    /*
      try reallocating the output string to be a bit longer
      if it fails then set any existing buffer to the return
      value and return true.
    */

    reallocMsg(&newWs, potentialTotalLength+1);

    /* copy the buffer data into potentialNewWs */
    memcpy(newWs+totalLength, &buf, bufferLength);

    /* the potential new string becomes the actual one */
    totalLength = potentialTotalLength;

    /* if the last character is '\n' (ie we've reached
    the end of a line) then return the result */
    if(
        newWs[totalLength-1] == '\n' ||
        newWs[totalLength-1] == '\x0d'
    ) {
      /* ensure null termination of the string */
      newWs[totalLength-1] = '\0';

      /* set the output string pointer and return true */
      if(*ws) {
        free(*ws);
      }

      *ws = newWs;

      return TRUE;
    }
  }

  /* if we've already retrieved some text */
  if(newWs != NULL) {
    /* ensure null termination of the string */
    newWs[totalLength] = '\0';

    /* set the output string point and return true */
    if(*ws) {
      free(*ws);
    }

    *ws = newWs;

    return TRUE;
  }

  /*
    otherwise no successful allocation was made.
    return false without modifying the output string location
  */
  return FALSE;
}

int d_sprintf(void **str, char *format, ...) __stdc {
  size_t newSize;
  char *newStr = NULL;
  va_list args;
  char * temp = NULL;

  temp = *str;

  /* Check sanity of inputs */
  if(str == NULL || format == NULL) {
    return FALSE;
  }

  /* get the space needed for the new string */
  va_start(args, format);
  newSize = (size_t)(vsnprintf(NULL, 0, format, args)); /* plus '\0' */
  va_end(args);

  /* Create a new block of memory with the correct size rather
  than using realloc as any old values could overlap with the
  format string. quit on failure */
  reallocMsg((void**)&newStr, newSize+1);

  /* do the string formatting for real. vsnprintf doesn't seem
  to be available on Lattice C */
  va_start(args, format);
  vsnprintf(newStr, newSize+1, format, args);
  va_end(args);

  /* ensure null termination of the string */
  newStr[newSize] = '\0';

  /* free the old contents of the output if present */
  free(temp);

  /* set the output pointer to the new pointer location */
  *str = newStr;

  /* everything occurred successfully */
  return newSize;
}

/* Z88DK doesn't have getenv or setenv, so we define them here */
char* getenv(char* name) {
  FILE* envFile;
  int gotch;
  char* temp;
  unsigned char found = FALSE;
  struct envVar* tempEnv = NULL;

  if(name == NULL || (envFile = fopen(envName, "rb")) == NULL) {
    return NULL;
  }

  do {
    /* attempt to find the variable we're looking for */
    temp = name;

    do {
      gotch = fgetc(envFile);

      if(gotch == '=') {
        if(*temp == 0) {
          found = TRUE;
        }

        break;
      }

      if(*temp != gotch) {
        if(gotch == '\x0d' || gotch == '\n' || gotch == EOF) {
          break;
        }

        continue;
      }

      temp++;
    } while(!feof(envFile));

    if(found) {
      /* Internally keep track of the memory we allocate so
      the caller of this function doesn't need to. */
      reallocMsg(&tempEnv, sizeof(struct envVar));
      tempEnv->text = NULL;

      if(firstEnv == NULL) {
        firstEnv = lastEnv = tempEnv;
      }
      else {
        lastEnv->next = tempEnv;
        lastEnv = tempEnv;
      }

      d_fgets(&(tempEnv->text), envFile);
      fclose(envFile);

      return tempEnv->text;
    }
    else while(gotch != '\x0d' && gotch != '\n' && gotch != EOF) {
      gotch = fgetc(envFile);
    }

  } while(!feof(envFile));

  fclose(envFile);
  return NULL;
}

int setenv(char *name, char *value, int overwrite) {
  FILE* envFile = NULL;
  FILE* envFileNew = NULL;
  char* temp = NULL;
  char* temp2;
  int len;
  unsigned char found = FALSE;

  int start = 0;
  int end = 0;

  if(name == NULL) {
    errno = EINVAL;
    return -1;
  }

  if(*name == '\0') {
    errno = EINVAL;
    return -1;
  }

  if(strchr(name, '=') != NULL) {
    errno = EINVAL;
    return -1;
  }

  if((envFile = fopen(envName, "rb")) != NULL) {
    len = strlen(name);
    found = TRUE;

    if((envFileNew = fopen(envNameNew, "wb")) == NULL) {
      fclose(envFile);
      errno = ENOMEM;
      return -1;
    }

    do {
      d_fgets(&temp, envFile);

      /* Discard the trailing characters from the CP/M file */
      if((temp2 = strchr(temp, '\x1a')) != NULL) {
        *temp2 = '\0';
      }

      /* Skip blank lines */
      if(temp == NULL) {
        continue;
      }

      /* Skip blank lines */
      if(temp[0] == 0) {
        continue;
      }

      if(strncmp(temp, name, len) == 0 && temp[len] == '=') {
        if(!overwrite) {
          /* The env var has already been set and we shouldn't
          overwrite it. return ENOMEM */
          fclose(envFile);
          fclose(envFileNew);
          remove(envFileNew);
          errno = ENOMEM;

          return -1;
        }
      }
      else {
        fprintf(envFileNew, "%s\x0d\n", temp);
      }
    } while(!feof(envFile));

    freeAndZero(temp);

    fclose(envFile);
  }
  else if((envFileNew = fopen(envName, "wb")) == NULL) {
    errno = ENOMEM;
    return -1;
  }

  /*add the new value to the file */
  fprintf(envFileNew, "%s=%s\x0d\n", name, value);

  fclose(envFileNew);

  if(found) {
    remove(envName);
    rename(envNameNew, envName);
  }

  return 0;
}

/* skip any non digits characters */
int myatoi(char* str) {
  while(*str < 48 || *str > 57) {
    if(*str == 0) {
      break;
    }

    str++;
  }

  return atoi(str);
}

long offset = 0;

int myfgetc(FILE * stream) {
  offset++;

  return fgetc(stream);
}

int myungetc(int character, FILE * stream) {
  offset--;

  return ungetc(character, stream);
}

int myfseek(FILE * stream, long int off, int origin) {
  offset = off;

  return fseek(stream, off, origin);
}

long int myftell(FILE * stream) {
  return offset;
}

int getch2() {
  int temp;

  do {
    temp = bdos(CPM_DCIO, 0xff);
  } while(temp == 0);

  return temp;
}

int mygetch() {
  int temp;
  int first;
  int inEscape = FALSE;

  do {
    temp = getch2();

    if(!inEscape && temp > 31) {
      return temp;
    }

    switch(temp) {
      case 30: {
        if(mode == MODE_ADM3A) {
          return KEY_DOWN;
        }
      } /* fall thru */

      case 11:
      case 'A':
        return KEY_UP;

      case 31: {
        if(mode == MODE_ADM3A) {
          return KEY_UP;
        }
      } /* fall thru */

      case 10:
      case 'B':
        return KEY_DOWN;

      case 6:
      case 12:
      case 28:
      case 'C':
        return KEY_RIGHT;

      case 1:
      case 8:
      case 29:
      case 'D':
        return KEY_LEFT;

      case 27: {
        inEscape = TRUE;
        first = TRUE;
      } break;

      case '0':
      case '1':
      case '2':
      case '3':
      case '4':
      case '5':
      case '6':
      case '7':
      case '8':
      case '9':
      case '?':
      case ';': {
        first = FALSE;
      } break;

      case 'O':
      case '[': {
        if(first) {
          first = FALSE;
          break;
        }
      } /* fall thru */

      default:
        if(!inEscape) {
          return temp;
        }

        inEscape = FALSE;
      break;
    }
  } while(1);
}

void moveCursor(int x, int y) {
  switch(mode) {
    case MODE_VT52: {
      printf(ESC "Y%c%c", (unsigned char)(y+32), (unsigned char)(x+32));
      fflush(stdout);
    } break;

    case MODE_VT100: {
      printf(CSI "%d;%dH", y+1, x+1);
      fflush(stdout);
    } break;
  }
}

void putp(int code) {
  switch(code) {
    case insert_line: {
      switch(mode) {
        case MODE_VT52: {
          printf(ESC "L");
          fflush(stdout);
        } break;

        case MODE_VT100: {
          printf(CSI "T");
          fflush(stdout);
        } break;
      }
    } break;

    case cursor_home: {
      moveCursor(0, 0);
    } break;

    case enter_reverse_mode: {
    } break;

    case exit_attribute_mode: {
    } break;
  }
}

struct offsets {
  long value;
  int number;
  struct offsets * previous;
  struct offsets * next;
};

struct offsets * topLineOffset = NULL;
struct offsets * lastLineOffset = NULL;
char * line = NULL;
int lineLength = 0;
char * lastWord = NULL;
int lastWordLength = 0;

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

void maintainLineOffsets(
    struct offsets ** lastLineOffset,
    FILE * input,
    int lineNumber,
    int lastWordLength,
    int width
) {
  struct offsets * temp = NULL;

  int current = myftell(input);

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
    gotch = myfgetc(input);

    switch(gotch) {
      case '\x0d': {
        gotch = myfgetc(input);

        /* only skip \n after \r */
        if(gotch != '\n') {
          myungetc(gotch, input);
          gotch = '\n';
        }
      } /* fall thru */

      case '\n':

      case EOF:
      case '\0':
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
          while((gotch = myfgetc(input)) > 126 && gotch < 192) {}
          myungetc(gotch, input);
          gotch = '?';
        }

        /* Append the character to the new word */
        strAppend(gotch, lastWord, lastWordLength);
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
    putchar('\r');
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
      myfseek(input, topLineOffset->value, SEEK_SET);

      if(mode == MODE_ADM3A) {
        for(i = 0, j = width*height; i < j; i++) {
          putchar('\b');
        }

        printf("\r");
      }
      else {
        /* if the terminal has the insert line capability then use it */

        /* clear the last but 1 line. This will become the bottom
        line after we insert a new line. We'll print the 'move with arrow
        keys' message over the top of this */
        moveCursor(0, height-2);
        for(i = 0; i < width; i++) {
          putchar(' ');
        }

        putp(cursor_home);
        putp(insert_line);
        putp(cursor_home);

        doTparm = TRUE;
      }

      freeAndZero(lastWord);
      lastWordLength = 0;
    }
    else {
      /* The screen was resized. just redraw it in the new size */
      myfseek(input, topLineOffset->value, SEEK_SET);

      if(mode == MODE_ADM3A) {
        for(i = 0, j = width*height; i < j; i++) {
          putchar('\b');
        }

        printf("\r");
      }
      else {
        putp(cursor_home);
      }

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
  /* putp(enter_reverse_mode); */
  printf("Move with arrow keys.q exits");
  /* putp(exit_attribute_mode); */
  fflush(stdout);

  if(notLastLine && startLine == *maxLine) {
    (*maxLine)++;
  }

  return 0;
}

int parseCursorLocation(int *x, int * y) {
  int temp = 0, i, arr[2];

  if(getch2() == 27) {
    /* skip [*/
    getch2();

    for(i = 0; i != 2; i++) {
      arr[i] = 0;

      do {
        temp = getch2();

        if(temp > 47 && temp < 58) { /* quit if not a digit */
          arr[i] = arr[i] * 10 + temp - 48;
        }
        else {
          break;
        }
      } while (1);
    }

    /* Consume everything else on the input buffer */
    while(bdos(CPM_DCIO, 0xff) != 0) {}

    *x = arr[1];
    *y = arr[0];

    return TRUE;
  }

  return FALSE;
}

int main(int argc, char * argv[]) {
  FILE * input;

  int width = 0;
  int height = 0;

  int leftOffset = 0;

  int temp = 0;

  int virtualWidth = 80;

  int currentLine = 0;
  int previousLine = 0;
  int maxLine = 0;
  int firstTime = TRUE;

  int origx = 0;
  int origy = 0;
  int x = 0;
  int y = 0;
  char * columns;
  char * lines;
  const char * cenv = "COLUMNS";
  const char * lenv = "LINES";
  char * temp2 = NULL;
  int notLastLine = TRUE;
  int i;

  mallinit();
  sbrk(40000, 5000);

  mode = MODE_ADM3A;

  /* test for preexisting env vars */
  /* try to get the COLUMNS and LINES environment variables. */
  columns = getenv(cenv);
  lines = getenv(lenv);

  if(columns != NULL && lines != NULL) {
    x = myatoi(columns);
    y = myatoi(lines);
  }

  /* Test if the computer is an MSX */
  else if(
    bdos(CPM_VERS, 0) == 0x22 && /* MSX computers return 2.2 as their CP/M version */
    bdos(MSX_DOSVER, 0) == 0 && /* only MSX computers return 0 for this call */
    /* MSX computers store the console size directly */
    (x = *((unsigned char *)0xF3B0)) != 0 &&
    (y = *((unsigned char *)0xF3B1)) != 0
  ) {
    /* all MSX computers simulate a vt52 */
    mode = MODE_VT52;
  }
  else {
    /* Consume any waiting keypresses there may be */
    while(kbhit()) {
      getch2();
    }

    /* Test if the computer is hooked up to an ANSI terminal */
    printf("\n" CSI "6n\b\b\b");
    fflush(stdout);

    if(parseCursorLocation(&origx, &origy)) {
      /* The console supports vt100 control codes */

      /* attempt to move the cursor to 255, 255 then get the cursor position
      again. This will get limited to the terminal's actual size */
      printf(CSI "255;255H" CSI "6n");
      fflush(stdout);

      parseCursorLocation(&x, &y);

      /* reposition the cursor */
      printf(CSI "%d;%dH", origy-1, origx);
      fflush(stdout);

      mode = MODE_VT100;
    }
    else {
      /* Test to see if the terminal is a VT52 */
      while(kbhit()) {
        getch2();
      }

      printf(ESC "Z\b");
      fflush(stdout);

      if(
          getch2() == '\033' &&
          getch2() == '/'
      ) {
        switch(getch2()) {
          case 'A':
          case 'H':
          case 'J':
            /* VT50 can't position the cursor */
            mode = MODE_DUMB;
          break;

          default: {
            width = 80;
            height = 24;
            mode = MODE_VT52;
          }
        }
      }

      while(kbhit()) {
        getch2();
      }

      columns = NULL;
      lines = NULL;

      /* If they don't exist, ask the user for them directly,
      then try to store them for next time with setenv */
      printf("SCREEN COLUMNS (80)?\n");
      d_fgets(&columns, stdin);

      printf("SCREEN ROWS (0)?\n");
      d_fgets(&lines, stdin);

      x = myatoi(columns);
      y = myatoi(lines);

      if(x < 1) {
        x = 80; /* default value */
      }

      d_sprintf(&columns, "%d", x);
      d_sprintf(&lines, "%d", y);

      printf("Updating ENVIRON file\n");

      /*
        Try to store the values we got back into the environment.
        We don't care if this works or not though as it's just a
        convenience for the user.
      */
      setenv(cenv, columns, TRUE);
      setenv(lenv, lines, TRUE);

      /* Remember to clean up the memory we allocated */
      freeAndZero(columns);
      freeAndZero(lines);
    }
  }

  /* Use dumb terminal mode if y == 0 */
  if(y == 0) {
    mode = MODE_DUMB;
  }

  width = x;
  height = y;

  input = fopen("TESTFILE.TXT", "rb");

  if(input == NULL) {
    fputs("file couldn't be opened\n", stdout);
    return 0;
  }

  //setvbuf(input, NULL, _IOFBF , 512);

  if(mode == MODE_DUMB) {
    printf("Press 'q' to quit or SPACE to continue\n");

    do {
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
    } while(temp != 'q' && temp != 'Q');
  }

  free(line);

  fclose(input);

  putchar('\r');

  return EXIT_SUCCESS;
}