#include <stdio.h>

int main() {
   int character;
   int prev_char_is_space = 0;

   printf("Enter a sentence with multiple spaces (Press CTRL+D to exit):\n");

   while ((character = getchar()) != EOF) {
      if (character == ' ' || character == '\t') {
         if (!prev_char_is_space) {
            putchar(character);
            prev_char_is_space = 1;
         }
      } else {
         putchar(character);
         prev_char_is_space = 0;
      }
   }

   printf("\n");
   return 0;
}