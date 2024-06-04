#include <stdio.h>

struct Person {
   int age;
   char gender;
   char first_name[BUFSIZ];
   char last_name[BUFSIZ];
};

int main() {
   struct Person gabe = {31, 'M', "Gabriel", "Lepoutre"};

   printf("%s %s is %d years old and is %c\n", gabe.first_name, gabe.last_name, gabe.age, gabe.gender);
   
   return 0;
}