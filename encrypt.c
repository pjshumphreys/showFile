#include <stdio.h>

int main(int argc, char * argv[]) {
  FILE * input;
  FILE * output;

  unsigned char * key = "R0+13/P1u$p|Us";
  unsigned char* current = key;
  int readByte;
  unsigned char result;
  unsigned char checksum = 0;

  //read file
  input = fopen("testfile.txt", "rb");
  output = fopen("temp.bin", "wb");

  //encrypt it to stop casual tampering, computing a checksum as we go
  while((readByte = fgetc(input)) != EOF) {
    result = ((unsigned char)readByte)+(*current);

    checksum += result;

    switch(result) {
      case 0: {
        fputc('\0', output);
        fputc('\1', output);
      } break;

      case 10: {
        fputc('\0', output);
        fputc('\2', output);
      } break;

      case 13: {
        fputc('\0', output);
        fputc('\3', output);
      }

      case 26: {  /* don't ever put ASCII CTRL-Z in the file as it breaks reading on cp/m*/
        fputc('\0', output);
        fputc('\4', output);
      } break;

      default: {
        fputc(result, output);
      }
    }

    if(*(++current) == 0) {
      current = key;
    }
  }

  fclose(input);
  fclose(output);

  input = fopen("temp.bin", "rb");
  output = fopen("output.bin", "wb");

  //write a null byte to prevent any encrypted data being visible in a text editor
  fputc('\0', output);

  //write the checksum byte
  fputc(checksum, output);

  while((readByte = fgetc(input)) != EOF) {
    fputc(readByte, output);
  }

  fclose(input);
  fclose(output);

  return 0;
}