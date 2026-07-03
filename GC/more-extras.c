/* some more Lisp primitives to extend tinylisp-extras-gc.c */

/* ... prim[] = {
   ...
   {"%",       f_mod,    0},
   {"^",       f_exp,    0},
   {"<<",      f_lshift, 0},
   {">>",      f_rshift, 0},
   {"&",       f_bitand, 0},
   {"|",       f_bitor,  0},
   {"~",       f_bitxor, 0},
   {"abs",     f_abs,    0},
   {"sqrt",    f_sqrt,   0},
   {"sin",     f_sin,    0},
   {"cos",     f_cos,    0},
   {"tan",     f_tan,    0},
   {"asin",    f_asin,   0},
   {"acos",    f_acos,   0},
   {"atan",    f_atan,   0},
   {"atan2",   f_atan2,  0},
   {"floor",   f_floor,  0},
   {"ceiling", f_ceiling,0},
   {"char",    f_char,   0},
   {"code",    f_code,   0},
   {"list",    f_list,   0},
   {"append",  f_append, 0},
   {"time",    f_time,   0},
   {0}};
*/

/* (% x y ...) modulo of dividing x by y, then by ... */
L f_mod(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(gc(evarg(&t,e,&a))); while (isarg(&t,e,&a,&x)) n %= (int64_t)num(gc(x)); return n; }

/* (^ x y ...) raise x to the power y then by ... */
L f_exp(L t,L *e) { I a = 0; L x,n = gc(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n = pow(n,gc(x)); return num(n); }

/* (<< x y ...) shift signed integer x left by y and by ... */
L f_lshift(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(gc(evarg(&t,e,&a))); while (isarg(&t,e,&a,&x)) n <<= (int64_t)num(gc(x)); return n; }

/* (>> x y ...) shift signed integer x right by y and by ... */
L f_rshift(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(gc(evarg(&t,e,&a))); while (isarg(&t,e,&a,&x)) n >>= (int64_t)num(gc(x)); return n; }

/* (& x y ...) bitwise and signed integers */
L f_bitand(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(gc(evarg(&t,e,&a))); while (isarg(&t,e,&a,&x)) n &= (int64_t)num(gc(x)); return n; }

/* (| x y ...) bitwise or signed integers */
L f_bitor(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(gc(evarg(&t,e,&a))); while (isarg(&t,e,&a,&x)) n |= (int64_t)num(gc(x)); return n; }

/* (~ x y ...) bitwise xor signed integers */
L f_bitxor(L t,L *e) { I a = 0; L x; int64_t n = (int64_t)num(gc(evarg(&t,e,&a))); while (isarg(&t,e,&a,&x)) n ^= (int64_t)num(gc(x)); return n; }

/* (abs x) */
L f_abs(L t,L *e) { I a = 0; return num(fabs(gc(evarg(&t,e,&a)))); }

/* (sqrt x) */
L f_sqrt(L t,L *e) { I a = 0; return num(sqrt(gc(evarg(&t,e,&a)))); }

/* (sin x) */
L f_sin(L t,L *e) { I a = 0; return num(sin(gc(evarg(&t,e,&a)))); }

/* (cos x) */
L f_cos(L t,L *e) { I a = 0; return num(cos(gc(evarg(&t,e,&a)))); }

/* (tan x) */
L f_tan(L t,L *e) { I a = 0; return num(tan(gc(evarg(&t,e,&a)))); }

/* (asin x) */
L f_asin(L t,L *e) { I a = 0; return num(asin(gc(evarg(&t,e,&a)))); }

/* (acos x) */
L f_acos(L t,L *e) { I a = 0; return num(acos(gc(evarg(&t,e,&a)))); }

/* (atan x) */
L f_atan(L t,L *e) { I a = 0; return num(atan(gc(evarg(&t,e,&a)))); }

/* (atan2 x y) */
L f_atan2(L t,L *e) { I a = 0; L x,n = gc(evarg(&t,e,&a)); while (isarg(&t,e,&a,&x)) n = atan2(n,gc(x)); return num(n); }

/* (floor x) */
L f_floor(L t,L *e) { I a = 0; return num(floor(gc(evarg(&t,e,&a)))); }

/* (ceiling x) */
L f_ceiling(L t,L *e) { I a = 0; return num(ceil(gc(evarg(&t,e,&a)))); }

/* (char n)
   return a single character atom for the given character code -128 <= n <= 255 */
L f_char(L t,L *e) { I a = 0; *buf = (int)num(gc(evarg(&t,e,&a))); buf[1] = '\0'; return atom(buf); }

/* (code <atom> [n])
   return the code 0 to 255 of a single character in an atom at the front or at an optional given index n, returns 0 when beyond the end of the atom */
L f_code(L t,L *e) {
 I i,k,a = 0; L x,v = gc(evarg(&t,e,&a));
 k = T(v) == ATOM ? strlen(A+ord(v)) : 0;
 i = isarg(&t,e,&a,&x) ? (I)num(x) : 0; 
 gc(x);
 return i < k ? *(A+ord(v)+i)&0xff : 0;
}

/* (list ...) - built-in for speed to replace the list definition in common.lisp (remove it)
   returns a list of its arguments - a built-in Lisp primitive of (define list (lambda args args)) */
L f_list(L t,L *e) { return evlis(t,*e); }

/* (append ...) - built-in for speed to replace the append definition in list.lisp (remove it)
   returns the concatenation of its list arguments as a new list */
L f_append(L t,L *e) {
 I a = 0; L x = nil,y,s,*p = &s;
 rc(&y,nil); rc(&s,nil);
 while (isarg(&t,e,&a,&x) && !not(t))
  for (gc(y),y = x; !not(x); x = cdr(x),p = &CDR(*p)) *p = cons(dup(car(x)),nil);
 *p = x;
 gc(y); rr(2);
 return s;
}

/* (time <expr> [n]) 
   elapsed running time to evaluate an expression, optionally executed n times to improve measurement accuracy */
#include <sys/time.h>
L f_time(L t,L *e) {
 L x = nil; I i,k = let(t) ? (I)num(car(CDR(t))) : 1;
 struct timeval tv0, tv1;
 float ms;
 gettimeofday(&tv0, NULL);
 for (i = 0; i < k; ++i) {
  gc(x);
  x = eval(car(t),*e);
 }
 gettimeofday(&tv1, NULL);
 ms = tv1.tv_usec;
 ms -= tv0.tv_usec;
 ms = 1000.0 * (tv1.tv_sec - tv0.tv_sec) + ms/1000.0;
 if (ms < 0.0) ms += 60000.0;
 printf("\nelapsed time is %g ms\n",ms/k);
 return x;
}
