#include <interrupts/interrupts.h>

char *exceptionString[32] = {
    "Division by zero exception",
    "Debug exception",
    "Non maskable interrupt",
    "Breakpoint exception",
    "Into detected overflow",
    "Out of bounds exception",
    "Invalid opcode exception",
    "No coprocessor exception",
    "Double fault exception",
    "Coprocessor segment overrun",
    "Bad TSS",
    "Segment not present",
    "Stack fault",
    "General protection fault",
    "Page fault",
    "Unknown interrupt exception",
    "Coprocessor fault",
    "Alignment check exception",
    "Machine check exception",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved",
    "Reserved"
};

//Timer Tick
void timerTickHandler(registers_t regs) {
    _increaseTTCounter();
}

//Keyboard
void keyboardHandler(registers_t regs) {
	handleScanCode(_port_in(0x60));
}

void systemCallHandler(int sysCallNumber, void ** args) {
	switch(sysCallNumber) {
		case SYSTEM_WRITE:
			sysWrite((int) args[0], args[1], (int)args[2]);
			break;
		case SYSTEM_READ:
			sysRead((int) args[0], args[1],(int)args[2]);
			break;
	}
}

// This gets called from our ASM interrupt handler stub.
void isr_handler(registers_t regs) {
    
    if (regs.int_no < 32) {
        panic(exceptionString[regs.int_no], 1, true);
    }
    /*
    if (interruptHandlers[regs.int_no] != 0) {
        isr_t handler = interruptHandlers[regs.int_no];
        handler(regs);
    }
    */
}

// This gets called from our ASM interrupt handler stub.
void irq_handler(registers_t regs) {
    // Send an EOI (end of interrupt) signal to the PICs.
    // If this interrupt involved the slave.
    if (regs.int_no >= 40) {
        // Send reset signal to slave.
        _port_out(0xA0, 0x20);
    }
    // Send reset signal to master. (As well as slave, if necessary).
    _port_out(0x20, 0x20);

    if (interruptHandlers[regs.int_no] != 0)
    {
        isr_t handler = interruptHandlers[regs.int_no];
        handler(regs);
    }
}