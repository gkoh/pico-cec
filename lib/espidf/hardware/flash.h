#define XIP_BASE 0x100
#define FLASH_PAGE_SIZE 16
#define FLASH_SECTOR_SIZE 512


uint32_t save_and_disable_interrupts();
void flash_range_erase(int address, int size);
void flash_range_program(int address, uint8_t * a, int size);
