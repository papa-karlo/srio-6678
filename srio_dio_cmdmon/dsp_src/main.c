//#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <c6x.h>
/* CSL Chip Functional Layer */
#include <ti/csl/csl_chip.h>
/* PSC CSL Include Files */
#include <ti/csl/csl_psc.h>
#include <ti/csl/csl_pscAux.h>
/* CSL SRIO Functional Layer */
#include <ti/csl/csl_srio.h>
#include <ti/csl/csl_srioAux.h>
#include <ti/csl/csl_srioAuxPhyLayer.h>

/* CSL Modules */
#include <ti/csl/csl_bootcfg.h>
#include <ti/csl/csl_bootcfgAux.h>
#include <ti/csl/csl_tsc.h>


#include <dzy/stdio.h>
#include <dzy/ctype.h>
#include <dzy/drv.h>
#include <prf/sys6678.h>

#define MAX_MSG_LEN 128

extern CSL_SrioHandle      hSrio;
extern SrioDevice_init();

/* These are the device identifiers used in the Example Application */
const uint32_t DEVICE_ID1_16BIT    = 0xBEEF;
const uint32_t DEVICE_ID1_8BIT     = 0xAB;
const uint32_t DEVICE_ID2_16BIT    = 0x4560;
const uint32_t DEVICE_ID2_8BIT     = 0xCD;
const uint32_t DEVICE_ID3_16BIT    = 0x1234;
const uint32_t DEVICE_ID3_8BIT     = 0x12;
const uint32_t DEVICE_ID4_16BIT    = 0x5678;
const uint32_t DEVICE_ID4_8BIT     = 0x56;

/** These are the possible values for SRIO lane mode */
typedef enum
{
  srio_lanes_form_four_1x_ports = 0,             /**< SRIO lanes form four 1x ports */
  srio_lanes_form_one_2x_port_and_two_1x_ports,  /**< SRIO lanes form one 2x port and two 1x ports */
  srio_lanes_form_two_1x_ports_and_one_2x_port,  /**< SRIO lanes form two 1x ports and one 2x port */
  srio_lanes_form_two_2x_ports,                  /**< SRIO lanes form two 2x ports */
  srio_lanes_form_one_4x_port                    /**< SRIO lanes form one 4x port */
} srioLanesMode_e;

int verbose_flag = 0;

/**
 *  @b Description
 *  @n
 *      Wait for configured SRIO ports to show operational status.
 *
  *  @param[in]  hSrio
 *      SRIO Handle for the CSL Functional layer.
 *  @param[in]  laneMode
 *      Mode number to determine which ports to check for operational status.
 *
 *  @retval
 *      Success - 0
 */
int32_t waitAllSrioPortsOperational (CSL_SrioHandle hSrio, srioLanesMode_e laneMode)
{
	Uint8		port;
	Uint8		portsMask=0xF;		// 0b1111
	char		statusText[4]="";
    uint64_t	tscTemp;

    /* Set port mask to use based on the lane mode specified */
	switch (laneMode)
	{
		case srio_lanes_form_four_1x_ports:					/* check ports 0 to 3 */
			portsMask = 0xF;	// 0b1111
			break;
		case srio_lanes_form_one_2x_port_and_two_1x_ports:	/* check ports 0, 2, and 3 */
			portsMask = 0xD;	// 0b1101
			break;
		case srio_lanes_form_two_1x_ports_and_one_2x_port:	/* check ports 0, 1, and 2 */
			portsMask = 0x7;	// 0b0111
			break;
		case srio_lanes_form_two_2x_ports:					/* check ports 0 and 2 */
			portsMask = 0x5;	// 0b0101
			break;
		case srio_lanes_form_one_4x_port:					/* check port 0 */
			portsMask = 0x1;	// 0b0001
			break;
		default: /* check ports 0 to 3 */
			portsMask = 0xF;	// 0b1111
			break;
	}

    /* Wait for all SRIO ports for specified lane mode to be operational */
	if( verbose_flag) printf ("Debug: Waiting for SRIO ports to be operational...  \n");
	tscTemp = CSL_tscRead () + 5000000000;
    for (port = 0; port < 4; port++)
    {
    	if ((portsMask >> port) & 0x1 == 1)
    	{
    		sprintf (statusText, "NOT ");
    		/* Check for good port status on each valid port, timeout if not received after 5 seconds */
    	    while (CSL_tscRead() < tscTemp)
    	    {
				if (CSL_SRIO_IsPortOk (hSrio, port) == TRUE)
				{
					sprintf (statusText,"");
					break;
				}
    	    }

    	    if( verbose_flag) printf ("Debug: SRIO port %d is %soperational.\n", port, statusText);
    		tscTemp = CSL_tscRead() + 1000000000;
    	}
    }

    return 0;
}

/*
 *  @b Description
 *  @n
 *      This function enables the power/clock domains for SRIO.
 *
 *  @retval
 *      Not Applicable.
 */
static Int32 enable_srio (void)
{
    /* SRIO power domain is turned OFF by default. It needs to be turned on before doing any
     * SRIO device register access. This not required for the simulator. */

    /* Set SRIO Power domain to ON */
    CSL_PSC_enablePowerDomain (CSL_PSC_PD_SRIO);

    /* Enable the clocks too for SRIO */
    CSL_PSC_setModuleNextState (CSL_PSC_LPSC_SRIO, PSC_MODSTATE_ENABLE);

    /* Start the state transition */
    CSL_PSC_startStateTransition (CSL_PSC_PD_SRIO);

    /* Wait until the state transition process is completed. */
    while (!CSL_PSC_isStateTransitionDone (CSL_PSC_PD_SRIO));

    /* Return SRIO PSC status */
    if ((CSL_PSC_getPowerDomainState(CSL_PSC_PD_SRIO) == PSC_PDSTATE_ON) &&
        (CSL_PSC_getModuleState (CSL_PSC_LPSC_SRIO) == PSC_MODSTATE_ENABLE))
    {
        /* SRIO ON. Ready for use */
        return 0;
    }
    else
    {
        /* SRIO Power on failed. Return error */
        return -1;
    }
}

/**
 *  @b Description
 *  @n
 *      The function is used to get the status of the SRIO lane configuration.
 *      Allows user to confirm setting of the lane configuration against the
 *      actual status shown for the lane configuration.
 *
 *  @param[in]  hSrio
 *      SRIO Handle for the CSL Functional layer.
 *
 *  @retval
 *      Success - 0
 *  @retval
 *      Error   - <0 (Invalid status)
 */
int32_t displaySrioLanesStatus (CSL_SrioHandle hSrio)
{
	SRIO_LANE_STATUS	ptrLaneStatus;
	Int8				i;
	uint32_t			laneCode=0;
	int32_t				returnStatus=0;
	char				laneSetupText[MAX_MSG_LEN];

   	for (i = 3; i >= 0; i--)
   	{
   		CSL_SRIO_GetLaneStatus (hSrio,i,&ptrLaneStatus);
   		laneCode = laneCode | (ptrLaneStatus.laneNum << (i * 4));
   	}

	switch (laneCode)
	{
		case 0x0000:	/* four 1x ports */
			sprintf (laneSetupText,"four 1x ports");
			break;
		case 0x0010:	/* one 2x port and two 1x ports */
			sprintf (laneSetupText,"one 2x port and two 1x ports");
			break;
		case 0x1000:	/* two 1x ports and one 2x port */
			sprintf (laneSetupText,"two 1x ports and one 2x port");
			break;
		case 0x1010:	/* two 2x ports */
			sprintf (laneSetupText,"two 2x ports");
			break;
		case 0x3210:	/* one 4x port */
			sprintf (laneSetupText,"one 4x port");
			break;
		default: /* four 1x ports */
			sprintf (laneSetupText,"INVALID LANE COMBINATION FOUND");
			returnStatus = -1;
			break;
	}
	printf ("Lanes status shows lanes formed as %s%s", laneSetupText, "\n");

	return returnStatus;
}

/**
 *  @b Description
 *  @n
 *      The function is used to set the SRIO lane configuration.
 *      Prerequisite: Run setEnableSrioPllRxTx() first or it will overwrite the MSYNC setting done
 *                    by this function.
 *
 *  @param[in]  hSrio
 *      SRIO Handle for the CSL Functional layer.
 *  @param[in]  laneMode
 *      Mode number to set the lanes to a specific configuration.
 *
 *  @retval
 *      Success - 0
 *  @retval
 *      Error   - <0 (Invalid lane configuration mode specified)
 */
int32_t setSrioLanes (CSL_SrioHandle hSrio, srioLanesMode_e laneMode)
{
	Uint8	port, pathMode;
	Uint8	bootCompleteFlag;
	Uint32	serdesTXConfig;
	Uint32	msyncSet = 0x00100000;
	CSL_SRIO_GetBootComplete (hSrio, &bootCompleteFlag);

    if (bootCompleteFlag == 1)
    	/* Set boot complete to be 0; to enable writing to the SRIO registers. */
		CSL_SRIO_SetBootComplete (hSrio, 0);

    /* Set the path mode number to the lane configuration specified */
	switch (laneMode)
	{
		case srio_lanes_form_four_1x_ports:					/* four 1x ports (forms ports: 0,1,2 and 3) */
			pathMode = 0;
			break;
		case srio_lanes_form_one_2x_port_and_two_1x_ports:	/* one 2x port and two 1x ports (forms ports: 0, 2 and 3) */
			pathMode = 1;
			break;
		case srio_lanes_form_two_1x_ports_and_one_2x_port:	/* two 1x ports and one 2x port (forms ports: 0, 1 and 2) */
			pathMode = 2;
			break;
		case srio_lanes_form_two_2x_ports:					/* two 2x ports (forms ports: 0 and 2) */
			pathMode = 3;
			break;
		case srio_lanes_form_one_4x_port:					/* one 4x port (forms port: 0) */
			pathMode = 4;
			break;
		default:	/* Invalid lane configuration mode specified */
		    return -1;
	}

    /* Configure the path mode for all ports. */
    for (port = 0; port < 4; port++)
    {
        /* Configure the path mode for the port. */
        CSL_SRIO_SetPLMPortPathControlMode (hSrio, port, pathMode);

        /* Get the current SERDES TX config */
        CSL_BootCfgGetSRIOSERDESTxConfig (port,&serdesTXConfig);

        /* Determine the MSYNC bit's setting according to laneMode being used */
    	switch (laneMode)
    	{
    		case srio_lanes_form_four_1x_ports:						/* four 1x ports (forms ports: 0,1,2 and 3) */
    			msyncSet = 0x00100000;
    			break;
    		case srio_lanes_form_one_2x_port_and_two_1x_ports:		/* one 2x port and two 1x ports (forms ports: 0, 2 and 3) */
    			msyncSet = (port != 1 ? 0x00100000 : 0xFFEFFFFF);
    			break;
    		case srio_lanes_form_two_1x_ports_and_one_2x_port:		/* two 1x ports and one 2x port (forms ports: 0, 1 and 2) */
    			msyncSet = (port != 3 ? 0x00100000 : 0xFFEFFFFF);
    			break;
    		case srio_lanes_form_two_2x_ports:						/* two 2x ports (forms ports: 0 and 2) */
    			msyncSet = (((port != 1) && (port != 3)) ? 0x00100000 : 0xFFEFFFFF);
    			break;
    		case srio_lanes_form_one_4x_port:						/* one 4x port (forms port: 0) */
    			msyncSet = (port == 0 ? 0x00100000 : 0xFFEFFFFF);
    			break;
    		default:	/* Invalid lane configuration mode specified */
    		    return -1;
    	}

    	/* Set msync for each port according to the lane mode (port width) specified */
    	if (msyncSet == 0x00100000)
        	serdesTXConfig |= msyncSet;				/* Set MSYNC bit */
        else
        	serdesTXConfig &= msyncSet;				/* Clear MSYNC bit */

    	/* Write SERDES TX MSYNC bit */
		CSL_BootCfgSetSRIOSERDESTxConfig (port, serdesTXConfig);
    }

    if (bootCompleteFlag == 1)
		/* Set boot complete back to 1; configuration is complete. */
		CSL_SRIO_SetBootComplete (hSrio, 1);

    return 0;
}

uint8_t		main_deviceID = 0;

//int read_flag = 0;
//int write_flag = 0;
//int maint_flag = 0;
//int loop_flag = 0;
//int idle_flag = 0;

int speed = 1;



int dbg_flag = 0;
int board_id = 1;

uint8_t	dest_id = 0;
uint8_t	ttype = 0;
uint8_t	ftype = 0;
uint8_t	hop_count = 0;
uint32_t LSU = 0;
volatile uint32_t SRC = 0x10860000;		// address of src

volatile uint32_t dest_adr = 0; 	// address of dest

void	print_usage(void)
{
	printf("\n");
	printf("SRIO Command Monitor Application for C6678 modules\n");
	printf("Keys:  -h, -H, -?            -- this message\n");
	printf("       -d<DD>, -D<DD>        -- main SRIO Device ID DD(hex-8bit)\n");
	printf("       -b<N>, -B<N>          -- board id N (dec)\n");
	printf("       -v, -V                -- verbose\n");
}

/*********************** dbg_printf ********************
****************************************************/
int     dbg_printf( const char *format, ... )
{
	if(dbg_flag==0) return 0;

	va_list arglist;
	int		ret;

	va_start( arglist, format );
	ret = vfprintf( stdout, format, arglist );
	va_end(arglist);
	return ret;
}

///////////////////////////////////////////////////////////////
// Command functions
///////////////////////////////////////////////////////////////

typedef int CmdFunc( char *cmdStr );
typedef struct { char cmd[16]; CmdFunc *func; } CmdEntry;

int	parse_word(char *word, char *cmdbuf)
{
	int i; 			// cmdbuf counter
	int cmdCnt=0; 	// cmd counter
	int	first = 0;	// first space flag

	for(i=0;i<32;i++) {
		char ch;
		ch = cmdbuf[i];
		if (isalnum (ch)) { word[cmdCnt++] = ch; first++; }
		else if(first!=0) {
			word[cmdCnt++] = 0;
			break;
		}
	}
	if(i==32) return -1;	// bad command string

	return i;
}

int rdFunc(char *cmdStr)
{
	dbg_printf("RD: %s\n", cmdStr);

	uint32_t	adr;
	char		adrStr[128];

	int 		len=parse_word(adrStr,cmdStr);
	if( len < 0) {
		printf("### rdFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	char *end;
	adr = strtoul(adrStr, &end, 16);

	printf("RD: 0x%08lX = 0x%08lX\n", adr, MEM(adr));
	return 0;
}

int dumpFunc(char *cmdStr)
{
	dbg_printf("DUMP: %s\n", cmdStr);

	uint32_t	adr;
	char		adrStr[128];
	uint32_t	size;

	int 		len=parse_word(adrStr,cmdStr);
	if( len < 0) {
		printf("### dumpFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	char *end;
	adr = strtoul(adrStr, &end, 16);

	len=parse_word(adrStr,&cmdStr[len+1]);
	if( len < 0) {
		printf("### dumpFunc: parse_word(,'%s') error\n", &cmdStr[len+1]);
		return -1;
	}
	size = strtoul(adrStr, &end, 10);
	if(size>1024) size=1024;

	printf("DUMP: 0x%08lX ... 0x%08lX\n", adr, adr+(4*size));
	int i;
	for(i=0;i<size;i+=4) {
		printf("0x%08lX: 0x%08lX  ", adr+4*i, MEM(adr+4*i));
		if((i+1)>=size) break;
		printf("0x%08lX  ", MEM(adr+4*(i+1)));
		if((i+2)>=size) break;
		printf("0x%08lX  ", MEM(adr+4*(i+2)));
		if((i+3)>=size) break;
		printf("0x%08lX  ", MEM(adr+4*(i+3)));
		printf("\n");
	}
	printf("\n");

	return 0;
}

int wrFunc(char *cmdStr)
{
	dbg_printf("WR: %s\n", cmdStr);

	uint32_t	adr;
	uint32_t	val;
	char		str[128];
	int 		len;

	char *pp = &cmdStr[0];
	if( (len=parse_word(str,cmdStr)) < 0) {
		printf("### wrFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	char *end;
	adr = strtoul(str, &end, 16);

	pp = &cmdStr[len+1];
	if( (len=parse_word(str,pp)) < 0) {
		printf("### wrFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	val = strtoul(str, &end, 16);

	printf("WR: 0x%08lX = 0x%08lX\n", adr, val);

	MEM(adr) = val;

	return 0;
}

int fillFunc(char *cmdStr)
{
	dbg_printf("FILL: %s\n", cmdStr);

	uint32_t	adr;
	uint32_t	size;
	uint32_t	val;
	char		str[128];

	char *pp = &cmdStr[0];
	int 		len1;
	if( (len1=parse_word(str,cmdStr)) < 0) {
		printf("### fillFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	char *end;
	adr = strtoul(str, &end, 16);

	pp = &cmdStr[len1+1];
	int 		len2;
	if( (len2=parse_word(str,pp)) < 0) {
		printf("### fillFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	size = strtoul(str, &end, 10);
	if(size>1024) size=1024;

	pp = &cmdStr[len1+len2+2];
	if( (len2=parse_word(str,pp)) < 0) {
		printf("### fillFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	val = strtoul(str, &end, 16);

	printf("FILL: 0x%08lX ... 0x%08lX = 0x%08lX\n", adr, adr+(4*size), val);

	int i;
	for(i=0;i<size;i++) {
		MEM(adr+4*i) = val;
	}

	return 0;
}

int	quitFunc(char *cmdStr)
{
	dbg_printf("QUIT\n");
	exit(0);
	return 0;
}

int dbgFunc(char *cmdStr)
{
	dbg_printf("DBG\n");
	dbg_flag++;
	dbg_flag&=1;

	return 0;
}

int hopFunc(char *cmdStr)
{
	dbg_printf("HOP_COUNT: %s\n", cmdStr);

	uint8_t		val;
	char		str[128];

	if( (parse_word(str,cmdStr)) < 0) {
		printf("### hopFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	val = atol(str);
	printf("HOP_COUNT: value = %d\n", val);
	hop_count = val;
	return 0;
}

int lsuFunc(char *cmdStr)
{
	dbg_printf("LSU: %s\n", cmdStr);

	uint8_t		val;
	char		str[128];

	if( (parse_word(str,cmdStr)) < 0) {
		printf("### rdFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	val = atol(str);
	printf("LSU: value = %d\n", val);
	return 0;
}

int nreadFunc(char *cmdStr)
{
	dbg_printf("NREAD: %s\n", cmdStr);

	uint32_t	destAdr;
	uint8_t		destId;

	char		str[128];
	int 		len;

	char *pp = &cmdStr[0];
	if( (len=parse_word(str,cmdStr)) < 0) {
		printf("### wrFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	char *end;
	destId = strtoul(str, &end, 16);

	pp = &cmdStr[len+1];
	if( (len=parse_word(str,pp)) < 0) {
		printf("### wrFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	destAdr = strtoul(str, &end, 16);

	*((uint32_t *)SRC) =  0x01020304;

	/* Make sure there is space in the Shadow registers to write*/
	while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);
	// NWRITE Packet Type
	CSL_SRIO_SetLSUReg0 (hSrio, LSU, 0); //no rapidio MSB
	CSL_SRIO_SetLSUReg1 (hSrio, LSU, destAdr);
	CSL_SRIO_SetLSUReg2 (hSrio, LSU, SRC);
	CSL_SRIO_SetLSUReg3 (hSrio, LSU, 0x4, 0); //write 4 bytes, no doorbell
	CSL_SRIO_SetLSUReg4 (hSrio, LSU,
		destId,// destid
		0,      // src id map = 0, using RIO_DEVICEID_REG0
		0, 		//1,      // id size = 1 for 16bit device IDs
		0,      // outport id = 0
		0,      // priority = 0
		0,      // xambs = 0
		0,      // suppress good interrupt = 0 (don't care about interrupts)
		0);     // interrupt request = 0
	CSL_SRIO_SetLSUReg5 (hSrio, LSU,
		4,//ttype,
		2,//ftype,
		hop_count,
		0); 	// doorbell = 0

	printf("NREAD (ID=0x%02lX, HOP=%d):  0x%08lX = 0x%08lX\n", destId, hop_count, destAdr, *((uint32_t *)SRC));

	volatile static int timeout = 0;
	while( *((int32_t *)SRC) == 0x01020304) {
		timeout++;
		if(timeout>10000000) {
			printf("### Timeout Error\n");
			return -1;
		}
	}

	printf("NREAD (ID=0x%02lX, HOP=%d):  0x%08lX = 0x%08lX\n", destId, hop_count, destAdr, *((uint32_t *)SRC));

	return 0;
}

///////////////////////////////////////////////////////////////
////////// nwriteFunc() ///////////////////////////////////////
///////////////////////////////////////////////////////////////
int nwriteFunc(char *cmdStr)
{
	dbg_printf("NWRITE: %s\n", cmdStr);

	uint32_t	destAdr;
	uint8_t		destId;
	uint32_t	val;

	char		str[128];
	int 		len1;

	char *pp = &cmdStr[0];
	if( (len1=parse_word(str,cmdStr)) < 0) {
		printf("### nwriteFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	char *end;
	destId = strtoul(str, &end, 16);

	int 		len2;
	pp = &cmdStr[len1+1];
	if( (len2=parse_word(str,pp)) < 0) {
		printf("### nwriteFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	destAdr = strtoul(str, &end, 16);

	int len3;
	pp = &cmdStr[len1+len2+2];
	if( (len3=parse_word(str,pp)) < 0) {
		printf("### nwriteFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	val = strtoul(str, &end, 16);
	*((uint32_t *)SRC) =  val;

	/* Make sure there is space in the Shadow registers to write*/
	while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);
	// NWRITE Packet Type
	CSL_SRIO_SetLSUReg0 (hSrio, LSU, 0); //no rapidio MSB
	CSL_SRIO_SetLSUReg1 (hSrio, LSU, destAdr);
	CSL_SRIO_SetLSUReg2 (hSrio, LSU, SRC);
	CSL_SRIO_SetLSUReg3 (hSrio, LSU, 0x4, 0); //write 4 bytes, no doorbell
	CSL_SRIO_SetLSUReg4 (hSrio, LSU,
		destId,// destid
		0,      // src id map = 0, using RIO_DEVICEID_REG0
		0, 		//1,      // id size = 1 for 16bit device IDs
		0,      // outport id = 0
		0,      // priority = 0
		0,      // xambs = 0
		0,      // suppress good interrupt = 0 (don't care about interrupts)
		0);     // interrupt request = 0
	CSL_SRIO_SetLSUReg5 (hSrio, LSU,
		4,//ttype,
		5,//ftype,
		hop_count,
		0); 	// doorbell = 0

	printf("NWRITE (ID=0x%02lX, HOP=%d):  0x%08lX = 0x%08lX\n", destId, hop_count, destAdr, *((uint32_t *)SRC));

	return 0;
}

///////////////////////////////////////////////////////////////
////////// mreadFunc() ///////////////////////////////////////
///////////////////////////////////////////////////////////////
int mreadFunc(char *cmdStr)
{
	dbg_printf("MREAD: %s\n", cmdStr);

	uint32_t	destAdr;
	uint8_t		destId;

	char		str[128];
	int 		len;

	char *pp = &cmdStr[0];
	if( (len=parse_word(str,cmdStr)) < 0) {
		printf("### mreadFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	char *end;
	destId = strtoul(str, &end, 16);

	pp = &cmdStr[len+1];
	if( (len=parse_word(str,pp)) < 0) {
		printf("### mreadFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	destAdr = strtoul(str, &end, 16);

	*((uint32_t *)SRC) =  0x01020304;

	/* Make sure there is space in the Shadow registers to write*/
	while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);
	// NWRITE Packet Type
	CSL_SRIO_SetLSUReg0 (hSrio, LSU, 0); //no rapidio MSB
	CSL_SRIO_SetLSUReg1 (hSrio, LSU, destAdr);
	CSL_SRIO_SetLSUReg2 (hSrio, LSU, SRC);
	CSL_SRIO_SetLSUReg3 (hSrio, LSU, 0x4, 0); //write 4 bytes, no doorbell
	CSL_SRIO_SetLSUReg4 (hSrio, LSU,
		destId,// destid
		0,      // src id map = 0, using RIO_DEVICEID_REG0
		0, 		//1,      // id size = 1 for 16bit device IDs
		0,      // outport id = 0
		0,      // priority = 0
		0,      // xambs = 0
		0,      // suppress good interrupt = 0 (don't care about interrupts)
		0);     // interrupt request = 0
	CSL_SRIO_SetLSUReg5 (hSrio, LSU,
		0,//ttype,
		8,//ftype,
		hop_count,
		0); 	// doorbell = 0

	printf("NREAD (ID=0x%02lX, HOP=%d):  0x%08lX = 0x%08lX\n", destId, hop_count, destAdr, *((uint32_t *)SRC));

	volatile static int timeout = 0;
	while( *((int32_t *)SRC) == 0x01020304) {
		timeout++;
		if(timeout>10000000) {
			printf("### Timeout Error\n");
			return -1;
		}
	}

	printf("MREAD (ID=0x%02lX, HOP=%d):  0x%08lX = 0x%08lX\n", destId, hop_count, destAdr, *((uint32_t *)SRC));

	return 0;
}

///////////////////////////////////////////////////////////////
////////// mwriteFunc() ///////////////////////////////////////
///////////////////////////////////////////////////////////////
int mwriteFunc(char *cmdStr)
{
	dbg_printf("MWRITE: %s\n", cmdStr);

	uint32_t	destAdr;
	uint8_t		destId;
	uint32_t	val;

	char		str[128];
	int 		len1;

	char *pp = &cmdStr[0];
	if( (len1=parse_word(str,cmdStr)) < 0) {
		printf("### mwriteFunc: parse_word(,'%s') error\n", cmdStr);
		return -1;
	}
	char *end;
	destId = strtoul(str, &end, 16);

	int 		len2;
	pp = &cmdStr[len1+1];
	if( (len2=parse_word(str,pp)) < 0) {
		printf("### mwriteFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	destAdr = strtoul(str, &end, 16);

	int len3;
	pp = &cmdStr[len1+len2+2];
	if( (len3=parse_word(str,pp)) < 0) {
		printf("### mwriteFunc: parse_word(,'%s') error\n", pp);
		return -1;
	}
	val = strtoul(str, &end, 16);
	*((uint32_t *)SRC) =  val;

	/* Make sure there is space in the Shadow registers to write*/
	while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);
	// NWRITE Packet Type
	CSL_SRIO_SetLSUReg0 (hSrio, LSU, 0); //no rapidio MSB
	CSL_SRIO_SetLSUReg1 (hSrio, LSU, destAdr);
	CSL_SRIO_SetLSUReg2 (hSrio, LSU, SRC);
	CSL_SRIO_SetLSUReg3 (hSrio, LSU, 0x4, 0); //write 4 bytes, no doorbell
	CSL_SRIO_SetLSUReg4 (hSrio, LSU,
		destId,// destid
		0,      // src id map = 0, using RIO_DEVICEID_REG0
		0, 		//1,      // id size = 1 for 16bit device IDs
		0,      // outport id = 0
		0,      // priority = 0
		0,      // xambs = 0
		0,      // suppress good interrupt = 0 (don't care about interrupts)
		0);     // interrupt request = 0
	CSL_SRIO_SetLSUReg5 (hSrio, LSU,
		1,//ttype,
		8,//ftype,
		hop_count,
		0); 	// doorbell = 0

	printf("NWRITE (ID=0x%02lX, HOP=%d):  0x%08lX = 0x%08lX\n", destId, hop_count, destAdr, *((uint32_t *)SRC));

	return 0;
}

///////////////////////////////////////////////////////////////
////////// helpFunc() ///////////////////////////////////////
///////////////////////////////////////////////////////////////
int	print_cmd_list()
{
	printf("=========== Command List ===================================================================\n");

	printf("=========== Internal Command ======================================================\n");
	printf("quit                                Quit application 	(alias - q)\n");
	printf("read <AdrHex>                       Read memory word 	(alias - rd or r)\n");
	printf("dump <AdrHex> <SizeDec>             Read memory dump\n");
	printf("write <AdrHex> <ValHex>             Write memory word (alias - wr or w)\n");
	printf("fill <AdrHex> <SizeDec> <ValHex>    Fill memry\n");
	printf("dbg                                 Set/clr debug print message (alias - d)\n");
	printf("help                                View this help message (alias - h or ?)\n");

	printf("=========== SRIO Command ===========================================================\n");
	printf("nread  <IdHex> <AdrHex>             Read memory word from SRIO ID (alias - nr)\n");
	printf("nwrite <IdHex> <AdrHex> <ValHex>    Write memory to SRIO ID (alias - nw)\n");
	printf("mread  <IdHex> <AdrHex>             Maint read memory word from SRIO ID (alias - nr)\n");
	printf("mwrite <IdHex> <AdrHex> <ValHex>    Maint write memory to SRIO ID (alias - nw)\n");
	printf("lsu <NumDec>                        Set LSU number (default 0)\n");
	printf("hop <NumDec>                        Set hop_count (default 0)\n");

	printf("============================================================================================\n");

	return 0;
}

int helpFunc(char *cmdStr)
{
	dbg_printf("Help: %s\n", cmdStr);

	char		str[128];

	print_cmd_list();

	return 0;
}

static CmdEntry   cmdEntries[] =
{
	{ "nread",		nreadFunc },	// SRIO NREAD Packet
	{ "nr",			nreadFunc },	// SRIO NREAD Packet
	{ "nwrite",		nwriteFunc },	// SRIO NWRITE Packet
	{ "nw",			nwriteFunc },	// SRIO NWRITE Packet
	{ "mread",		mreadFunc },	// SRIO Maint Read Packet
	{ "mr",			mreadFunc },	// SRIO Maint Read Packet
	{ "mwrite",		mwriteFunc },	// SRIO Maint Write Packet
	{ "mw",			mwriteFunc },	// SRIO Maint Write Packet
	{ "hop",		hopFunc },		// SRIO set hop_count value
	{ "lsu",		lsuFunc },		// SRIO set LSU number
	{ "quit",		quitFunc },		// quit app
	{ "q",			quitFunc },		// quit app
	{ "read",		rdFunc },		// direct read memory
	{ "rd",			rdFunc },		// direct read memory
	{ "r",			rdFunc },		// direct read memory
	{ "write",		wrFunc },		// direct write memory
	{ "wr",			wrFunc },		// direct write memory
	{ "dump",		dumpFunc },		// read dump memory
	{ "fill",		fillFunc },		// fill memory
	{ "dbg",		dbgFunc },		// set/clr debug mode
	{ "d",			dbgFunc },		// set/clr debug mode
	{ "help",		helpFunc },		// print help verbose
	{ "h",			helpFunc },		// print help verbose
	{ "?",			helpFunc },		// print help verbose
    { 0, NULL }
};
///////////////////////////////////////////////////////////////

char	cmdbuf[256];

/*
 * main.c
 */
int main(int argc, char *argv[])
{

	volatile uint32_t destination = 0x10840000; 	// address of dest
	volatile uint32_t source = 0x10850000;		// address of src
	int LSU = 0;
	int	i;

//	volatile uint32_t main_src = 0x10841000; 	// address of main_src

	printf("\n");
	printf("================ SRIO COMMAND MONITOR ======================= \n");
	printf("======== (C) PapaKarlo Software, Sep. 2019) ================= \n");
	printf("============================================================= \n");

	for(i=0;i<argc;i++)
	{
		if(argv[i][0] == '-')
		{
			switch(argv[i][1]) {
			case 'v':
			case 'V':	verbose_flag = 1; break;
			case 'b':
			case 'B':	board_id = atol(&argv[i][2]); break;
			case 'd':
			case 'D':	{
				char *end;
				main_deviceID = strtoul(&argv[i][2], &end, 16);
				break;
				}
			case 'h':
			case 'H':
			case '?':	print_usage(); return 0;
			}
		}
	}

	printf ("Board ID = %d, main Device ID = 0x%X\n", board_id, main_deviceID);

	if(verbose_flag) printf ("SRIO APP START\n");

	/* Power on SRIO peripheral before using it */
	if (enable_srio () < 0)
	{
		printf ("Error: SRIO PSC Initialization Failed\n");
		return -1;
	}
	if(verbose_flag) printf ("SRIO Enable\n");

	/* Device Specific SRIO Initializations: This should always be called before
	 * initializing the SRIO Driver. */
	if (SrioDevice_init(speed) < 0)
		return -2;

//	setSrioLanes (hSrio, srio_lanes_form_one_4x_port);

	/* SRIO Driver is operational at this time. */
	if(verbose_flag) printf ("SRIO Driver has been initialized\n");
	//displaySrioLanesStatus (hSrio);
	Uint16 config;
	CSL_BootCfgGetSRIOSERDESConfigPLL (&config);
	if(config == 0x281) printf ("RATE 1.25GHz (PLL = %X)\n", config);
	else 				printf ("RATE UNKNOW  (PLL = %X)\n", config);

    Uint16         deviceId;
    Uint16         deviceVendorId;
    Uint32         deviceRev;
    CSL_SRIO_GetDeviceInfo(hSrio, &deviceId, &deviceVendorId, &deviceRev);
    if(verbose_flag) printf("DevID = 0x%X, DevVendorID = 0x%X, DevRev = 0x%X\n", deviceId, deviceVendorId, deviceRev);

	while(1)	// Command cycle
	{
		if(dbg_flag==0) 		printf("$>");					//
		else if(dbg_flag==1) 	printf("DBG$>");					//
		gets(cmdbuf);					// get command string
		dbg_printf("%s\n",cmdbuf);			// print Command String
		// parse command
		char cmd[32];
		int ret = 0;
		if( (ret=parse_word(cmd,cmdbuf)) < 0) {
			printf("Error command string");
			continue;
		}
		dbg_printf("command: '%s' (length = %d)\n", cmd, ret);

		int ii;
	    for( ii=0; ii<sizeof(cmdEntries)/sizeof(cmdEntries[0]); ii++ )
	    {
	        if( strcmp(cmdEntries[ii].cmd,cmd) == 0 )
	        {
	            cmdEntries[ii].func( &cmdbuf[ret+1] );
	            break;
	        }
	    }
	    if(ii==sizeof(cmdEntries)/sizeof(cmdEntries[0])) {
	    	printf("### BAD Command - %s\n", cmd);
	    }

	}



	/* Make sure there is space in the Shadow registers to write*/
	while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);
	enable_srio ();

	*((uint32_t *)destination) =  0;
	*((uint32_t *)source)    =  0;


	return 0;

}
