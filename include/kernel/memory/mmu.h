#pragma once

#define mmu_page_is_user_readable(p) (p->bits.user)
#define mmu_page_is_user_writable(p) (p->bits.writable)
