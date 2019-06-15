#include <conio.h>
#include <stdio.h>
#include <dos.h>

#define CSI "\033["
#define FALSE 0
#define TRUE !FALSE

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

        if(temp > 47 && temp < 58) { /* quit if not digit */
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
  int x = 0;
  int y = 0;

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

    parseCursorLocation(&x, &y);

    /* reposition the cursor */
    printf(CSI "%d;%dH", origy-1, origx);
    fflush(stdout);
  }
  else {
    /* The console doesn't support vt100 control codes.
    Use interupt 10h services the control it instead */
    noAnsi(&x, &y);
  }

  printf("x: %d, y: %d", x, y);
  fflush(stdout);;
}