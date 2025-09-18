void undef_handler(void)          { while(1); }
void prefetch_abort_handler(void) { while(1); }
void data_abort_handler(void)     { while(1); }
void reserved_handler(void)       { while(1); }
void fiq_handler(void)            { while(1); }

/* IRQ/SWI stubs call your C handlers in scheduler */
extern void swi_handler(void);
extern void timer0_irq_handler(void);