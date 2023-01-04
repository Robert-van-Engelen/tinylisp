#ifndef __TINY_LISP_H
#define __TINY_LISP_H

#define I unsigned
#define L double

extern L env;
extern I hp, sp;

void gc(void);
L eval(L exp, L env);
L Read(void);
void print(L exp);
void pretty_print(L x, int level);
void init_tinylisp(void);
int _main(int argc, char **argv);

char scan(void);
extern char buf[40];

#endif  // ifndef __TINY_LISP_H