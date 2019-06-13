#include <conio.h>
#include <stdio.h>
#include <dos.h>

#define CSI "\033["

int main(int argc, char * argv[]) {
  int temp = 0;
  int x = 0;
  int y = 0;
  int oldy = 0;
  int origy = 0;
  int origx = 0;
  union REGS regs;

  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  /* clear the keyboard buffer */
  while(kbhit()) {
    getch();
  }

  /* test for vt100 capability */
  printf("\n" CSI "6n\b\b\b\b");
  fflush(stdout);

  /*stuff the keyboard buffer so we can reliably call
  getch the first time without blocking */
  regs.h.ah = 0x05;
  regs.h.ch = 0x1e;
  regs.h.cl = 0x61;
  int86(0x16, &regs, &regs);

  if(kbhit() && (temp = getch()) == 27) {
    /* The console supports vt100 control codes */

    /* skip [*/
    getch();

    do {
      temp = getch();

      if(temp > 47 && temp < 58) { /* quit if semicolon */
        origy = origy * 10 + temp - 48;
      }
      else {
        break;
      }
    } while (1);

    do {
      temp = getch();

      if(temp > 47 && temp < 58) { /* quit if 'H' */
        origx = origx * 10 + temp - 48;
      }
      else {
        break;
      }
    } while (1);

    /* consume everthing else on the input buffer */
    while(kbhit() && (getch() != 'a')) {}

    printf(CSI "255;255H" CSI "6n");
    fflush(stdout);

    regs.h.ah = 0x05;
    regs.h.ch = 0x1e;
    regs.h.cl = 0x61;
    int86(0x16, &regs, &regs);

    getch(); /* skip ESC */
    getch(); /* skip [ */

    /* get max y position */
    do {
      temp = getch();

      if(temp > 47 && temp < 58) { /* quit if semicolon */
        y = y * 10 + temp - 48;
      }
      else {
        break;
      }
    } while (1);

    do {
      temp = getch();

      if(temp > 47 && temp < 58) { /* quit if 'H' */
        x = x * 10 + temp - 48;
      }
      else {
        break;
      }
    } while (1);

    /* reposition the cursor */
    printf(CSI "%d;%dH", origy-1, origx);
    fflush(stdout);

    printf("x: %d, y: %d", x, y);
    fflush(stdout);

    while(kbhit() && (getch() != 'a')) {}
  }
  else {
    /* The console doesn't support vt100 control codes.
    Use interupt 10h services the control it instead */

    /* get the current video page */
    regs.h.ah = 0x0f;
    int86(0x10, &regs, &regs);

    x = regs.h.ah; /* bios returns us the column count directly */
    temp = regs.h.bh; /* it also returns us the active page number */

    /* get the current cursor position */
    regs.h.ah = 0x03;
    regs.h.bh = temp;
    int86(0x10, &regs, &regs);

    oldy = origy = y = regs.h.dh;

    do {
      printf("\n");

      /* Read the current x and y. When the y value stops
      increasing, this is how big the screen is */
      regs.h.ah = 0x03;
      regs.h.bh = temp;
      int86(0x10, &regs, &regs);

      y = regs.h.dh;

    } while(oldy != y && (oldy = y));

    y++;

    regs.h.ah = 0x02;
    regs.h.dh = origy-2;
    regs.h.dl = 0;
    int86(0x10, &regs, &regs);

    printf("x: %d, y: %d", x, y);
  }
}