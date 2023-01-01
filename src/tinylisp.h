#ifndef __TINY_LISP_H
#define __TINY_LISP_H

#define I unsigned
#define L double

extern L env;
extern I hp, sp;

void gc(void);
L eval(L exp, L env);
L read(void);
void print(L exp);
void pretty_print(L x, int level);
void init(void);

#endif  // ifndef __TINY_LISP_H