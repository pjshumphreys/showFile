#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define __Z88DK_R2L_CALLING_CONVENTION /* Makes varargs kinda work on Z88DK as long as the function using them uses the __stdc calling convention */
#include <stdarg.h>

#include <conio.h>
#include <cpm.h>

#pragma output protect8080 /* Abort if the cpu isn't a z80 */

#define MSX_DOSVER 0x6f
#define CSI "\033["
#define FALSE 0
#define TRUE !FALSE

static const char* envName = "ENVIRON";
static const char* envNameNew = "ENVIRON.NEW";
static struct envVar* firstEnv = NULL;
static struct envVar* lastEnv = NULL;

struct envVar {
  char* text;
  struct envVar* next;
};

long heap;

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
      if it fails then set any existing buffer to the return value and return true
    */

    reallocMsg(&newWs, potentialTotalLength+1);

    /* copy the buffer data into potentialNewWs */
    memcpy(newWs+totalLength, &buf, bufferLength);

    /* the potential new string becomes the actual one */
    totalLength = potentialTotalLength;

    /* if the last character is '\n' (ie we've reached the end of a line) then return the result */
    if(newWs[totalLength-1] == '\n' || newWs[totalLength-1] == '\x0d') {
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

  /* Create a new block of memory with the correct size rather than using realloc */
  /* as any old values could overlap with the format string. quit on failure */
  reallocMsg((void**)&newStr, newSize+1);

  /* do the string formatting for real. vsnprintf doesn't seem to be available on Lattice C */
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
      if(strlen(temp) == 0) {
        continue;
      }

      if(strncmp(temp, name, len) == 0 && temp[len] == '=') {
        if(!overwrite) {
          /* The env var has already been set and we shouldn't overwrite it. return ENOMEM */
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

int parseCursorLocation(int *x, int * y) {
  int temp, i, arr[2];

  if(kbhit() && getch() == 27) {
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

    /* Consume everthing else on the input buffer */
    while(kbhit()) {
      getch();
    }

    *x = arr[1];
    *y = arr[0];

    return TRUE;
  }

  return FALSE;
}

int main(int argc, char * argv[]) {
  int origx = 0;
  int origy = 0;
  int x = 0;
  int y = 0;
  char * columns;
  char * lines;
  const char * cenv = "COLUMNS";
  const char * lenv = "LINES";
  char * temp = NULL;

  mallinit();
  sbrk(30000, 1000);

  /* Test if the computer is an MSX */
  if(
    bdos(CPM_VERS, 0) == 0x22 && /* MSX computers return 2.2 as their CP/M version */
    bdos(MSX_DOSVER, 0) == 0 /* only MSX computers return 0 for this call */
  ) {
    /* MSX computers store the console size directly */
    x = *((unsigned char *)0xF3B0);
    y = *((unsigned char *)0xF3B1);
  }
  else {
    /* Consume any waiting keypresses there may be */
    while(kbhit()) {
      getch();
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
    }
    else {
      while(kbhit()) {
        getch();
      }

      /* try to get the COLUMNS and LINES environment variables. */
      columns = getenv(cenv);
      lines = getenv(lenv);

      if(columns != NULL && lines != NULL) {
        x = atoi(columns);
        y = atoi(lines);
      }
      else {
        columns = NULL;
        lines = NULL;

        /* If they don't exist, ask the user for them directly,
        then try to store them for next time with set*/
        printf("SCREEN COLUMNS (80)?\n");
        d_fgets(&columns, stdin);

        printf("SCREEN LINES (24)?\n");
        d_fgets(&lines, stdin);

        x = myatoi(columns);
        y = myatoi(lines);

        if(x < 1) {
          x = 80; /* default value */
        }

        if(y < 1) {
          y = 24; /* default value */
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
  }

  printf("x: %d, y: %d\n", x, y);

  return EXIT_SUCCESS;
}