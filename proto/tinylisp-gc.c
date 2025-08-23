/* tinylisp-gc.c with ref count garbage collection and error handling by Robert A. van Engelen 2025 */
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
L err(I i) { longjmp(jb,i); }
I ref[N/2],hp,fp,lp,fn,ATOM=0x7ff8,PRIM=0x7ff9,CONS=0x7ffa,CLOS=0x7ffb,NIL=0x7ffc;
L cell[N],nil,tru,env;
L box(I t,I i) { L x; *(unsigned long long*)&x = (unsigned long long)t<<48|i; return x; }
I ord(L x) { return *(unsigned long long*)&x; }
L num(L n) { return n; }
I equ(L x,L y) { return *(unsigned long long*)&x == *(unsigned long long*)&y; }
L atom(const char *s) {
 I i = 0; while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 return i == hp && (hp += strlen(strcpy(A+i,s))+1) > lp<<3 ? err(4) : box(ATOM,i);
}
I lomem(I i) { return lp = i < lp ? i : lp; }
L alloc() { I i = fp; fp = ref[i/2]; ref[i/2] = 1; --fn; return hp > lomem(i)<<3 ? err(4) : box(CONS,i); }
L cons(L x,L y) { L p = alloc(); cell[ord(p)+1] = x; cell[ord(p)] = y; return p; } 
L car(L p) { return (T(p)&~(CONS^CLOS)) == CONS ? cell[ord(p)+1] : err(1); }
L cdr(L p) { return (T(p)&~(CONS^CLOS)) == CONS ? cell[ord(p)] : err(1); }
L pair(L v,L x,L e) { return cons(cons(v,x),e); }
L closure(L v,L x,L e) { return box(CLOS,ord(pair(v,x,e))); }
L assoc(L v,L e) { while (T(e) == CONS && !equ(v,car(car(e)))) e = cdr(e); return T(e) == CONS ? cdr(car(e)) : err(2); }
I not(L x) { return T(x) == NIL; }
I let(L x) { return !not(x) && !not(cdr(x)); }
L dup(L x) { if ((T(x)&~(CONS^CLOS)) == CONS) ++ref[ord(x)/2]; return x; }
void del(I i) { ref[i/2] = fp; fp = i; ++fn; }
void gc(L x) { I i; if ((T(x)&~(CONS^CLOS)) == CONS && !--ref[(i = ord(x))/2]) { del(i); gc(cell[i+1]),gc(cell[i]); } }
L eval(L,L),parse();
L evlis(L t,L e) { return T(t) == CONS ? cons(eval(car(t),e),evlis(cdr(t),e)) : T(t) == ATOM ? dup(assoc(t,e)) : nil; }
L f_eval(L t,L e) { L x = eval(car(t = evlis(t,e)),e); gc(t); return x; }
L f_quote(L t,L _) { return dup(car(t)); }
L f_cons(L t,L e) { L p; t = evlis(t,e); p = cons(dup(car(t)),dup(car(cdr(t)))); gc(t); return p; }
L f_car(L t,L e) { L x = dup(car(car(t = evlis(t,e)))); gc(t); return x; }
L f_cdr(L t,L e) { L x = dup(cdr(car(t = evlis(t,e)))); gc(t); return x; }
L f_add(L t,L e) { L s = t = evlis(t,e),n = car(s); while (!not(s = cdr(s))) n += car(s); gc(t); return num(n); }
L f_sub(L t,L e) { L s = t = evlis(t,e),n = car(s); while (!not(s = cdr(s))) n -= car(s); gc(t); return num(n); }
L f_mul(L t,L e) { L s = t = evlis(t,e),n = car(s); while (!not(s = cdr(s))) n *= car(s); gc(t); return num(n); }
L f_div(L t,L e) { L s = t = evlis(t,e),n = car(s); while (!not(s = cdr(s))) n /= car(s); gc(t); return num(n); }
L f_int(L t,L e) { L n = car(t = evlis(t,e)); n = n<1e16 && n>-1e16 ? (long long)n : n; gc(t); return n; }
L f_lt(L t,L e) { L x; t = evlis(t,e); x = car(t) - car(cdr(t)) < 0 ? tru : nil; gc(t); return x; }
L f_eq(L t,L e) { L x; t = evlis(t,e); x = equ(car(t),car(cdr(t))) ? tru : nil; gc(t); return x; }
L f_pair(L t,L e) { L x = car(t = evlis(t,e)); x = T(x) == CONS ? tru : nil; gc(t); return x; }
L f_or(L t,L e) { L x = nil; for (; !not(t) && not(x); x = eval(car(t),e),t = cdr(t)) gc(x); return x; }
L f_and(L t,L e) { L x = tru; for (; !not(t) && !not(x); x = eval(car(t),e),t = cdr(t)) gc(x); return x; }
L f_not(L t,L e) { L x = not(car(t = evlis(t,e))) ? tru : nil; gc(t); return x; }
L f_cond(L t,L e) { L x; while (x = eval(car(car(t)),e),gc(x),not(x)) t = cdr(t); return eval(car(cdr(car(t))),e); }
L f_if(L t,L e) { L x = eval(car(t),e),y = eval(car(cdr(not(x) ? cdr(t) : t)),e); gc(x); return y; }
L f_leta(L t,L e) { L x; for (e = dup(e); let(t); t = cdr(t)) e = pair(car(car(t)),eval(car(cdr(car(t))),e),e); x = eval(car(t),e); gc(e); return x; }
L f_lambda(L t,L e) { return closure(dup(car(t)),dup(car(cdr(t))),equ(e,env) ? nil : dup(e)); }
L f_define(L t,L e) { env = pair(car(t),eval(car(cdr(t)),e),env); return car(t); }
struct { const char *s; L (*f)(L,L); } prim[] = {
{"eval", f_eval },{"car",f_car},{"-",f_sub},{"<",  f_lt },{"or", f_or },{"cond",f_cond},{"lambda",f_lambda},
{"quote",f_quote},{"cdr",f_cdr},{"*",f_mul},{"int",f_int},{"and",f_and},{"if",  f_if  },{"define",f_define},
{"cons", f_cons },{"+",  f_add},{"/",f_div},{"eq?",f_eq },{"not",f_not},{"let*",f_leta},{"pair?", f_pair  },{0}};
L bind(L v,L t,L e) { return not(v) ? e : T(v) == CONS ? bind(cdr(v),cdr(t),pair(car(v),dup(car(t)),e)) : pair(v,dup(t),e); }
L reduce(L f,L t,L e) { L x = eval(cdr(car(f)),e = bind(car(car(f)),t = evlis(t,e),dup(not(cdr(f)) ? env : cdr(f)))); gc(e); gc(t); return x; }
L apply(L f,L t,L e) { return T(f) == PRIM ? prim[ord(f)].f(t,e) : T(f) == CLOS ? reduce(f,t,e) : err(3); }
L eval(L x,L e) { L f = nil; x = T(x) == ATOM ? dup(assoc(x,e)) : T(x) == CONS ? apply(f = eval(car(x),e),cdr(x),e) : dup(x); gc(f); return x; }
char buf[40],see = ' ';
void look() { int c = getchar(); if (c == EOF) freopen("/dev/tty","r",stdin),c = ' '; see = c; }
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
L list() { L x; return scan() == ')' ? nil : !strcmp(buf, ".") ? (x = Read(),scan(),x) : (x = parse(),cons(x,list())); }
L quote() { return cons(atom("quote"),cons(Read(),nil)); }
L atomic() { L n; int i; return sscanf(buf,"%lg%n",&n,&i) > 0 && !buf[i] ? n : atom(buf); }
L parse() { return *buf == '(' ? list() : *buf == '\'' ? quote() : atomic(); }
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
 for (fp = 0,lp = N-2,fn = 1,i = 2; i < N; i += 2) if (ref[i/2]) lomem(i); else del(i);
}
void rebuild() { memset(ref,0,sizeof(ref)); mark(env); sweep(); }
int main() {
 I i; printf("tinylisp-gc");
 env = 0; rebuild();
 nil = box(NIL,0); atom("ERR"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
 if ((i = setjmp(jb)) > 0) printf("\e[31;1mERR %u%s\e[m",i,i == 4 ? " out of memory" : "");
 while (1) { L x,y; rebuild(); printf("\n%u>",2*fn-hp/8); print(y = eval(x = Read(),env)); gc(y); gc(x); }
}
