#include "main.h"
#include "flash_layout.h"
#include "bl_jump.h"


typedef void (*pFunction)(void);

void JumpToApplication(void)
{
    uint32_t appStack;
    uint32_t appResetHandler;
    pFunction appEntry;

    /* Read application stack pointer */
    appStack = *(volatile uint32_t*)APP_START_ADDR;		// lấy 4 byte đầu ở 0x0800 0000 nạp vào thanh ghi SP

    /* Read reset handler address */
    appResetHandler = *(volatile uint32_t*)(APP_START_ADDR + 4);	// lấy 4 byte kế nạp vào thanh ghi PC
    appEntry = (pFunction)appResetHandler;

    /* Disable interrupts */
    __disable_irq();

    /* Stop SysTick */
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;

    /* Set main stack pointer */
    __set_MSP(appStack);		// nạp 32bit/8 = 4 byte đầu ở địa chỉ APP_START_ADDR vào thanh ghi SP

    /* Jump to application reset handler */
    appEntry();		// ép thanh ghi PC nhảy tới địa chỉ APP_START_ADDR + 4
}
