/* tinylisp-opt-gc.c optimized and ref count garbage collection by Robert A. van Engelen 2025 */
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define I unsigned
#define L double
#define T(x) *(unsigned long long*)&x>>48
#define A (char*)cell
#define N 8192
#include <setjmp.h>
jmp_buf jb; /* we longjmp(jb,1) to REPL on memory overflow then mark-sweep garbage collect, instead of abort() */
I ref[N/2],hp,fp,lp,fn,ATOM=0x7ff8,PRIM=0x7ff9,CONS=0x7ffa,CLOS=0x7ffb,NIL=0x7ffc;
L cell[N],nil,tru,err,env;
L box(I t,I i) { L x; *(unsigned long long*)&x = (unsigned long long)t<<48|i; return x; }
I ord(L x) { return *(unsigned long long*)&x; }
L num(L n) { return n; }
I equ(L x,L y) { return *(unsigned long long*)&x == *(unsigned long long*)&y; }
L atom(const char *s) {
 I i = 0; while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 if (i == hp && (hp += strlen(strcpy(A+i,s))+1) > lp<<3) longjmp(jb,1);
 return box(ATOM,i);
}
I lomem(I i) { return lp = i < lp ? i : lp; }
L alloc() { I i = fp; fp = ref[i/2]; ref[i/2] = 1; --fn; if (hp > lomem(i)<<3) longjmp(jb,1); return box(CONS,i); }
L cons(L x,L y) { L p = alloc(); cell[ord(p)+1] = x; cell[ord(p)] = y; return p; }
L car(L p) { return (T(p)&~(CONS^CLOS)) == CONS ? cell[ord(p)+1] : err; }
L cdr(L p) { return (T(p)&~(CONS^CLOS)) == CONS ? cell[ord(p)] : err; }
L pair(L v,L x,L e) { return cons(cons(v,x),e); }
L closure(L v,L x,L e) { return box(CLOS,ord(pair(v,x,e))); }
L assoc(L v,L e) { while (T(e) == CONS && !equ(v,car(car(e)))) e = cdr(e); return T(e) == CONS ? cdr(car(e)) : err; }
I not(L x) { return T(x) == NIL; }
I let(L x) { return !not(x) && !not(cdr(x)); }
L dup(L x) { if ((T(x)&~(CONS^CLOS)) == CONS) ++ref[ord(x)/2]; return x; }
void del(I i) { ref[i/2] = fp; fp = i; ++fn; }
void gc(L x) { I i; if ((T(x)&~(CONS^CLOS)) == CONS && !--ref[(i = ord(x))/2]) { del(i); gc(cell[i+1]); gc(cell[i]); } }
L eval(L,L),parse();
L evlis(L t,L e) {
 L s,*p;
 for (s = nil,p = &s; T(t) == CONS; p = &cell[ord(*p)],t = cdr(t)) *p = cons(eval(car(t),e),nil);
 if (T(t) == ATOM) *p = dup(assoc(t,e));
 return s;
}
L evarg(L *t,L *e,I *a) {
 L x;
 if (T(*t) == ATOM) *t = assoc(*t,*e),*a = 1;
 x = car(*t); *t = cdr(*t);
 return *a ? dup(x) : eval(x,*e);
}
L f_eval(L t,L *e) { I a = 0; L x = evarg(&t,e,&a),y = eval(x,*e); gc(x); return y; }
L f_quote(L t,L *_) { return dup(car(t)); }
L f_cons(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); return cons(x,evarg(&t,e,&a)); }
L f_car(L t,L *e) { I a = 0; L x = evarg(&t,e,&a),y = dup(car(x)); gc(x); return y; }
L f_cdr(L t,L *e) { I a = 0; L x = evarg(&t,e,&a),y = dup(cdr(x)); gc(x); return y; }
L f_add(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); while (!not(t)) n += evarg(&t,e,&a); return num(n); }
L f_sub(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); while (!not(t)) n -= evarg(&t,e,&a); return num(n); }
L f_mul(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); while (!not(t)) n *= evarg(&t,e,&a); return num(n); }
L f_div(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); while (!not(t)) n /= evarg(&t,e,&a); return num(n); }
L f_int(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); return n<1e16 && n>-1e16 ? (long long)n : n; }
L f_lt(L t,L *e) { I a = 0; L n = evarg(&t,e,&a); return n - evarg(&t,e,&a) < 0 ? tru : nil; }
L f_eq(L t,L *e) { I a = 0; L x = evarg(&t,e,&a),y = evarg(&t,e,&a); gc(x); gc(y); return equ(x,y) ? tru : nil; }
L f_pair(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); gc(x); return T(x) == CONS ? tru : nil; }
L f_not(L t,L *e) { I a = 0; L x = evarg(&t,e,&a); gc(x); return not(x) ? tru : nil; }
L f_or(L t,L *e) { I a = 0; L x = nil; for (; !not(t) && not(x); x = evarg(&t,e,&a)) gc(x); return x; }
L f_and(L t,L *e) { I a = 0; L x = tru; for (; !not(t) && !not(x); x = evarg(&t,e,&a)) gc(x); return x; }
L f_cond(L t,L *e) { L x; while (x = eval(car(car(t)),*e),gc(x),not(x)) t = cdr(t); return car(cdr(car(t))); }
L f_if(L t,L *e) { L x,y = car(cdr(not(x = eval(car(t),*e)) ? cdr(t) : t)); gc(x); return y; }
L f_leta(L t,L *e) { for (; let(t); t = cdr(t)) *e = pair(car(car(t)),eval(car(cdr(car(t))),*e),*e); return car(t); }
L f_lambda(L t,L *e) { return closure(dup(car(t)),dup(car(cdr(t))),equ(*e,env) ? nil : dup(*e)); }
L f_define(L t,L *e) { env = pair(car(t),eval(car(cdr(t)),*e),env); return car(t); }
struct { const char *s; L (*f)(L,L*); short t; } prim[] = {
{"eval",  f_eval,  0},{"quote", f_quote, 0},{"cons", f_cons,0},{"car", f_car, 0},{"cdr",f_cdr,0},{"+",   f_add, 0},
{"-",     f_sub,   0},{"*",     f_mul,   0},{"/",    f_div, 0},{"int", f_int, 0},{"<",  f_lt, 0},{"eq?", f_eq,  0},
{"or",    f_or,    0},{"and",   f_and,   0},{"not",  f_not, 0},{"cond",f_cond,1},{"if", f_if, 1},{"let*",f_leta,1},
{"lambda",f_lambda,0},{"define",f_define,0},{"pair?",f_pair,0},{0}};
L eval(L x,L e) {
 I a; L f = nil,g,v,d;
 dup(e);
 while (1) {
  if (T(x) == ATOM) { x = dup(assoc(x,e)); break; }
  if (T(x) != CONS) { x = dup(x); break; }
  g = f; f = eval(car(x),e); x = cdr(x);
  if (T(f) == PRIM) {
   x = prim[ord(f)].f(x,&e); gc(g);
   if (prim[ord(f)].t) continue;
   break;
  }
  if (T(f) != CLOS) { x = err; break; }
  v = car(car(f)); d = dup(not(cdr(f)) ? env : cdr(f));
  for (a = 0; T(v) == CONS; v = cdr(v)) d = pair(car(v),evarg(&x,&e,&a),d);
  if (T(v) == ATOM) d = pair(v,a ? dup(x) : evlis(x,e),d);
  x = cdr(car(f)); gc(e); e = d; gc(g);
 }
 gc(e); gc(f);
 return x;
}
char buf[40],see = ' ';
void look() { int c = getchar(); see = c; if (c == EOF) exit(0); }
I seeing(char c) { return c == ' ' ? see > 0 && see <= c : see == c; }
char get() { char c = see; look(); return c; }
char scan() {
 int i = 0;
 while (seeing(' ')) look();
 if (seeing('(') || seeing(')') || seeing('\'')) buf[i++] = get();
 else do buf[i++] = get(); while (i < 39 && !seeing('(') && !seeing(')') && !seeing(' '));
 return buf[i] = 0,*buf;
}
L Read() { return scan(),parse(); }
L list() {
 L t,*p;
 for (t = nil,p = &t; ; *p = cons(parse(),nil),p = &cell[ord(*p)]) {
  if (scan() == ')') return t;
  if (*buf == '.' && !buf[1]) return *p = Read(),scan(),t;
 }
}
L parse() {
 L n; int i;
 if (*buf == '(') return list();
 if (*buf == '\'') return cons(atom("quote"),cons(Read(),nil));
 return sscanf(buf,"%lg%n",&n,&i) > 0 && !buf[i] ? n : atom(buf);
}
void print(L);
void printlist(L t) {
 for (putchar('('); ; putchar(' ')) {
  print(car(t));
  if (not(t = cdr(t))) break;
  if (T(t) != CONS) { printf(" . "); print(t); break; }
 }
 putchar(')');
}
void print(L x) {
 if (T(x) == NIL) printf("()");
 else if (T(x) == ATOM) printf("%s",A+ord(x));
 else if (T(x) == PRIM) printf("<%s>",prim[ord(x)].s);
 else if (T(x) == CONS) printlist(x);
 else if (T(x) == CLOS) printf("{%u}",ord(x));
 else printf("%.10lg",x);
}
void mark(L x) { if ((T(x)&~(CONS^CLOS)) == CONS && !ref[ord(x)/2]++) { mark(cell[ord(x)+1]); mark(cell[ord(x)]); } }
void sweep() {
 I i; for (hp = 0,i = 0; i < N; ++i) if (ref[i/2] && T(cell[i]) == ATOM && ord(cell[i]) > hp) hp = ord(cell[i]);
 hp += strlen(A+hp)+1;
 for (fp = 0,lp = N-2,fn = 1,i = 2; i < N; i += 2) if (!ref[i/2]) del(i); else lomem(i);
}
void rebuild() { memset(ref,0,sizeof(ref)); mark(env); sweep(); }
int main() {
 I i; printf("tinylisp-opt-gc");
 env = 0; rebuild();
 nil = box(NIL,0); err = atom("ERR"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
 if (setjmp(jb)) printf("\e[31;1mout of memory\e[m");
 while (1) { L x,y; rebuild(); printf("\n%u>",2*fn-hp/8); print(y = eval(x = Read(),env)); gc(y); gc(x); }
}
