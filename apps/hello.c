#define __NR_call0 20
#define __NR_call1 21
#define __NR_print 22

/// @brief Heart of the code that calls a system call with 0 parameters.
#define __inline_syscall0(res, name)                                           \
  __asm__ __volatile__("int $0x80" : "=a"(res) : "0"(__NR_##name))

/// @brief Heart of the code that calls a system call with 1 parameter.
#define __inline_syscall1(res, name, arg1)                                     \
  __asm__ __volatile__("push %%ebx ; movl %2,%%ebx ; int $0x80 ; pop %%ebx"    \
                       : "=a"(res)                                             \
                       : "0"(__NR_##name), "ri"((int)(arg1))                   \
                       : "memory");

/// @brief Handle the value returned from a system call.
/// @param type Specifies the type of the returned value.
/// @param res  The name of the variable where the result of the SC is stored.
#define __syscall_return(type, res)                                            \
  do { return (type)(res); } while (0)

/// @brief System call with 0 parameters.
#define _syscall0(type, name)                                                  \
  type name(void) {                                                            \
    long __res;                                                                \
    __inline_syscall0(__res, name);                                            \
    __syscall_return(type, __res);                                             \
  }

/// @brief System call with 1 parameter.
#define _syscall1(type, name, type1, arg1)                                     \
  type name(type1 arg1) {                                                      \
    long __res;                                                                \
    __inline_syscall1(__res, name, arg1);                                      \
    __syscall_return(type, __res);                                             \
  }

_syscall0(int, call0)
_syscall1(int, call1, int, arg)
_syscall1(int, print, const char*, arg)

int main() {
  // printf("Hello World From Userspace!\n");
  char s[] = "0";

  print("Hello World From User Space!\n");
  int a = call0();
  s[0] = (char)(a & 0xff);
  print(s);
  int b = call1(a+1);
  s[0] = (char)(b & 0xff);
  print(s);

  return 0;
}
