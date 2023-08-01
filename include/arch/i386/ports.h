#include <kernel/types.h>

static inline unsigned char inportb(unsigned short _port) {
  unsigned char rv;
  asm volatile("inb %1, %0" : "=a"(rv) : "dN"(_port));
  return rv;
}

static inline void outportb(unsigned short _port, unsigned char _data) {
  asm volatile("outb %1, %0" : : "dN"(_port), "a"(_data));
}

static inline unsigned short inportw(unsigned short _port) {
  unsigned short rv;
  asm volatile("inw %1, %0" : "=a"(rv) : "dN"(_port));
  return rv;
}

static inline void outportw(unsigned short _port, unsigned short _data) {
  asm volatile("outw %1, %0" : : "dN"(_port), "a"(_data));
}

static inline unsigned int inportl(unsigned short _port) {
  unsigned int rv;
  asm volatile("inl %%dx, %%eax" : "=a"(rv) : "dN"(_port));
  return rv;
}

static inline void outportl(unsigned short _port, unsigned int _data) {
  asm volatile("outl %%eax, %%dx" : : "dN"(_port), "a"(_data));
}

static inline unsigned short inports(unsigned short _port) {
  unsigned short rv;
  asm volatile("inw %1, %0" : "=a"(rv) : "dN"(_port));
  return rv;
}

static inline void outports(unsigned short _port, unsigned short _data) {
  asm volatile("outw %1, %0" : : "dN"(_port), "a"(_data));
}

static inline void inportsw(uint16_t portid, void *addr, size_t count) {
  asm volatile("rep insw" : "+D"(addr), "+c"(count) : "d"(portid) : "memory");
}

static inline void outportsw(uint16_t portid, const void *addr, size_t count) {
  asm volatile("rep outsw" : "+S"(addr), "+c"(count) : "d"(portid));
}
