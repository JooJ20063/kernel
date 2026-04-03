<<<<<<< HEAD
#ifndef ARCH_X86_64_PIC_H
#define ARCH_X86_64_PIC_H

#include <stdint.h>

void pic_remap(int offset1, int offset2);
=======
#ifndef PIC_H
#define PIC_H

#include <stdint.h>

void pic_remap(uint8_t offset1, uint8_t offset2);
>>>>>>> 4cc05da (Merge local changes post-PR)
void pic_send_eoi(uint8_t irq);
void pic_mask_all(void);
void pic_unmask_irq(uint8_t irq);

#endif
