#ifndef MAIN_H
#define MAIN_H

struct menuItem {
	const char* name;
	fp function;
};

void showMenu(menuItem menu[],int count);
void quit();
void dumpCID();
void restoreNAND();
void backupNAND();
void chk();
void backupWifi();
void backupSettings();
void backupBIOS();
void backupFirmware();
void dummy();
void clearStatus();
int readJEDEC();
int saveToFile(const char *filename,u8 *buffer,size_t size);
extern "C" {
	bool nand_ReadSectors(sec_t sector, sec_t numSectors,void* buffer);
	bool nand_WriteSectors(sec_t sector, sec_t numSectors,const void* buffer); //!!!
}
#endif
