/* lisp850.c with BCD boxing (optimized version) by Robert A. van Engelen 2022 */
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
L assoc(L v,L e) { while (T e == CONS && v != car(car(e))) e = cdr(e); return T e == CONS ? cdr(car(e)) : err; }
I not(L x) { return T x == NIL; }
I let(L x) { return T x != NIL && (x = cdr(x),T x != NIL); }
L eval(L,L),parse();
L evlis(L t,L e) {
 L s,*p; for (s = nil,p = &s; T t == CONS; p = cell+sp,t = cdr(t)) *p = cons(eval(car(t),e),nil);
 if (T t) != NIL) *p = eval(t,e);
 return s;
}
L f_eval(L t,L e) { return eval(car(evlis(t,e)),e); }
L f_quote(L t,L _) { return car(t); }
L f_cons(L t,L e) { return t = evlis(t,e),cons(car(t),car(cdr(t))); }
L f_car(L t,L e) { return car(car(evlis(t,e))); }
L f_cdr(L t,L e) { return cdr(car(evlis(t,e))); }
L f_add(L t,L e) { L n = car(t = evlis(t,e)); while (!not(t = cdr(t))) n += car(t); return num(n); }
L f_sub(L t,L e) { L n = car(t = evlis(t,e)); while (!not(t = cdr(t))) n -= car(t); return num(n); }
L f_mul(L t,L e) { L n = car(t = evlis(t,e)); while (!not(t = cdr(t))) n *= car(t); return num(n); }
L f_div(L t,L e) { L n = car(t = evlis(t,e)); while (!not(t = cdr(t))) n /= car(t); return num(n); }
L f_int(L t,L e) { return t = car(evlis(t,e)),t-1e9 < 0 && t+1e9 > 0 ? (long)t : t; }
L f_lt(L t,L e) { return t = evlis(t,e),car(t) - car(cdr(t)) < 0 ? tru : nil; }
L f_eq(L t,L e) { return t = evlis(t,e),car(t) == car(cdr(t)) ? tru : nil; }
L f_not(L t,L e) { return not(car(evlis(t,e))) ? tru : nil; }
L f_or(L t,L e) { L x = nil; while (T t != NIL && not(x = eval(car(t),e))) t = cdr(t); return x; }
L f_and(L t,L e) { L x = nil; while (T t != NIL && !not(x = eval(car(t),e))) t = cdr(t); return x; }
L f_cond(L t,L e) { while (T t == CONS && not(eval(car(car(t)),e))) t = cdr(t); return eval(car(cdr(car(t))),e); }
L f_if(L t,L e) { return eval(car(cdr(not(eval(car(t),e)) ? cdr(t) : t)),e); }
L f_leta(L t,L e) { while (let(t)) e = pair(car(car(t)),eval(car(cdr(car(t))),e),e),t = cdr(t); return eval(car(t),e); }
L f_lambda(L t,L e) { return box(CLOS,ord(pair(car(t),car(cdr(t)),e == env ? nil : e))); }
L f_define(L t,L e) { env = pair(car(t),eval(car(cdr(t)),e),env); return car(t); }
struct { const char *s; L (*f)(L,L); } prim[] = {
{"eval",f_eval},{"quote",f_quote},{"cons",f_cons},{"car", f_car}, {"cdr",   f_cdr},   {"+",     f_add},   {"-",  f_sub},
{"*",   f_mul}, {"/",    f_div},  {"int", f_int}, {"<",   f_lt},  {"eq?",   f_eq},    {"or",    f_or},    {"and",f_and},
{"not", f_not}, {"cond", f_cond}, {"if",  f_if},  {"let*",f_leta},{"lambda",f_lambda},{"define",f_define},{0}};
L eval(L x,L e) {
 L f,v,d;
 for (; ; x = cdr(car(f)),e = d) {
  if (T x == ATOM) return assoc(x,e);
  if (T x != CONS) return x;
  f = eval(car(x),e),x = cdr(x);
  if (T f == PRIM) return prim[T f &= 15,(I)f-10].f(x,e);
  if (T f != CLOS) return err;
  v = car(car(f)),d = cdr(f);
  if (T d == NIL) d = env;
  while (T v == CONS && T x == CONS) d = pair(car(v),eval(car(x),e),d),v = cdr(v),x = cdr(x);
  if (T v == CONS) x = eval(x,e);
  while (T v == CONS) d = pair(car(v),car(x),d),v = cdr(v),x = cdr(x);
  if (T x == CONS) x = evlis(x,e);
  else if (T x != NIL) x = eval(x,e);
  if (T v != NIL) d = pair(v,x,d);
 }
}
char buf[40],see = ' ';
void look() { see = getchar(); }
I seeing(char c) { return c == ' ' ? see > 0 && see <= c : see == c; }
char get() { char c = see; look(); return c; }
char scan() {
 I i = 0;
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
 L n; I i;
 if (*buf == '(') return list();
 if (*buf == '\'') return cons(atom("quote"),cons(read(),nil));
 i = strlen(buf);
 return isdigit(buf[*buf == '-']) && sscanf(buf,"%lg%n",&n,&i) > 0 && !buf[i] ? n : atom(buf);
}
void print(L);
void printlist(L t) {
 putchar('(');
 while (1) {
  print(car(t));
  if (not(t = cdr(t))) break;
  if (T t != CONS) { printf(" . "); print(t); break; }
  putchar(' ');
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
 I i; printf("lisp850");
 nil = box(NIL,0); err = atom("ERR"); tru = atom("#t"); env = pair(tru,tru,nil);
 for (i = 0; prim[i].s; ++i) env = pair(atom(prim[i].s),box(PRIM,i),env);
 while (1) { printf("\n%u>",sp-hp/8); print(eval(read(),env)); gc(); }
}
