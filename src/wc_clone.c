#include <stdio.h>

#define     IN    1
#define     OUT   0

int main() {
   int character, newlines, num_words, num_chars, state;

   state = OUT;
   newlines = num_words = num_chars = 0;

   printf("Enter some text (press CTRL+D to exit):\n\n");

   while ((character = getchar()) != EOF) {
      ++num_chars;
      if (character == '\n')
         ++newlines;
      if (character == ' ' || character == '\n' || character == '\t') {
         state = OUT;
      } else if (state == OUT) {
         state = IN;
         ++num_words;
      }
   }

   printf("Lines: %d\tWords: %d\tCharacters: %d\n", newlines, num_words, num_chars);
   
   return 0;
}