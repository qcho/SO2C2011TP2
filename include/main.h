#ifndef MAIN_H
#define MAIN_H

#include <defs.h>
#include <fs.h>
#include <shell.h>
#include <multiboot.h>
#include <interrupts/interrupts.h>
#include <driver/video.h>
#include <asm/libasm.h>
#include <io.h>
#include <lib/stdio.h>
#include <interrupts/interruptDefs.h>
#include <driver/ata_disk.h>

int getNextPID();

int getCurrPID();
/* __write
*
* Recibe como parametros:
* - File Descriptor
* - Buffer del source
* - Cantidad
*
**/
u32int __write(int fd, const void* buffer, u32int count);

/* __read
*
* Recibe como parametros:
* - File Descriptor
* - Buffer a donde escribir
* - Cantidad
*
**/
u32int __read(int fd, void* buffer, u32int count);

#endif

