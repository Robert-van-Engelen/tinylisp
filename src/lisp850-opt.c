/* lisp850-opt.c with BCD boxing (optimized version) by Robert A. van Engelen 2022 */
#define I unsigned
#define L double
#define T *(char*)&
#define A (char*)cell
#define N 1024
I hp=0,sp=N,ATOM=32,PRIM=48,CONS=64,CLOS=80,NIL=96;
L cell[N],nil,tru,err,env;
L box(I t,I i) { L x = i+10; T x = t; return x; }
I ord(L x) { T x &= 15; return (I)x-10; }
L num(L n) { T n &= 159; return n; }
L atom(const char *s) {
 I i = 0; while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 if (i == hp && (hp += strlen(strcpy(A+i,s))+1) > sp<<3) abort();
 return box(ATOM,i);
}
L cons(L x,L y) { cell[--sp] = x; cell[--sp] = y; if (hp > sp<<3) abort(); return box(CONS,sp); }
L car(L p) { return (T p&224) == CONS ? cell[T p &= 15,(I)p-9] : err; }
L cdr(L p) { return (T p&224) == CONS ? cell[T p &= 15,(I)p-10] : err; }
L pair(L v,L x,L e) { return cons(cons(v,x),e); }
L closure(L v,L x,L e) { return box(CLOS,ord(pair(v,x,e == env ? nil : e))); }
L assoc(L v,L e) { while (T e == CONS && v != car(car(e))) e = cdr(e); return T e == CONS ? cdr(car(e)) : err; }
I not(L x) { return T x == NIL; }
I let(L x) { return !not(x) && !not(cdr(x)); }
L eval(L,L),parse();
L evlis(L t,L e) {
 L s,*p;
 for (s = nil,p = &s; T t == CONS; p = cell+sp,t = cdr(t)) *p = cons(eval(car(t),e),nil);
 if (T t == ATOM) *p = assoc(t,e);
 return s;
}
L f_eval(L t,L *e) { return car(evlis(t,*e)); }
L f_quote(L t,L *_) { return car(t); }
L f_cons(L t,L *e) { return t = evlis(t,*e),cons(car(t),car(cdr(t))); }
L f_car(L t,L *e) { return car(car(evlis(t,*e))); }
L f_cdr(L t,L *e) { return cdr(car(evlis(t,*e))); }
L f_add(L t,L *e) { L n = car(t = evlis(t,*e)); while (!not(t = cdr(t))) n += car(t); return num(n); }
L f_sub(L t,L *e) { L n = car(t = evlis(t,*e)); while (!not(t = cdr(t))) n -= car(t); return num(n); }
L f_mul(L t,L *e) { L n = car(t = evlis(t,*e)); while (!not(t = cdr(t))) n *= car(t); return num(n); }
L f_div(L t,L *e) { L n = car(t = evlis(t,*e)); while (!not(t = cdr(t))) n /= car(t); return num(n); }
L f_int(L t,L *e) { L n = car(evlis(t,*e)); return n-1e9 < 0 && n+1e9 > 0 ? (long)n : n; }
L f_lt(L t,L *e) { return t = evlis(t,*e),car(t) - car(cdr(t)) < 0 ? tru : nil; }
L f_eq(L t,L *e) { return t = evlis(t,*e),car(t) == car(cdr(t)) ? tru : nil; }
L f_pair(L t,L *e) { L x = car(evlis(t,*e)); return T x == CONS ? tru : nil; }
L f_or(L t,L *e) { L x = nil; while (T t != NIL && not(x = eval(car(t),*e))) t = cdr(t); return x; }
L f_and(L t,L *e) { L x = tru; while (T t != NIL && !not(x = eval(car(t),*e))) t = cdr(t); return x; }
L f_not(L t,L *e) { return not(car(evlis(t,*e))) ? tru : nil; }
L f_cond(L t,L *e) { while (T t != NIL && not(eval(car(car(t)),*e))) t = cdr(t); return car(cdr(car(t))); }
L f_if(L t,L *e) { return car(cdr(not(eval(car(t),*e)) ? cdr(t) : t)); }
L f_leta(L t,L *e) { for (;let(t); t = cdr(t)) *e = pair(car(car(t)),eval(car(cdr(car(t))),*e),*e); return car(t); }
L f_lambda(L t,L *e) { return closure(car(t),car(cdr(t)),*e); }
L f_define(L t,L *e) { env = pair(car(t),eval(car(cdr(t)),*e),env); return car(t); }
struct { const char *s; L (*f)(L,L*); short t; } prim[] = {
{"eval",  f_eval,  1},{"quote", f_quote, 0},{"cons", f_cons,0},{"car", f_car, 0},{"cdr",f_cdr,0},{"+",   f_add, 0},
{"-",     f_sub,   0},{"*",     f_mul,   0},{"/",    f_div, 0},{"int", f_int, 0},{"<",  f_lt, 0},{"eq?", f_eq,  0},
{"or",    f_or,    0},{"and",   f_and,   0},{"not",  f_not, 0},{"cond",f_cond,1},{"if", f_if, 1},{"let*",f_leta,1},
{"lambda",f_lambda,0},{"define",f_define,0},{"pair?",f_pair,0},{0}};
L eval(L x,L e) {
 L f,v,d;
 while (1) {
  if (T x == ATOM) return assoc(x,e);
  if (T x != CONS) return x;
  f = eval(car(x),e); x = cdr(x);
  if (T f == PRIM) {
   x = prim[ord(f)].f(x,&e);
   if (prim[ord(f)].t) continue;
   return x;
  }
  if (T f != CLOS) return err;
  v = car(car(f)); d = cdr(f);
  if (T d == NIL) d = env;
  for (;T v == CONS && T x == CONS; v = cdr(v),x = cdr(x)) d = pair(car(v),eval(car(x),e),d);
  for (x = T x == CONS ? evlis(x,e) : eval(x,e); T v == CONS; v = cdr(v),x = cdr(x)) d = pair(car(v),car(x),d);
  if (T v == ATOM) d = pair(v,x,d);
  x = cdr(car(f)); e = d;
 }
}
char buf[40],see = ' ';
void look() { int c = getchar(); see = c; if (c == -1) exit(0); }
I seeing(char c) { return c == ' ' ? see > 0 && see <= c : see == c; }
char get() { char c = see; look(); return c; }
char scan() {
 int i = 0;
 while (seeing(' ')) look();
 if (seeing('(') || seeing(')') || seeing('\'')) buf[i++] = get();
 else do buf[i++] = get(); while (i < 39 && !seeing('(') && !seeing(')') && !seeing(' '));
 return buf[i] = 0,*buf;
}
L read() { return scan(),parse(); }
L list() {
 L t,*p;
 for (t = nil,p = &t; ; *p = cons(parse(),nil),p = cell+sp) {
  if (scan() == ')') return t;
  if (*buf == '.' && !buf[1]) return *p = read(),scan(),t;
 }
}
L parse() {
 L n; int i;
 if (*buf == '(') return list();
 if (*buf == '\'') return cons(atom("quote"),cons(read(),nil));
 i = strlen(buf);
 return isdigit(buf[*buf == '-']) && sscanf(buf,"%lg%n",&n,&i) > 0 && !buf[i] ? n : atom(buf);
}
void print(L);
void printlist(L t) {
 for (putchar('('); ; putchar(' ')) {
  print(car(t));
  if (not(t = cdr(t))) break;
  if (T t != CONS) { printf(" . "); print(t); break; }
 }
 putchar(')');
}
void print(L x) {
 if (T x == NIL) printf("()");
 else if (T x == ATOM) printf("%s",A+ord(x));
 else if (T x == PRIM) printf("<%s>",prim[ord(x)].s);
 else if (T x == CONS) printlist(x);
 else if (T x == CLOS) printf("{%u}",ord(x));
 else printf("%.10lg",x);
}
void gc() { sp = ord(env); }
int main() {
 int i;
 printf("lisp850");
 nil = box(NIL,0); err = atom("ERR"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
 while (1) { printf("\n%u>",sp-hp/8); print(eval(read(),env)); gc(); }
}
