#!/bin/bash

cat <<EOF | gcc -xc -c -o tmp2.o -
int ret3() { return 3; }
int ret5() { return 5; }
int add(int x, int y) { return x+y; }
int sub(int x, int y) { return x-y; }

int add6(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; }
EOF

assert() {
  expected="$1"
  input="$2"

  ./ncc "$input" > tmp.s
  #gcc -static -o tmp tmp.s tmp2.o
  gcc -o tmp tmp.s
  ./tmp
  actual="$?"

  if [ "$actual" = "$expected" ]; then
    echo "$input => $actual"
  else
    echo "$input => $expected expected, but got $actual"
    exit 1
  fi
}

assert 0 'int main() { return 0; }'
assert 42 'int main() { return 42; }'
assert 21 'int main() { return 5+20-4; }'
assert 41 'int main() { return 12 + 34 - 5 ; }'
assert 47 'int main() { return 5+6*7; }'
assert 15 'int main() { return 5*(9-6); }'
assert 4 'int main() { return (3+5)/2; }'
assert 10 'int main() { return -10+20; }'
assert 10 'int main() { return - -10; }'
assert 10 'int main() { return - - +10; }'

assert 0 'int main() { return 0==1; }'
assert 1 'int main() { return 42==42; }'
assert 1 'int main() { return 0!=1; }'
assert 0 'int main() { return 42!=42; }'

assert 1 'int main() { return 0<1; }'
assert 0 'int main() { return 1<1; }'
assert 0 'int main() { return 2<1; }'
assert 1 'int main() { return 0<=1; }'
assert 1 'int main() { return 1<=1; }'
assert 0 'int main() { return 2<=1; }'

assert 1 'int main() { return 1>0; }'
assert 0 'int main() { return 1>1; }'
assert 0 'int main() { return 1>2; }'
assert 1 'int main() { return 1>=0; }'
assert 1 'int main() { return 1>=1; }'
assert 0 'int main() { return 1>=2; }'

assert 3 'int main() { 1; 2; return 3; }'

assert 3 'int main() { int a; a=3; return a; }'
assert 8 'int main() { int a; int z; a=3; z=5; return a+z; }'
assert 6 'int main() { int a; int b; a=b=3; return a+b; }'
assert 3 'int main() { int foo; foo=3; return foo; }'
assert 8 'int main() { int foo123; foo123=3; int bar; bar =5;return foo123 +bar; }'
assert 2 'int main() { int _foo_bar; _foo_bar =1; return 1+_foo_bar; }'

assert 1 'int main() { return 1; }'
assert 6 'int main() { int a; int b; a=2;b=3;return a*b; }'
assert 4 'int main() { int a; a=1; return a+ 3; 1; }'
assert 3 'int main() { int a; int b; a=6; b=2; return(a/b); }'

assert 3 'int main() { if (0) return 2; return 3; }'
assert 3 'int main() { if (1-1) return 2; return 3; }'
assert 2 'int main() { if (1) return 2; return 3; }'
assert 2 'int main() { if (2>1) return 2; else return 3; }'
assert 3 'int main() { if (0) return 2; else return 3; }'
assert 4 'int main() { if (0) return 2; else if (1-1) return 3; else return 4; }'

assert 55 'int main() { int i; int j; i=0; j=0; for (i-0; i<=10; i=i+1) j=i+j; return j; }'
assert 3 'int main() { for (;;) return 3; return 5; }'
assert 1 'int main() { int i; for (i=0; i < 5; i=i+1) if (i==3) return 1; return 5; }'

assert 10 'int main() { int i; i=0; while(i<10) i = i+1; return i; }'

assert 3 'int main() { {1; {2;} return 3;} }'

assert 5 'int main() { ;;; return 5; }'

assert 3 'int main() { int x; x=3; return *&x; }'
assert 3 'int main() { int x; int *y; int **z; x=3; y=&x; z=&y; return **z; }'
assert 5 'int main() { int x; int y; x=3; y=5; return *(&x+1); }'
assert 3 'int main() { int x; int y; x=3; y=5; return *(&y-1); }'
assert 5 'int main() { int x; int *y; x=3; y=&x; *y=5; return x; }'
assert 7 'int main() { int x; int y; x=3; y=5; *(&x+1)=7; return y; }'
assert 7 'int main() { int x; int y; x=3; y=5; *(1+&x)=7; return y; }'
assert 7 'int main() { int x; int y; x=3; y=5; *(&y-1)=7; return x; }'
assert 7 'int main() { int x; int y; x=3; y=5; *(&y-1)=7; return x; }'
assert 1 'int main() { int x; int y; x=3; y=5; return &y - &x; }'

assert 1 'int main() { int x =1; x; }'
assert 5 'int main() { int x=3; int *y=&x; *y=5; return x; }'

assert 2 'int main() { int x,y; x=3; y=5; return y-x; }'
assert 2 'int main() { int x=3, y=5; return y-x; }'

assert 3 'int ret3() { return 3; } int main() { return ret3(); }'
assert 5 'int ret5() { return 5; } int main() { return ret5(); }'

assert 8 'int add(int x, int y) { return x + y; } int main() { return add(3, 5); }'
assert 2 'int sub(int x, int y) { return x - y; } int main() { return sub(5, 3); }'
assert 21 'int add6(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; } int main() { return add6(1,2,3,4,5,6); }'
assert 66 'int add6(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; } int main() { return add6(1,2,add6(3,4,5,6,7,8),9,10,11); }'
assert 136 'int add6(int a, int b, int c, int d, int e, int f) { return a+b+c+d+e+f; }int main() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }'

assert 3 'int main() { int x[2]; int *y=&x; *y=3; return *x; }'

assert 3 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *x; }'
assert 4 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+1); }'
assert 5 'int main() { int x[3]; *x=3; *(x+1)=4; *(x+2)=5; return *(x+2); }'

assert 3 'int main() { int x[3]; x[0]=3; x[1]=4; x[2]=5; return x[0]; }'
assert 4 'int main() { int x[3]; x[0]=3; x[1]=4; x[2]=5; return x[1]; }'
assert 5 'int main() { int x[3]; x[0]=3; x[1]=4; x[2]=5; return x[2]; }'
assert 5 'int main() { int x[3]; x[0]=3; x[1]=4; x[2]=5; return x[1+1]; }'
assert 3 'int main() { int x[3]; x[0]=3; x[1]=4; x[2]=5; return 0[x]; }'

assert 0 'int main() { int x[2][3]; int *y=x; *y=0; return **x; }'
assert 1 'int main() { int x[2][3]; int *y=x; *(y+1)=1; return *(*x+1); }'
assert 2 'int main() { int x[2][3]; int *y=x; *(y+2)=2; return *(*x+2); }'
assert 3 'int main() { int x[2][3]; int *y=x; *(y+3)=3; return **(x+1); }'
assert 4 'int main() { int x[2][3]; int *y=x; *(y+4)=4; return *(*(x+1)+1); }'
assert 5 'int main() { int x[2][3]; int *y=x; *(y+5)=5; return *(*(x+1)+2); }'

echo OK
