#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FALSE 0
#define TRUE 1

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
    if(*lastWordLength <= width) {
      reallocMsg(line, (*lastWordLength)+1);
      memcpy(*line, *lastWord, *lastWordLength);
      (*line)[*lastWordLength] = '\0';
      *lineLength = *lineLength + *lastWordLength;
      notFirstWordOnLine = 1;

      freeAndZero(*lastWord);
      *lastWordLength = 0;
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
            memcpy(&((*line)[*lineLength+notFirstWordOnLine]), *lastWord, *lastWordLength);
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

void maintainLineOffsets(
    struct offsets ** lastLineOffset,
    FILE * input,
    int lastWordLength
) {
  struct offsets * temp = NULL;

  reallocMsg(&temp, sizeof(struct offsets));
  temp->value = ftell(input) - lastWordLength;
  temp->previous = *lastLineOffset;
  temp->next = NULL;

  if(*lastLineOffset) {
    (*lastLineOffset)->next = temp;
  }

  *lastLineOffset = temp;
}

int main(int argc, char * argv[]) {
  //open the file for reading
  FILE * input;

  int width = 80;
  int notLastLine = TRUE;

  char * line = NULL;
  int lineLength = 0;
  char * lastWord = NULL;
  int lastWordLength = 0;

  int i;

  struct offsets * lastLineOffset = NULL;

  input = fopen("testfile.txt", "rb");

  if(input == NULL) {
    fputs("file couldn't be opened\n", stdout);
    return 0;
  }

  do {
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
  } while(notLastLine && fputc('\n', stdout) != EOF);

  free(line);

  fclose(input);

  return 0;
}
