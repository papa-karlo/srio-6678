/**********************************************************
*	===== drvcfg.c =====
*	Drivers configuration file
*
***********************************************************/

#include <dzy/drv.h>

/**********************************************************
*  Device Table Array
***********************************************************/
extern  DRV_Fxn HIO_Fxn;
DEVICE  DEV_HIO = { "hio", &HIO_Fxn, (void*)0 };

extern  DRV_Fxn HOST_Fxn;
DEVICE  DEV_HOST = { "host", &HOST_Fxn, (void*)0 };

extern  DRV_Fxn HFIFO_Fxn;
DEVICE	DEV_HFIFO      = { "hfifo", &HFIFO_Fxn, (void*)0 };





DEVICE	*DEV_TAB[]=
{
	(DEVICE*)&DEV_HIO,
	(DEVICE*)&DEV_HOST,
	(DEVICE*)&DEV_HFIFO,

	(DEVICE*)0
};

/*
*  End of File
*/

