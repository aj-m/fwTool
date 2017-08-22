#include <nds.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fat.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include "main.hpp"

#define MAX_SIZE	(1*1024*1024)

#define CONSOLE_SCREEN_WIDTH 32
#define CONSOLE_SCREEN_HEIGHT 24

extern "C" {
	bool nand_ReadSectors(sec_t sector, sec_t numSectors,void* buffer);
	bool nand_WriteSectors(sec_t sector, sec_t numSectors,const void* buffer); //!!!
}

// Global variable declarations
int menuTop = 5, statusTop = 18; // What are these used for? Screen row offsets?
u8 *firmware_buffer;
size_t userSettingsOffset, fwSize, wifiOffset, wifiSize;
char dirname[15] = "FW";
char serial[13];
u32 sysid=0;
u32 ninfo=0;
u32 sizMB=0;
char nand_type[20]={0};
char nand_dump[80]={0};
char nand_rest[80]={0};
bool quitting = false;
// Global declarations end

/*
struct menuItem {
	const char* name;
	fp function;
};
*/
int file_exists(char const* file){
	struct stat st;
	if(stat(file, &st) == 0)
		return 1;
	return 0;
}

int saveToFile(const char *filename, u8 *buffer, size_t size) {
	FILE *f = fopen(filename, "wb");
	if (NULL==f) return -1;
	size_t written = fwrite(buffer, 1, size, f);
	fclose(f);
	if (written != size) return -2;
	return 0;
}

int readJEDEC() {
	fifoSendValue32(FIFO_USER_01, 1);
	fifoWaitValue32(FIFO_USER_01);
	return fifoGetValue32(FIFO_USER_01);
}

void clearStatus() {
	iprintf("\x1b[%d;0H\x1b[J\x1b[15;0H",statusTop); 
	iprintf("                                ");    //clean up after previous residents
	iprintf("                                ");
	iprintf("                                ");
	iprintf("                                ");
	iprintf("\x1b[%d;0H\x1b[J\x1b[15;0H",statusTop);
}

void dummy() {
	clearStatus();
	iprintf("\x1b[%d;6HNOT IMPLEMENTED!",statusTop+3);
}

void backupFirmware() {
	clearStatus();
	readFirmware(0, firmware_buffer, fwSize);
	if (saveToFile("firmware.bin", firmware_buffer, fwSize) < 0) {
		iprintf("Error saving firmware!\n");
		return;
	}
	iprintf("Firmware saved as\n\n%s/firmware.bin", dirname );
}

void backupBIOS() {
	int dumpcmd = 0;
	clearStatus();
	const char *arm7file, *arm9file;
	size_t arm7size, arm9size;

	if (isDSiMode()) {
		arm7file = "bios7i.bin";
		arm7size = 64 * 1024;
		arm9file = "bios9i.bin";
		arm9size = 64 * 1024;
		dumpcmd = 3;
	} else {
		arm7file = "bios7.bin";
		arm7size = 16 * 1024;
		arm9file = "bios9.bin";
		arm9size = 32 * 1024;
		dumpcmd = 2;
	}

	if (saveToFile(arm9file, (u8*)0xffff0000, arm9size ) < 0) {
		iprintf("Error saving arm9 bios\n");
		return;
	}

	fifoSendValue32(FIFO_USER_01, dumpcmd);
	fifoSendValue32(FIFO_USER_01, (u32)firmware_buffer);
	fifoWaitValue32(FIFO_USER_01);
	fifoGetValue32(FIFO_USER_01);

	if (saveToFile(arm7file, firmware_buffer, arm7size) < 0 ) {
		iprintf("Error saving arm7 bios\n");
		return;
	}
	iprintf("BIOS saved as\n\n%1$s/%2$s\n%1$s/%3$s", dirname, arm7file, arm9file );
}


void backupSettings() {
	clearStatus();
	readFirmware(userSettingsOffset, firmware_buffer + userSettingsOffset, 512);

	if (saveToFile("UserSettings.bin", firmware_buffer + userSettingsOffset, 512) < 0) {
		iprintf("Error saving settings1!\n");
		return;
	}
	iprintf("User settings saved as\n\n%s/UserSettings.bin", dirname );
}

void backupWifi() {
	clearStatus();
	readFirmware(wifiOffset, firmware_buffer + wifiOffset, wifiSize);

	if (saveToFile("WifiSettings.bin", firmware_buffer + wifiOffset, wifiSize) < 0) {
		iprintf("Error saving Wifi settings!\n");
		return;
	}
	iprintf("Wifi settings saved as\n\n%s/WifiSettings.bin", dirname );
}

void chk() {
	
	nand_ReadSectors(0 , 1 , firmware_buffer);
	memcpy(&sysid, firmware_buffer + 0x100, 4);
	memcpy(&ninfo, firmware_buffer + 0x104, 4);
	
	if     (ninfo==0x00200000){sizMB=943; strcpy(nand_type,"nand_o3ds.bin");} //old3ds
	else if(ninfo==0x00280000){sizMB=1240;strcpy(nand_type,"nand_n3ds.bin");} //new3ds
	else if(sysid!=0x4453434E){sizMB=240; strcpy(nand_type,"nand_dsi.bin");}  //dsi
	else                      {sizMB=0;   strcpy(nand_type,"");}              //not recognized, do nothing
	sprintf(nand_dump,"Dump    %s",nand_type);
	sprintf(nand_rest,"Restore %s",nand_type);
	
}

void backupNAND() {
	clearStatus();

	if (!isDSiMode()) {
		iprintf("Not a DSi or 3ds!\n");
		return;
	} 

	FILE *f = fopen(nand_type, "wb");

	if (NULL == f) {
		iprintf("failure creating %s\n", nand_type);
		return;
	} 

	iprintf("Writing %s/%s\n\n", dirname, nand_type);
	size_t i;
	size_t sectors = 128;
	size_t blocks = (sizMB * 1024 * 1024) / (sectors * 512);
	for (i=0; i < blocks; i++) {
		if(!nand_ReadSectors(i * sectors,sectors,firmware_buffer)) {
			iprintf("\nError reading NAND!\n");
			break;
		}
		size_t written = fwrite(firmware_buffer, 1, 512 * sectors, f);
		if(written != 512 * sectors) {
			iprintf("\nError writing to SD!\n");
			break;
		}
		iprintf("Block %d of %d\r", i+1, blocks);
	}
	fclose(f);
}

void restoreNAND() {
	clearStatus();

	if (!isDSiMode()) {
		iprintf("Not a DSi or 3ds!\n");
		return;
	}
	
	FILE *f = fopen(nand_type, "rb");
	
	if (f == NULL) {
		iprintf("Failure opening %s\n", nand_type);
		return;
	}
	
	//Sanity checks
	//	Size check
	fseek(f, 0, SEEK_END);
	size_t dump_size = ftell(f);
	if ( dump_size != (sizMB * 1024 * 1024) ) {
		iprintf("%s and NAND sizes\ndo not match.\nOperation aborted.", nand_type);
		fclose(f);
		return;
	}
	rewind(f);
	//	MBR(decrypted image) check
	//	Taken from TWLtool (https://github.com/WinterMute/twltool)
	struct {
		u8 code[446];
		struct {
			u8  status;
			u8  start_chs[3];
			u8  partition_type;
			u8  end_chs[3];
			u32 start_sector;
			u32 num_sectors;
		} __attribute__((__packed__)) partition[4];
		u8 signature[2];
	} mbr;
	fread(&mbr, 1, 0x200, f);
	if(mbr.signature[0] == 0x55 || mbr.signature[1] == 0xAA) {
		iprintf("Found MBR in %s.\nImage is not encrypted.\nOperation aborted.", nand_type);
		fclose(f);
		return;
	}
	rewind(f);
	//Sanity checks end
	
	iprintf("Sure? NAND restore is DANGEROUS!");
	iprintf("START + SELECT confirm\n");
	iprintf("B to cancel\n");
	
	while (1) {
		scanKeys();
		int keys = keysHeld();
		if ((keys & KEY_START) && (keys & KEY_SELECT))
			break; // Do it
		if (keys & KEY_B) {
			clearStatus();
			fclose(f);
			return; // Abort!
		}
		swiWaitForVBlank();
	}
	
	clearStatus();
	
	iprintf("Reading %s/%s\n\n", dirname, nand_type);
	size_t i;
	size_t sectors = 128;
	size_t blocks = (sizMB * 1024 * 1024) / (sectors * 512);
	for (i=0; i < blocks; i++) {
		
		size_t read = fread(firmware_buffer, 1, 512 * sectors, f);
		
		if(read != 512 * sectors) {
			iprintf("\nError reading SD!\n");
			break;
		}
		
		if(!nand_WriteSectors(i * sectors,sectors,firmware_buffer)) {
			iprintf("\nError writing NAND!\n");
			break;
		}
		
		iprintf("%d/%d DON'T poweroff!\r", i+1, blocks);
	}
	fclose(f);
	
	clearStatus();
	iprintf("Restore %s success.\nYou may now Exit and restart\nyour console.",nand_type);
}

void dumpCID(){
	clearStatus();
	u8 *CID=(u8*)0x2FFD7BC;
	
	// Returns 0 on success, so !saveToFile means it saved ok
	if(!saveToFile("CID.bin",CID,16)) 
		iprintf("CID dumped!\n");
	else 
		iprintf("CID dump failed!\n");
}

void quit() {
	quitting = true;
	powerOn(PM_BACKLIGHT_TOP);
}

struct menuItem mainMenu[] = {
	{ "Exit", quit },
	{ "Backup Firmware", backupFirmware } ,
	{ "Dump Bios", backupBIOS } ,
	{ "Backup User Settings", backupSettings } ,
	{ "Backup Wifi Settings", backupWifi } ,
	{ "Dump CID", dumpCID} ,
	{ nand_dump , backupNAND},
	{ nand_rest , restoreNAND}
/*
	TODO

	{ "Restore Firmware", dummy } ,
	{ "Restore User Settings", dummy } ,
	{ "Restore Wifi Settings", dummy } ,
*/	
};

void showMenu(menuItem menu[], int count) {
	int i;
	for (i=0; i<count; i++ ) {
		iprintf("\x1b[%d;5H%s", i + menuTop, menu[i].name);
	}
}

int main() {
	defaultExceptionHandler();
	/*
	// This doesn't do much right now. Simply ensures top screen doesn't remain white. :P
	videoSetMode(MODE_0_2D | DISPLAY_BG0_ACTIVE);
	vramSetBankA (VRAM_A_MAIN_BG_0x06000000);
	REG_BG0CNT = BG_MAP_BASE(0) | BG_COLOR_256 | BG_TILE_BASE(2);
	BG_PALETTE[0]=0;
	BG_PALETTE[255]=0xffff;
	u16* bgMapTop = (u16*)SCREEN_BASE_BLOCK(0);
	for (int i = 0; i < CONSOLE_SCREEN_WIDTH*CONSOLE_SCREEN_HEIGHT; i++) {
		bgMapTop[i] = (u16)i;
	}
	*/
	// Turn off top screen backlight. fwtool doesn't use topscreen for anything right now.
	powerOff(PM_BACKLIGHT_TOP);
	
	consoleDemoInit();

	if (!fatInitDefault()) {
		printf("FAT init failed!\n");
		return 0; // Short-circuit out to make the rest of this easy on the eyes
	}
	// FAT init success
	// Setup firmware buffer
	iprintf("DS(i) firmware tool %s\n",VERSION);
	firmware_buffer = (u8 *)memalign(32,MAX_SIZE); // Allocate 1MB on an address divisible by 32
	readFirmware(0, firmware_buffer, 512);

	// Get MAC address, print to screen and add to directory name buffer
	// 0x36 == MAC address offset in firmware
	iprintf("\x1b[2;0HMAC ");
	for (int i=0; i<6; i++) {
		printf("%02X", firmware_buffer[0x36+i]);
		sprintf(&dirname[2+(2*i)],"%02X",firmware_buffer[0x36+i]);
		if (i < 5) printf(":");
	}

	// Null-terminate the directory name
	dirname[14] = 0;
	// Create directory if DNE and enter it
	mkdir(dirname, 0777);
	chdir(dirname);

	userSettingsOffset = (firmware_buffer[32] + (firmware_buffer[33] << 8)) *8;

	fwSize = userSettingsOffset + 512;

	iprintf("\n%dK flash, jedec %X", fwSize/1024,readJEDEC());

	wifiOffset = userSettingsOffset - 1024;
	wifiSize = 1024;

	if ( firmware_buffer[29] == 0x57 ) {
		wifiOffset -= 1536;
		wifiSize += 1536;
	}

	int count = sizeof(mainMenu) / sizeof(menuItem);
	
	chk();
	showMenu(mainMenu, count);

	int selected = 0;
	quitting = false;
	// menu loop
	while(!quitting) {
		iprintf("\x1b[%d;3H]\x1b[23C[",selected + menuTop);
		swiWaitForVBlank();
		scanKeys();
		int keys = keysDownRepeat();
		iprintf("\x1b[%d;3H \x1b[23C ",selected + menuTop);
		if ( (keys & KEY_UP))
			selected--;
		if (selected < 0)
			selected = count - 1;
		if ( (keys & KEY_DOWN))
			selected++;
		if (selected >= count)
			selected = 0;
		if ( keys & KEY_A )
			mainMenu[selected].function();
	}

	return 0;
}

