#include <stdio.h>
#include <stdlib.h>
#include <kernel.h>

const int columns[] = {
  80,
  40,
  20,
  80,
  40,
  20,
  40,
  40,
  80
};

const int rows[] = {
  32,
  32,
  32,
  25,
  32,
  32,
  25,
  25,
  0 /* 0 means just output all lines then quit */
};

int getch() {
  unsigned int keyPressed;

  do {
    keyPressed = _kernel_osbyte(129, 0x01, 0x04);

    if((keyPressed & 0xff00) == 0) {
      return (keyPressed & 0xff);
    }
  } while(1);
}

int main(int argc, char* argv[]) {
  int keyPress;

  /* get the current screen mode and use it to figure out
  the current terminal columns and rows */
  unsigned int currentMode =
    (_kernel_osbyte(135, 0, 0) & 0xff00) >> 8;

  if(currentMode > 7) {
    currentMode = 8;
  }

  printf(
      "columns: %d rows:%d\n",
      columns[currentMode],
      rows[currentMode]
    );

  /* make arrow keys send ascii codes */
  _kernel_osbyte(4, 1, 0);

  do {
    keyPress = getch();

    printf("%d\n", keyPress);
  } while(keyPress != 'q' && keyPress != 'Q');

  /* make arrow keys control the cursor again */
  _kernel_osbyte(4, 0, 0);

  return 0;
}
