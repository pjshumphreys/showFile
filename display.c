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
}

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

int getLine(
    FILE * input,
    int width,
    char ** line,
    int * lineLength,
    char ** lastWord,
    int * lastWordLength
) {

  if(line == NULL) {
    reallocMsg(line, width);
    reallocMsg(lastWord, width<<1); /* Using twice width to
    help prevent heap fragmentation */
  }

  /* add the characters from last word if there are any,
  break very long words */


  /* read characters into the lastword buffer.
  each time a space of newline character is read see if the lastword fits.

  if it does then continue */




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

    fputc('\n', stdout);

  } while(notLastLine);

  free(line);

  fclose(input);

  return 0;
}









