#include <stdio.h>
#include <string.h>

#define CSI "\033["
#define FALSE 0
#define TRUE !FALSE

#define KEY_RESIZE 800
#define KEY_UP 321
#define KEY_DOWN 322
#define KEY_LEFT 324
#define KEY_RIGHT 323



FILE foo;
FILE* stdin2;

int parseCursorLocation(int *x, int * y) {
  int temp, i, arr[2];

  temp = fgetc(stdin2);

  if(temp == 27 || temp == 155) {
    /* skip [*/
    if(temp == 27) {
      fgetc(stdin2);
    }

    for(i = 0; i != 2; i++) {
      arr[i] = 0;

      do {
        temp = fgetc(stdin2);

        if(temp > 47 && temp < 58) { /* quit if not a digit */
          arr[i] = arr[i] * 10 + temp - 48;
        }
        else {
          break;
        }
      } while (1);
    }

    *x = arr[1];
    *y = arr[0];

    return TRUE;
  }

  return FALSE;
}

int mygetch() {
  int temp, arr = 0;
  int i = 0;
  int currentItem = 0;
  int retval = 0;

  do {
    temp = fgetc(stdin2);

    if(temp == 155) {
      i = 0;
      arr = 0;

      do {
        do {
          temp = fgetc(stdin2);

          if(temp > 47 && temp < 58) { /* quit if not a digit */
            if(i == 0) {
              arr = arr * 10 + temp - 48;
            }
          }
          else {
            break;
          }
        } while (1);

        i++;
      } while (temp == 59);

      if(i == 8 && arr == 12) {
        retval = KEY_RESIZE;
      }
      else {
        if(temp == 63) {
          fgetc(stdin2);
        }

        retval = temp + 256;
      }
    }
    else if(temp) {
      if(temp > 31 || temp == 27 || temp == 8 || temp == 9 || temp == 13) {
        retval = temp;
      }
    }
  } while(retval == 0);

  return retval;
}


int main(int argc, char** argv) {
  int origx = 0;
  int origy = 0;
  int temp;

  FILE * input;

  int width = 0;
  int height = 0;

  int leftOffset = 0;

  int virtualWidth = 80;

  int currentLine = 0;
  int previousLine = 0;
  int maxLine = 0;
  int firstTime = TRUE;


  FILE* fp;

  /*
  if(argc != 0) {
    return 0;
  }
  */

  /* point the file pointer at global memory as it will
  be properly cleaned up even if ctrl-c if pressed. */
  stdin2 = &foo;

  /* open console for reading and writing so that we can prevent the console window
  from closing until enter has been pressed */
  if((fp = freopen("RAW:30/30/502/173/QueryCSV", "a+", stdout)) == NULL) {
    return 0;
  }

  /* Hack to get keystrokes from the same console window by
  reusing the buffers set up for stdin. This makes stdin itself unusable,
  but doesn't crash even if ctrl-c is pressed. This is because no files
  were (re)opened, so none will be closed */
  memcpy(stdin2, stdin, sizeof(FILE));
  stdin2->filehandle = stdout->filehandle;

  /* enable the raw events so we can be notified
  if the window is resized */
  fputs(CSI "12{" CSI "6n", stdout);
  fflush(stdout);

  parseCursorLocation(&origx, &origy);

  /* attempt to move the cursor to 255, 255 then get the cursor position
  again. This will get limited to the terminal's actual size*/
  printf(CSI "255;255H" CSI "6n");
  fflush(stdout);

  parseCursorLocation(&width, &height);

  /* reposition the cursor  */
  printf(CSI "%d;%dH", origy, origx);
  fflush(stdout);

  printf("x: %d, y: %d\n", width, height);
  fflush(stdout);

  do {
    temp = mygetch();

    switch(temp) {
      case KEY_RESIZE: {
        fputs(CSI "6n", stdout);
        fflush(stdout);

        parseCursorLocation(&origx, &origy);

        /* attempt to move the cursor to 255, 255 then get the cursor position
        again. This will get limited to the terminal's actual size*/
        printf(CSI "255;255H" CSI "6n");
        fflush(stdout);

        parseCursorLocation(&width, &height);

        /* reposition the cursor  */
        printf(CSI "%d;%dH", origy, origx);
        fflush(stdout);

        printf("x: %d, y: %d\n", width, height);
        fflush(stdout);
      } break;

      case KEY_UP: {
        printf("up\n");
      } break;

      case KEY_DOWN: {
        printf("down\n");
      } break;

      case KEY_LEFT: {
        printf("left\n");
      } break;

      case KEY_RIGHT: {
        printf("right\n");
      } break;
    }
  } while (temp != 'q');

  return 0;
}
