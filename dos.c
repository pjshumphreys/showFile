#include <stdio.h>
#include <dos.h>

void main() {
  int len_written = 0;
  union REGS regs;

  setvbuf(stdin, NULL, _IONBF, 0);
  setvbuf(stdout, NULL, _IONBF, 0);

  fflush(stdin);
  printf("\n\033[6n\b\b\b\b");
  fflush(stdout);

  /*stuff the keyboard buffer so we can reliably call
  fgetc the first time without blocking */
  regs.h.ah = 0x05;
  regs.h.ch = 0x1e;
  regs.h.cl = 0x61;
  int86(0x16, &regs, &regs);

  len_written = getch();
  printf("q %d w\n", len_written);

  if(len_written == 27) {
    do {
      len_written = getch();
      printf("q %d w\n", len_written);
    } while (len_written != 'a');
  }
}