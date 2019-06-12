#include <stdio.h>

int main(int argc, char* argv[]) {
  FILE* input;

  unsigned char * key = "R0+13/P1u$p|Us";
  unsigned char* current = key;
  unsigned char orig;
  unsigned char computed = 0;
  int readByte;

  input = fopen("output.bin", "rb");

  if(fgetc(input) == 0) { //check the first byte is 0
    orig = fgetc(input);  //keep the checksum for later

    while((readByte = fgetc(input)) != EOF) {
      if(readByte == 0) {
        switch(fgetc(input)) {
          case '\1':
            readByte = 0;
          break;

          case '\2':
            readByte = 10;
          break;

          case '\3':
            readByte = 13;
          break;

          case '\4':
            readByte = 26;
          break;
        }
      }

      computed += readByte;
    }

    fclose(input);

    if(orig != computed) {
      return 0;
    }
  }

  input = fopen("output.bin", "rb");
  fgetc(input);
  fgetc(input);

  while((readByte = fgetc(input)) != EOF) {
    if(readByte == 0) {
      switch(fgetc(input)) {
        case '\1':
          readByte = 0;
        break;

        case '\2':
          readByte = 10;
        break;

        case '\3':
          readByte = 13;
        break;

        case '\4':
          readByte = 26;
        break;
      }
    }

    orig = readByte-(*current);

    if(*(++current) == 0) {
      current = key;
    }

    fputc(orig, stdout);
  }

  return 0;
}