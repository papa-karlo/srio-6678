//#include <stdio.h>
#include <string.h>
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
	printf ("Debug: Waiting for SRIO ports to be operational...  \n");
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

    	    printf ("Debug: SRIO port %d is %soperational.\n", port, statusText);
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
	printf ("Debug:   Lanes status shows lanes formed as %s%s", laneSetupText, "\n");

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

int master_flag = 0;
int board_id = 1;
int test_flag = 0;

void	print_usage(void)
{
	printf("\n");
	printf("SRIO Test Application for C6678 modules\n");
	printf("Keys:  -h, -H, -?			  -- this message\n");
	printf("       -m, -M                 -- master mode\n");
	printf("       -s, -S				  -- slave mode\n");
	printf("       -b<N>, -B<N>			  -- board id N\n");
}

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


	for(i=0;i<argc;i++)
	{
		if(argv[i][0] == '-')
		{
			switch(argv[i][1]) {
			case 'T':
			case 't':	test_flag = 1; break;
			case 'M':
			case 'm':	master_flag = 1; break;
			case 's':
			case 'S':	master_flag = 0; break;
			case 'b':
			case 'B':	board_id = atol(&argv[i][2]); break;
			case 'h':
			case 'H':
			case '?':	print_usage(); return 0;
			}
		}
	}


	printf ("SRIO APP START\n");

	/* Power on SRIO peripheral before using it */
	if (enable_srio () < 0)
	{
		printf ("Error: SRIO PSC Initialization Failed\n");
		return -1;
	}
	printf ("SRIO Enable\n");

	/* Device Specific SRIO Initializations: This should always be called before
	 * initializing the SRIO Driver. */
	if (SrioDevice_init() < 0)
		return -2;

//	setSrioLanes (hSrio, srio_lanes_form_one_4x_port);

	/* SRIO Driver is operational at this time. */
	printf ("SRIO Driver has been initialized\n");
//	displaySrioLanesStatus (hSrio);
	Uint16 config;
	CSL_BootCfgGetSRIOSERDESConfigPLL (&config);
	if(config == 0x281) printf ("SRIO PLL VALUE = %X, RATE 1.25GHz\n", config);
	else 				printf ("SRIO PLL VALUE = %X, RATE UNKNOW\n", config);

    Uint16         deviceId;
    Uint16         deviceVendorId;
    Uint32         deviceRev;
    CSL_SRIO_GetDeviceInfo(hSrio, &deviceId, &deviceVendorId, &deviceRev);
	printf("DevID = 0x%X, DevVendorID = 0x%X, DevRev = 0x%X\n", deviceId, deviceVendorId, deviceRev);

	uint8_t	dest_id;
	if( board_id == 1 ) 		dest_id = DEVICE_ID2_8BIT;
	else if( board_id == 2 ) 	dest_id = DEVICE_ID1_8BIT;


	if( master_flag==1) {
		*((uint32_t *)destination) =  0x11221122;
		*((uint32_t *)source)    =  0x87654321;//0xFEEDFEED;
	} else {
		*((uint32_t *)destination) =  0x33443344;
		*((uint32_t *)source)    =  0x18273645;//0xFEEDFEED;
	}
	printf("Destination address = 0x%x, Source address = 0x%x\n",destination,source);
	printf("Before SRIO transaction, destination value = 0x%x, source value = 0x%x\n",*((uint32_t *)destination), *((uint32_t *)source));

	if(master_flag!=0) {
		/* Make sure there is space in the Shadow registers to write*/
		while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);

		// NWRITE Packet Type
		CSL_SRIO_SetLSUReg0 (hSrio, LSU, 0); //no rapidio MSB
		CSL_SRIO_SetLSUReg1 (hSrio, LSU, destination);
		CSL_SRIO_SetLSUReg2 (hSrio, LSU, source);
		CSL_SRIO_SetLSUReg3 (hSrio, LSU, 0x4, 0); //write 4 bytes, no doorbell
		CSL_SRIO_SetLSUReg4 (hSrio, LSU,
			dest_id,// destid
			0,      // src id map = 0, using RIO_DEVICEID_REG0
			0, 		//1,      // id size = 1 for 16bit device IDs
			0,      // outport id = 0
			0,      // priority = 0
			0,      // xambs = 0
			0,      // suppress good interrupt = 0 (don't care about interrupts)
			0);     // interrupt request = 0
		CSL_SRIO_SetLSUReg5 (hSrio, LSU,
			4,  	// ttype,
			5,  	// ftype,
			0,  	// hop count = 0,
			0); 	// doorbell = 0
	} else {
		//while( *((int32_t *)destination) != *((int32_t *)source))
		while( *((int32_t *)destination) != 0x87654321 );
		printf("After SRIO transaction, destination value = 0x%x, source value = 0x%x\n",*((uint32_t *)destination), *((uint32_t *)source));
	}

	if( master_flag==1) {
		*((uint32_t *)destination) =  0x11221122;
		*((uint32_t *)source)    =  0x87654321;//0xFEEDFEED;
	} else {
		*((uint32_t *)destination) =  0x33443344;
		*((uint32_t *)source)    =  0x18273645;//0xFEEDFEED;
	}
	printf("Destination address = 0x%x, Source address = 0x%x\n",destination,source);
	printf("Before SRIO transaction, destination value = 0x%x, source value = 0x%x\n",*((uint32_t *)destination), *((uint32_t *)source));


	if(master_flag==0) {
		/* Make sure there is space in the Shadow registers to write*/
		while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);

		// NWRITE Packet Type
		CSL_SRIO_SetLSUReg0 (hSrio, LSU, 0); //no rapidio MSB
		CSL_SRIO_SetLSUReg1 (hSrio, LSU, destination);
		CSL_SRIO_SetLSUReg2 (hSrio, LSU, source);
		CSL_SRIO_SetLSUReg3 (hSrio, LSU, 0x4, 0); //write 4 bytes, no doorbell
		CSL_SRIO_SetLSUReg4 (hSrio, LSU,
			dest_id, // destid
			0,      // src id map = 0, using RIO_DEVICEID_REG0
			0, //1,      // id size = 1 for 16bit device IDs
			0,      // outport id = 0
			0,      // priority = 0
			0,      // xambs = 0
			0,      // suppress good interrupt = 0 (don't care about interrupts)
			0);     // interrupt request = 0
		CSL_SRIO_SetLSUReg5 (hSrio, LSU,
			4,  	// ttype,
			5,  	// ftype,
			0,  	// hop count = 0,
			0); 	// doorbell = 0
	} else {
		while( *((int32_t *)destination) != 0x18273645);
		printf("After SRIO transaction, destination value = 0x%x, source value = 0x%x\n",*((uint32_t *)destination), *((uint32_t *)source));
	}

	/* Make sure there is space in the Shadow registers to write*/
	while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);

	*((uint32_t *)source) =  0x01020304;

	// NREAD Packet Type
	CSL_SRIO_SetLSUReg0 (hSrio, LSU, 0); //no rapidio MSB
	CSL_SRIO_SetLSUReg1 (hSrio, LSU, destination); // config offset
	CSL_SRIO_SetLSUReg2 (hSrio, LSU, source);
	CSL_SRIO_SetLSUReg3 (hSrio, LSU, 0x8, 0); //read 4 bytes, no doorbell
	CSL_SRIO_SetLSUReg4 (hSrio, LSU,
		dest_id,// destid
		0,      // src id map = 0, using RIO_DEVICEID_REG0
		0, 		// id size = 1 for 16bit device IDs
		0,      // outport id = 0
		0,      // priority = 0
		0,      // xambs = 0
		0,      // suppress good interrupt = 0 (don't care about interrupts)
		0);     // interrupt request = 0
	CSL_SRIO_SetLSUReg5 (hSrio, LSU,  // maint read
		4,//0,  	// ttype,
		2,//8,  	// ftype,
		0,  	// hop count = 0,
		0); 	// doorbell = 0

	while( *((int32_t *)source) == 0x01020304);
	printf("Main Value = 0x%x\n", *((uint32_t *)source));

	/* Make sure there is space in the Shadow registers to write*/
	while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);
	/* Make sure there is space in the Shadow registers to write*/
//	while (CSL_SRIO_IsLSUBusy (hSrio, LSU) != 0);

	*((uint32_t *)source) =  0x01020304;

	// MAINT READ Packet Type
	CSL_SRIO_SetLSUReg0 (hSrio, LSU, 0); //no rapidio MSB
	CSL_SRIO_SetLSUReg1 (hSrio, LSU, 0x0000); // config offset
	CSL_SRIO_SetLSUReg2 (hSrio, LSU, source);
	CSL_SRIO_SetLSUReg3 (hSrio, LSU, 0x4, 0); //read 4 bytes, no doorbell
	CSL_SRIO_SetLSUReg4 (hSrio, LSU,
		dest_id,// destid
		0,      // src id map = 0, using RIO_DEVICEID_REG0
		0, 		// id size = 1 for 16bit device IDs
		0,      // outport id = 0
		0,      // priority = 0
		0,      // xambs = 0
		0,      // suppress good interrupt = 0 (don't care about interrupts)
		0);     // interrupt request = 0
	CSL_SRIO_SetLSUReg5 (hSrio, LSU,  // maint read
		0,  	// ttype,
		8,  	// ftype,
		0x3,  	// hop count = 0,
		0); 	// doorbell = 0

	while( *((int32_t *)source) == 0x01020304);
	//for(;;)
		printf("Main Value = 0x%x\n", *((uint32_t *)source));

	/* Make sure there is space in the Shadow registers to write*/
	while (CSL_SRIO_IsLSUFull (hSrio, LSU) != 0);
	enable_srio ();

	if((master_flag==1) && (test_flag!=0)) {
		if(*((uint32_t *)source) == 0x5349CD00)
			printf("OK\n");
		else
			printf("ERROR\n");
		//DZY_exit(0);
	}

	*((uint32_t *)destination) =  0;
	*((uint32_t *)source)    =  0;

	if((master_flag==0) && (test_flag!=0))
		DZY_exit(0);


	return 0;

}
