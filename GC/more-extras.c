/* some more Lisp primitives to extend tinylisp-extras-gc.c */

/* ... prim[] = {
   ...
   {"=",        f_is,      0},
   {"%",        f_mod,     0},
   {"^",        f_exp,     0},
   {"<<",       f_lshift,  0},
   {">>",       f_rshift,  0},
   {"&",        f_bitand,  0},
   {"|",        f_bitor,   0},
   {"~",        f_bitxor,  0},
   {"abs",      f_abs,     0},
   {"neg",      f_neg,     0},
   {"sqrt",     f_sqrt,    0},
   {"sin",      f_sin,     0},
   {"cos",      f_cos,     0},
   {"tan",      f_tan,     0},
   {"asin",     f_asin,    0},
   {"acos",     f_acos,    0},
   {"atan",     f_atan,    0},
   {"atan2",    f_atan2,   0},
   {"floor",    f_floor,   0},
   {"ceiling",  f_ceiling, 0},
   {"char",     f_char,    0},
   {"code",     f_code,    0},
   {"list",     f_list,    0},
   {"append",   f_append,  0},
   {"length",   f_length,  0},
   {"nthcdr",   f_nthcdr,  0},
   {"nth",      f_nth,     0},
   {"last",     f_last,    0},
   {"reverse",  f_reverse, 0},
   {"seq",      f_seq,     0},
   {"range",    f_range,   0},
   {"equal?",   f_equal,   0},
   {"member",   f_member,  0},
   {"make-list",f_makelist,0},
   {"time",     f_time,    0},
   {0}};
*/

/* (= x y) returns #t if number x equals number y, otherwise returns () */
L f_is(L t,L *e) { I a = 0; L x = num(gc(evarg(&t,e,&a))); return x == num(gc(evarg(&t,e,&a))); }

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

/* (neg x) */
L f_neg(L t,L *e) { I a = 0; return num(-gc(evarg(&t,e,&a))); }

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

/* (char k [n])
   return a string of n (default n=1) characters with code -128 <= k <= 255 */
L f_char(L t,L *e) {
 I a = 0,k = (int)num(gc(evarg(&t,e,&a))),n = 1; L y;
 if (isarg(&t,e,&a,&y)) { n = num(gc(y)); if (n >= sizeof(buf)) n = sizeof(buf)-1; }
 buf[n] = '\0';
 while (n--) buf[n] = k;
 return atom(buf);
}

/* (code <atom> [n])
   return the code 0 to 255 of a single character in an atom at the front or at an optional given index n, returns 0 when beyond the end of the atom */
L f_code(L t,L *e) {
 I i,k,a = 0; L x,v = gc(evarg(&t,e,&a));
 k = T(v) == ATOM ? strlen(A+ord(v)) : 0;
 i = isarg(&t,e,&a,&x) ? (I)num(gc(x)) : 0; 
 return i < k ? *(A+ord(v)+i)&0xff : 0;
}

/* (list ...) - built-in for speed to replace the list definition in common.lisp (remove it)
   returns a list of its arguments - a built-in Lisp primitive of (define list (lambda args args)) */
L f_list(L t,L *e) { return evlis(t,*e); }

/* (append ...) - built-in for speed to replace the append definition in list.lisp (remove it)
   returns the concatenation of its list arguments as a new list */
L f_append(L t,L *e) {
 I a = 0; L x = nil,y,s,*p;
 for (rc(&y,nil),rc(&s,nil),p = &s; isarg(&t,e,&a,&x) && !not(t); )
  for (gc(y),y = x; !not(x); x = cdr(x),p = &CDR(*p)) *p = cons(dup(car(x)),nil);
 *p = x;
 rr(1); rg(y);
 return s;
}

/* (length t) - built-in for speed to replace the length definition in list.lisp (remove it)
   return the length of list t */
L f_length(L t,L *e) {
 I a = 0,k = 0; L s = evarg(&t,e,&a);
 for (t = s; T(s) == CONS; s = CDR(s)) ++k;
 gc(t);
 return k;
}

/* (nthcdr n t) - built-in for speed to replace the nthcdr definition in list.lisp (remove it)
   return n'th rest of the list t */
L f_nthcdr(L t,L *e) {
 I a = 0,i = (I)num(gc(evarg(&t,e,&a))); L s = evarg(&t,e,&a);
 for (t = s; i > 0; --i) t = cdr(t);
 t = dup(t);
 gc(s);
 return t;
}

/* (nth n t) - built-in for speed to replace the nth definition in list.lisp (remove it)
   return n'th item in list t */
L f_nth(L t,L *e) {
 L s = f_nthcdr(t,e),x = dup(car(s));
 gc(s);
 return x;
}

/* (last t [n])
   returns last singleton list element of list t, optionally return list of n last list elements */
L f_last(L t,L *e) {
 I a = 0; L s,x,y; int n;
 rc(&x,evarg(&t,e,&a));
 n = isarg(&t,e,&a,&y) ? (int)num(gc(y)) : 1;
 for (t = s = x; T(t) == CONS; t = CDR(t)) if (n < 1) s = CDR(s); else --n;
 s = dup(s);
 rg(x);
 return s;
}

/* (reverse t) - built-in for speed to replace the reverse definition in list.lisp (remove it)
   return reversed copy of list t */
L f_reverse(L t,L *e) {
 I a = 0; L x,s = nil;
 for (rc(&x,evarg(&t,e,&a)),t = x; T(t) == CONS; t = CDR(t)) s = cons(dup(CAR(t)),s);
 rg(x);
 return s;
}

/* (seq n) - built-in for speed to replace the seq definition in list.lisp (remove it)
   returns list with the sequence (1 2 3 ... n-1) */
L f_seq(L t,L *e) {
 I a = 0; int n = (int)num(gc(evarg(&t,e,&a))),m = (int)num(gc(evarg(&t,e,&a))); L s,*p;
 for (rc(&s,nil),p = &s; n < m; ++n,p = &CDR(*p)) *p = cons(n,nil);
 rr(1);
 return s;
}

/* (range n m k) - built-in for speed to replace the range definition in list.lisp (remove it)
   returns list with the sequence (n n+k n+2k ... m-1) where optional k=1 by default */
L f_range(L t,L *e) {
 I a = 0; int n = (int)num(gc(evarg(&t,e,&a))),m = (int)num(gc(evarg(&t,e,&a))),k = 1; L x,s,*p;
 if (isarg(&t,e,&a,&x)) k = (int)num(gc(x));
 for (rc(&s,nil),p = &s; k*m > k*n; n += k,p = &CDR(*p)) *p = cons(n,nil);
 rr(1);
 return s;
}

/* (equal? x y) - built-in for speed to replace the equal? definition in list.lisp (remove it)
   deep check for equality, does not permit cyclic data structures */
I equal(L x,L y) {
 if (equ(x,y)) return 1;
 if (T(x) != T(y) || (T(x) != CONS && T(x) != CLOS && T(x) != MACR)) return 0;
 for (; T(x) == T(y) && (T(x) == CONS || T(x) == CLOS || T(x) == MACR); x = CDR(x),y = CDR(y))
  if (!equal(CAR(x),CAR(y))) return 0;
 return equal(x,y);
}
L f_equal(L t,L *e) {
 I a = 0; L x,y,z;
 rc(&x,evarg(&t,e,&a));
 y = evarg(&t,e,&a);
 z = equal(x,y) ? tru : nil;
 gc(y); rg(x);
 return z;
}

/* (member x t) - built-in for speed to replace the member definition in list.lisp (remove it)
   returns rest of list t from the first list element that is equal to x */
L f_member(L t,L *e) {
 I a = 0; L s,x;
 rc(&x,evarg(&t,e,&a));
 s = t = evarg(&t,e,&a);
 while (T(t) == CONS && !equal(x,CAR(t))) t = CDR(t);
 t = dup(t);
 gc(s); rg(x);
 return t;
}

/* (make-list n [x]) - built-in for speed to replace the make-list definition in list.lisp (remove it)
   returns list of n copies of optional x, x is () by default */
L f_makelist(L t,L *e) {
 I a = 0; int n = (int)num(gc(evarg(&t,e,&a))); L s = nil,x = nil;
 isarg(&t,e,&a,&x);
 while (n-- > 0) s = cons(dup(x),s);
 gc(x);
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
