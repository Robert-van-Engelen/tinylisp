/* lisp850.c with BCD boxing by Robert A. van Engelen 2022 */
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
I equ(L x,L y) { return x == y; }
L atom(const char *s) {
 I i = 0; while (i < hp && strcmp(A+i,s)) i += strlen(A+i)+1;
 if (i == hp && (hp += strlen(strcpy(A+i,s))+1) > sp<<3) abort();
 return box(ATOM,i);
}
L cons(L x,L y) { cell[--sp] = x; cell[--sp] = y; if (hp > sp<<3) abort(); return box(CONS,sp); }
L car(L p) { return (T p&~(CONS^CLOS)) == CONS ? cell[ord(p)+1] : err; }
L cdr(L p) { return (T p&~(CONS^CLOS)) == CONS ? cell[ord(p)] : err; }
L pair(L v,L x,L e) { return cons(cons(v,x),e); }
L closure(L v,L x,L e) { return box(CLOS,ord(pair(v,x,equ(e,env) ? nil : e))); }
L assoc(L v,L e) { while (T e == CONS && !equ(v,car(car(e)))) e = cdr(e); return T e == CONS ? cdr(car(e)) : err; }
I not(L x) { return T x == NIL; }
I let(L x) { return !not(x) && !not(cdr(x)); }
L eval(L,L),parse();
L evlis(L t,L e) { return T t == CONS ? cons(eval(car(t),e),evlis(cdr(t),e)) : T t == ATOM ? assoc(t,e) : nil; }
L f_eval(L t,L e) { return eval(car(evlis(t,e)),e); }
L f_quote(L t,L _) { return car(t); }
L f_cons(L t,L e) { return t = evlis(t,e),cons(car(t),car(cdr(t))); }
L f_car(L t,L e) { return car(car(evlis(t,e))); }
L f_cdr(L t,L e) { return cdr(car(evlis(t,e))); }
L f_add(L t,L e) { L n = car(t = evlis(t,e)); while (!not(t = cdr(t))) n += car(t); return num(n); }
L f_sub(L t,L e) { L n = car(t = evlis(t,e)); while (!not(t = cdr(t))) n -= car(t); return num(n); }
L f_mul(L t,L e) { L n = car(t = evlis(t,e)); while (!not(t = cdr(t))) n *= car(t); return num(n); }
L f_div(L t,L e) { L n = car(t = evlis(t,e)); while (!not(t = cdr(t))) n /= car(t); return num(n); }
L f_int(L t,L e) { L n = car(evlis(t,e)); return n-1e9 < 0 && n+1e9 > 0 ? (long)n : n; }
L f_lt(L t,L e) { return t = evlis(t,e),car(t) - car(cdr(t)) < 0 ? tru : nil; }
L f_eq(L t,L e) { return t = evlis(t,e),equ(car(t),car(cdr(t))) ? tru : nil; }
L f_pair(L t,L e) { L x = car(evlis(t,e)); return T x == CONS ? tru : nil; }
L f_or(L t,L e) { L x = nil; while (T t != NIL && not(x = eval(car(t),e))) t = cdr(t); return x; }
L f_and(L t,L e) { L x = tru; while (T t != NIL && !not(x = eval(car(t),e))) t = cdr(t); return x; }
L f_not(L t,L e) { return not(car(evlis(t,e))) ? tru : nil; }
L f_cond(L t,L e) { while (T t != NIL && not(eval(car(car(t)),e))) t = cdr(t); return eval(car(cdr(car(t))),e); }
L f_if(L t,L e) { return eval(car(cdr(not(eval(car(t),e)) ? cdr(t) : t)),e); }
L f_leta(L t,L e) { for (;let(t); t = cdr(t)) e = pair(car(car(t)),eval(car(cdr(car(t))),e),e); return eval(car(t),e); }
L f_lambda(L t,L e) { return closure(car(t),car(cdr(t)),e); }
L f_define(L t,L e) { env = pair(car(t),eval(car(cdr(t)),e),env); return car(t); }
struct { const char *s; L (*f)(L,L); } prim[] = {
{"eval", f_eval },{"car",f_car},{"-",f_sub},{"<",  f_lt },{"or", f_or },{"cond",f_cond},{"lambda",f_lambda},
{"quote",f_quote},{"cdr",f_cdr},{"*",f_mul},{"int",f_int},{"and",f_and},{"if",  f_if  },{"define",f_define},
{"cons", f_cons },{"+",  f_add},{"/",f_div},{"eq?",f_eq },{"not",f_not},{"let*",f_leta},{"pair?", f_pair  },{0}};
L bind(L v,L t,L e) { return T v == NIL ? e : T v == CONS ? bind(cdr(v),cdr(t),pair(car(v),car(t),e)) : pair(v,t,e); }
L reduce(L f,L t,L e) { return eval(cdr(car(f)),bind(car(car(f)),evlis(t,e),not(cdr(f)) ? env : cdr(f))); }
L apply(L f,L t,L e) { return T f == PRIM ? prim[ord(f)].f(t,e) : T f == CLOS ? reduce(f,t,e) : err; }
L eval(L x,L e) { return T x == ATOM ? assoc(x,e) : T x == CONS ? apply(eval(car(x),e),cdr(x),e) : x; }
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
L list() { L x; return scan() == ')' ? nil : !strcmp(buf, ".") ? (x = read(),scan(),x) : (x = parse(),cons(x,list())); }
L quote() { return cons(atom("quote"),cons(read(),nil)); }
L atomic() {
  L n; int i = strlen(buf);
  return isdigit(buf[*buf == '-']) && sscanf(buf,"%lg%n",&n,&i) && !buf[i] ? n : atom(buf);
}
L parse() { return *buf == '(' ? list() : *buf == '\'' ? quote() : atomic(); }
void print(L);
void printlist(L t) {
 for (putchar('('); ; putchar(' ')) {
  print(car(t));
  if (not(t = cdr(t))) break;
  if (T t) != CONS) { printf(" . "); print(t); break; }
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
