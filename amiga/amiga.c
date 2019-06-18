#include <stdio.h>
#include <string.h>

FILE foo;
FILE* stdin2;

int main(int argc, char** argv) {
  FILE* fp;

  int temp;
  if(argc != 0) {
    return 0;
  }

  /* point the file pointer at global memory as it will
  be properly cleaned up even if ctrl-c if pressed. */
  stdin2 = &foo;

  /* open console for reading and writing so that we can prevent the console window
  from closing until enter has been pressed */
  if((fp = freopen("RAW:30/30/490/192/QueryCSV", "a+", stdout)) == NULL) {
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
  fputs("\033[12{Press a key\n", stdout);

  do {
    temp = fgetc(stdin2);
    printf("%d ", temp);
    fflush(stdout);
  } while (temp != 'q');

  return 0;
}
