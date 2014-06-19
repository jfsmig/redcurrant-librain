gcc -ldl -g -O2 -Wall -Wextra -pipe -std=gnu99 -Wunused -Wsequence-point -Wredundant-decls `pkg-config --cflags --libs glib-2.0` test.c -o test
