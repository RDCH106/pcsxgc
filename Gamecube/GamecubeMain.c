/*  Pcsx - Pc Psx Emulator
 *  Copyright (C) 1999-2002  Pcsx Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <gccore.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <fat.h>
#include "PsxCommon.h"
#include "PlugCD.h"

/* function prototypes */
int SysInit();
void SysReset();
void SysClose();
void SysPrintf(char *fmt, ...);
void *SysLoadLibrary(char *lib);
void *SysLoadSym(void *lib, char *sym);
char *SysLibError();
void SysCloseLibrary(void *lib);
void SysUpdate();
void SysRunGui();
void SysMessage(char *fmt, ...);

static u32* xfb[2] = { NULL, NULL };	/*** Framebuffers ***/
GXRModeObj *vmode;				/*** Graphics Mode Object ***/
void ScanPADSandReset() { PAD_ScanPads(); }
static void Initialise (void){
  static int whichfb = 0;        /*** Frame buffer toggle ***/
  VIDEO_Init();
  PAD_Init();
  PAD_Reset(0xf0000000);

  
  vmode = VIDEO_GetPreferredMode(NULL);
    
  VIDEO_Configure (vmode);
  xfb[0] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (&TVPal528IntDf)); //assume PAL largest
  xfb[1] = (u32 *) MEM_K0_TO_K1 (SYS_AllocateFramebuffer (&TVPal528IntDf));	//fixme for progressive?
  console_init (xfb[0], 20, 64, vmode->fbWidth, vmode->xfbHeight,
        vmode->fbWidth * 2);
  VIDEO_ClearFrameBuffer (vmode, xfb[0], COLOR_BLACK);
  VIDEO_ClearFrameBuffer (vmode, xfb[1], COLOR_BLACK);
  VIDEO_SetNextFramebuffer (xfb[0]);
  //VIDEO_SetPostRetraceCallback (PAD_ScanPads);
  VIDEO_SetPostRetraceCallback (ScanPADSandReset);
  VIDEO_SetBlack (0);
  VIDEO_Flush ();
  VIDEO_WaitVSync ();        /*** Wait for VBL ***/
  if (vmode->viTVMode & VI_NON_INTERLACE)
    VIDEO_WaitVSync ();

}

// Plugin structure
#include "GamecubePlugins.h"
PluginTable plugins[] =
	{ PLUGIN_SLOT_0,
	  PLUGIN_SLOT_1,
	  PLUGIN_SLOT_2,
	  PLUGIN_SLOT_3,
	  PLUGIN_SLOT_4,
	  PLUGIN_SLOT_5,
	  PLUGIN_SLOT_6,
	  PLUGIN_SLOT_7 
};


/* draw background */
void draw_splash(void)
{
   printf("Splash Screen");
}

long LoadCdBios;

int main(int argc, char *argv[]) {
/*	char *file = NULL;
	int runcd = 0;
	int loadst = 0;
	int i;
*/
	Initialise();
	fatInitDefault();
    draw_splash();
	
	/* Configure pcsx */
	memset(&Config, 0, sizeof(PcsxConfig));
	strcpy(Config.Bios, "SCPH1001.BIN"); // Use actual BIOS
	strcpy(Config.BiosDir, "PSXISOS/");
	strcpy(Config.Net,"Disabled");

	Config.Cpu = 1;	//interpreter

//	Config.CdTiming = 0;	//no longer used
	Config.PsxOut = 1;
	Config.HLE = 1;
    SysPrintf("start main()\r\n");

	if (SysInit() == -1) 
	{
		printf("SysInit() Error!\n");
		while(1);
	}

	/* Start gui */
//	menu_start();

	
	SysReset();
	
	CDR_open();
    SysPrintf("CheckCdrom()\r\n");
	CheckCdrom();
	

//	SysPrintf("Load()\r\n");
//	Load("/rd/pdx-dlcm.psx");
	LoadCdrom();		//code dies here if we try to execute cd-rom
//	while(1);
    SysPrintf("Execute()\r\n");
	psxCpu->Execute();

	return 0;
}

int SysInit() {
    SysPrintf("start SysInit()\r\n");

    SysPrintf("psxInit()\r\n");
	psxInit();

    SysPrintf("LoadPlugins()\r\n");
	LoadPlugins();
    SysPrintf("LoadMcds()\r\n");
	LoadMcds(Config.Mcd1, Config.Mcd2);

	SysPrintf("end SysInit()\r\n");
	return 0;
}

void SysReset() {
    SysPrintf("start SysReset()\r\n");
	psxReset();
	SysPrintf("end SysReset()\r\n");
}

void SysClose() {
	psxShutdown();
	ReleasePlugins();

	if (emuLog != NULL) fclose(emuLog);
}

void SysPrintf(char *fmt, ...) {
	va_list list;
	char msg[512];

	va_start(list, fmt);
	vsprintf(msg, fmt, list);
	va_end(list);

	if (Config.PsxOut) printf ("%s", msg);
#if defined (CPU_LOG) || defined(DMA_LOG) || defined(CDR_LOG) || defined(HW_LOG) || \
	defined(BIOS_LOG) || defined(GTE_LOG) || defined(PAD_LOG)
	fprintf(emuLog, "%s", msg);
#endif
}

void *SysLoadLibrary(char *lib) {
	int i;
	for(i=0; i<NUM_PLUGINS; i++)
		if((plugins[i].lib != NULL) && (!strcmp(lib, plugins[i].lib)))
			return (void*)i;
	return NULL;
}

void *SysLoadSym(void *lib, char *sym) {
	PluginTable* plugin = plugins + (int)lib;
	int i;
	for(i=0; i<plugin->numSyms; i++)
		if(plugin->syms[i].sym && !strcmp(sym, plugin->syms[i].sym))
			return plugin->syms[i].pntr;
	return NULL;
}

char *SysLibError() {
	return NULL;
}

void SysCloseLibrary(void *lib) {
//	dlclose(lib);
}

int framesdone = 0;
void SysUpdate() {
	printf("Executed %i frames\n",framesdone);
	framesdone++;
//	PADhandleKey(PAD1_keypressed());
//	PADhandleKey(PAD2_keypressed());
}

void SysRunGui() {
//	RunGui();
}

void SysMessage(char *fmt, ...) {
	
}