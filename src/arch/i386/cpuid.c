#include <arch/i386/cpuid.h>
#include <kernel/string.h>

void get_cpuid(cpuinfo_t *cpuinfo) {
  pt_regs ereg;

  ereg.eax = ereg.ebx = ereg.ecx = ereg.edx = 0;
  cpuid_write_vendor(cpuinfo, &ereg);

  ereg.eax = 1;
  ereg.ebx = ereg.ecx = ereg.edx = 0;
  cpuid_write_proctype(cpuinfo, &ereg);
}

void call_cpuid(pt_regs *registers) {
  __asm__("cpuid\n\t"
          : "=a"(registers->eax), "=b"(registers->ebx), "=c"(registers->ecx),
            "=d"(registers->edx)
          : "a"(registers->eax));
}

void cpuid_write_vendor(cpuinfo_t *cpuinfo, pt_regs *registers) {
  call_cpuid(registers);

  cpuinfo->cpu_vendor[0]  = (char)((registers->ebx & 0x000000FF));
  cpuinfo->cpu_vendor[1]  = (char)((registers->ebx & 0x0000FF00) >> 8);
  cpuinfo->cpu_vendor[2]  = (char)((registers->ebx & 0x00FF0000) >> 16);
  cpuinfo->cpu_vendor[3]  = (char)((registers->ebx & 0xFF000000) >> 24);
  cpuinfo->cpu_vendor[4]  = (char)((registers->edx & 0x000000FF));
  cpuinfo->cpu_vendor[5]  = (char)((registers->edx & 0x0000FF00) >> 8);
  cpuinfo->cpu_vendor[6]  = (char)((registers->edx & 0x00FF0000) >> 16);
  cpuinfo->cpu_vendor[7]  = (char)((registers->edx & 0xFF000000) >> 24);
  cpuinfo->cpu_vendor[8]  = (char)((registers->ecx & 0x000000FF));
  cpuinfo->cpu_vendor[9]  = (char)((registers->ecx & 0x0000FF00) >> 8);
  cpuinfo->cpu_vendor[10] = (char)((registers->ecx & 0x00FF0000) >> 16);
  cpuinfo->cpu_vendor[11] = (char)((registers->ecx & 0xFF000000) >> 24);
  cpuinfo->cpu_vendor[12] = '\0';
}

void cpuid_write_proctype(cpuinfo_t *cpuinfo, pt_regs *registers) {
  call_cpuid(registers);

  uint32_t type = cpuid_get_byte(registers->eax, 0xB, 0x3);

  switch (type) {
    case 0:
      cpuinfo->cpu_type = "Original OEM Processor";
      break;
    case 1:
      cpuinfo->cpu_type = "Intel Overdrive Processor";
      break;
    case 2:
      cpuinfo->cpu_type = "Dual processor";
      break;
    case 3:
      cpuinfo->cpu_type = "(Intel reserved bit)";
      break;
  }

  uint32_t familyID   = cpuid_get_byte(registers->eax, 0x7, 0xE);
  cpuinfo->cpu_family = familyID;

  if (familyID == 0x0F) {
    cpuinfo->cpu_family += cpuid_get_byte(registers->eax, 0x13, 0xFF);
  }

  uint32_t model     = cpuid_get_byte(registers->eax, 0x3, 0xE);
  cpuinfo->cpu_model = model;

  if (familyID == 0x06 || familyID == 0x0F) {
    uint32_t ext_model = cpuid_get_byte(registers->eax, 0xF, 0xE);
    cpuinfo->cpu_model += (ext_model << 4);
  }
  cpuinfo->apic_id = cpuid_get_byte(registers->ebx, 0x17, 0xFF);

  cpuid_feature_ecx(cpuinfo, registers->ecx);
  cpuid_feature_edx(cpuinfo, registers->edx);

  /* Get brand string to identify the processor */
  if (familyID >= 0x0F && model >= 0x03) {
    cpuinfo->brand_string = cpuid_brand_string(registers);
  } else {
    cpuinfo->brand_string = cpuid_brand_index(registers);
  }
}

void cpuid_feature_ecx(cpuinfo_t *cpuinfo, uint32_t ecx) {
  uint32_t temp = ecx;
  uint32_t i;

  for (i = 0; i < ECX_FLAGS_SIZE; ++i) {
    temp                        = cpuid_get_byte(temp, i, 1);
    cpuinfo->cpuid_ecx_flags[i] = temp;
    temp                        = ecx;
  }
}

void cpuid_feature_edx(cpuinfo_t *cpuinfo, uint32_t edx) {
  uint32_t temp = edx;
  uint32_t i;

  for (i = 0; i < EDX_FLAGS_SIZE; ++i) {
    temp                        = cpuid_get_byte(temp, i, 1);
    cpuinfo->cpuid_edx_flags[i] = temp;
    temp                        = edx;
  }
}

inline uint32_t cpuid_get_byte(uint32_t reg, uint32_t position,
                               uint32_t value) {
  return ((reg >> position) & value);
}

char *cpuid_brand_index(pt_regs *f) {
  char *indexes[21] = { "Reserved",
                        "Intel Celeron",
                        "Intel Pentium III",
                        "Intel Pentium III Xeon",
                        "Mobile Intel Pentium III",
                        "Mobile Intel Celeron",
                        "Intel Pentium 4",
                        "Intel Pentium 4",
                        "Intel Celeron",
                        "Intel Xeon MP",
                        "Intel Xeon MP",
                        "Mobile Intel Pentium 4",
                        "Mobile Intel Celeron",
                        "Mobile Genuine Intel",
                        "Intel Celeron M",
                        "Mobile Intel Celeron",
                        "Intel Celeron",
                        "Mobile Genuine Intel",
                        "Intel Pentium M",
                        "Mobile Intel Celeron",
                        NULL };

  int bx = (f->ebx & 0xFF);

  if (bx > 0x17) {
    bx = 0;
  }

  return indexes[bx];
}

char *cpuid_brand_string(pt_regs *f) {
  char *temp = "";

  for (f->eax = 0x80000002; f->eax <= 0x80000004; (f->eax)++) {
    f->ebx = f->ecx = f->edx = 0;
    call_cpuid(f);
    temp = strncat(temp, (const char *)f->eax, strlen((const char *)f->eax));
    temp = strncat(temp, (const char *)f->ebx, strlen((const char *)f->ebx));
    temp = strncat(temp, (const char *)f->ecx, strlen((const char *)f->ecx));
    temp = strncat(temp, (const char *)f->edx, strlen((const char *)f->edx));
  }

  return temp;
}
