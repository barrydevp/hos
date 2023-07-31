/* Syntactic details of assembler.  */

#ifdef _CET_ENDBR
# define _CET_NOTRACK notrack
#else
# define _CET_ENDBR
# define _CET_NOTRACK
#endif

#define CALL_MCOUNT		/* Do nothing.  */

/* ELF uses byte-counts for .align, most others use log2 of count of bytes.  */
#define ALIGNARG(log2) 1 << log2
#define ASM_SIZE_DIRECTIVE(name) .size name, .- name;

/* Define an entry point visible from C.  */
#define ENTRY(name)                                                            \
  .globl name;                                                                 \
  .type name, @function;                                                       \
  .align ALIGNARG(4);                                                          \
  name##:                                                                      \
  .cfi_startproc;                                                              \
  _CET_ENDBR;

#undef END
#define END(name)                                                              \
  .cfi_endproc;                                                                \
  ASM_SIZE_DIRECTIVE(name) CALL_MCOUNT
