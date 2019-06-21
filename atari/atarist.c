#include <stdio.h>
#include <stdlib.h>

#if defined(__VBCC__) || defined(__TURBOC__) || defined(LATTICE)
  #ifdef __VBCC__
    #include <gem.h>
  #else
    #include <vdi.h>
    #include <aes.h>
  #endif

  #include <tos.h>
#else
  #include <obdefs.h>
  #include <define.h>
  #include <gemdefs.h>
  #include <osbind.h>
#endif

short getch() {
  long x;
  unsigned char ascii;

  /* get keyboard key (no echo ascii mixed with scancodes) */
  x = Cnecin();

  /* bit 0-8 are the atascii code */
  ascii = x & 0xff;

  /* function and arrow key return ascii 0 */
  if(ascii) {
    return ascii;
  }

  /* bits 16-23 are the keyboard scan code. extract them and
  add 256 to the result so we can disambiguate from ascii */
  return ((x & 0xff0000) >> 16) + 256;
}

int main(int argc, char * argv[]) {
  short x;
  short a, b, c, d, rows, columns;

  /* get console width and height */
  vq_chcells(graf_handle(&a, &b, &c, &d), &rows, &columns);

  printf("%d %d\n", columns, rows);

  do {
    x = getch();
    printf("%d\n", x);
  } while (x != 'q');

  return EXIT_SUCCESS;
}