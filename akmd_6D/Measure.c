/******************************************************************************
 *
 *  $Id: $
 *
 * -- Copyright Notice --
 *
 * Copyright (c) 2004 Asahi Kasei Microdevices Corporation, Japan
 * All Rights Reserved.
 *
 * This software program is the proprietary program of Asahi Kasei Microdevices
 * Corporation("AKM") licensed to authorized Licensee under the respective
 * agreement between the Licensee and AKM only for use with AKM's electronic
 * compass IC.
 *
 * THIS SOFTWARE IS PROVIDED TO YOU "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABLITY, FITNESS FOR A PARTICULAR PURPOSE AND NON INFRINGEMENT OF
 * THIRD PARTY RIGHTS, AND WE SHALL NOT BE LIABLE FOR ANY LOSSES AND DAMAGES
 * WHICH MAY OCCUR THROUGH USE OF THIS SOFTWARE.
 *
 * -- End Asahi Kasei Microdevices Copyright Notice --
 *
 ******************************************************************************/
#include "AKCommon.h"
#include "AKMD_Driver.h"
#include "DispMessage.h"
#include "FileIO.h"
#include "Measure.h"
#include "misc.h"
#include "check_usb.h"
#include "AKM_HDataCheck.h"

#ifdef ENABLE_ACC_FILTER
#include "stable_checking.h"
#include "yaw_average.h"
#endif

#define ACC_ACQ_FLAG_POS	ACC_DATA_FLAG
#define MAG_ACQ_FLAG_POS	MAG_DATA_FLAG
#define FUSION_ACQ_FLAG_POS	FUSION_DATA_FLAG
#define ACC_MES_FLAG_POS	8
#define ACC_INT_FLAG_POS	9
#define MAG_MES_FLAG_POS	10
#define MAG_INT_FLAG_POS	11
#define SETTING_FLAG_POS	12

#define AKMD_MAG_MIN_INTERVAL	10000000	/*!< magnetometer interval */
#define AKMD_ACC_MIN_INTERVAL	10000000	/*!< acceleration interval */
#define AKMD_FUSION_MIN_INTERVAL	10000000	/*!< fusion interval */
#define AKMD_MAG_INTERVAL		50000000	/*!< magnetometer interval */
#define AKMD_ACC_INTERVAL		50000000	/*!< acceleration interval */
#define AKMD_FUSION_INTERVAL	10000000	/*!< fusion interval */
#define AKMD_LOOP_MARGIN		3000000		/*!< Minimum sleep time */
#define AKMD_SETTING_INTERVAL	500000000	/*!< Setting event interval */

#define DEG2RAD(x)      ((AKSC_FLOAT)(((x) * AKSC_PI) / 180.0))
#define AKSC2SI(x)		((AKSC_FLOAT)(((x) * 9.80665f) / 720.0))

#ifdef AKMD_AK099XX
#define AKMD_ST2_POS 8
#else
#define AKMD_ST2_POS 7
#endif

static FORM_CLASS* g_form = NULL;
#ifdef ENABLE_ACC_FILTER
static char phone_moving = -1;
#endif

/*!
 This function open formation status device.
 @return Return 0 on success. Negative value on fail.
 */
static int16 openForm(void)
{
	if (g_form != NULL) {
		if (g_form->open != NULL) {
			return g_form->open();
		}
	}
	// If function is not set, return success.
	return 0;
}

/*!
 This function close formation status device.
 @return None.
 */
static void closeForm(void)
{
	if (g_form != NULL) {
		if (g_form->close != NULL) {
			g_form->close();
		}
	}
}

/*!
 This function check formation status
 @return The index of formation.
 */
static int16 checkForm(void)
{
	if (g_form != NULL) {
		if (g_form->check != NULL) {
			return g_form->check();
		}
	}
	// If function is not set, return default value.
	return 0;
}

/*!
 This function registers the callback function.
 @param[in]
 */
void RegisterFormClass(FORM_CLASS* pt)
{
	g_form = pt;
}

/*!
 Initialize #AKSCPRMS structure. At first, 0 is set to all parameters.
 After that, some parameters, which should not be 0, are set to specific
 value. Some of initial values can be customized by editing the file
 \c "CustomerSpec.h".
 @param[out] prms A pointer to #AKSCPRMS structure.
 */
void InitAKSCPRMS(AKSCPRMS* prms)
{
	// Set 0 to the AKSCPRMS structure.
	memset(prms, 0, sizeof(AKSCPRMS));

	// Sensitivity
	prms->m_hs.u.x = AKSC_HSENSE_TARGET;
	prms->m_hs.u.y = AKSC_HSENSE_TARGET;
	prms->m_hs.u.z = AKSC_HSENSE_TARGET;

	// HDOE
	prms->m_hdst = AKSC_HDST_UNSOLVED;

	// (m_hdata is initialized with AKSC_InitDecomp)
	prms->m_hnave = CSPEC_HNAVE;
	prms->m_dvec.u.x = CSPEC_DVEC_X;
	prms->m_dvec.u.y = CSPEC_DVEC_Y;
	prms->m_dvec.u.z = CSPEC_DVEC_Z;
}

/*!
 Fill #AKSCPRMS structure with default value.
 @param[out] prms A pointer to #AKSCPRMS structure.
 */
void SetDefaultPRMS(AKSCPRMS* prms)
{
	int16 i, j;
	// Set parameter to HDST, HO, HREF
	for (i = 0; i < CSPEC_NUM_FORMATION; i++) {
		prms->HSUC_HDST[i] = AKSC_HDST_UNSOLVED;
		prms->HSUC_HO[i].u.x = 0;
		prms->HSUC_HO[i].u.y = 0;
		prms->HSUC_HO[i].u.z = 0;
		prms->HFLUCV_HREF[i].u.x = 0;
		prms->HFLUCV_HREF[i].u.y = 0;
		prms->HFLUCV_HREF[i].u.z = 0;
		prms->HSUC_HBASE[i].u.x = 0;
		prms->HSUC_HBASE[i].u.y = 0;
		prms->HSUC_HBASE[i].u.z = 0;
		for (j = 0; j < AKSC_DOEP_SIZE; j++) {
			prms->DOEP_PRMS[i][j] = (AKSC_FLOAT)(0.);
		}
	}
}

/*!
 Get interval from device driver. This function will not resolve dependencies.
 Dependencies will be resolved in Sensor HAL.
 @param[out] acc_mes Accelerometer measurement timing.
 @param[out] mag_mes Magnetometer measurement timing.
 @param[out] acc_acq Accelerometer acquisition timing.
 @param[out] mag_acq Magnetometer acquisition timing.
 @param[out] fusion_acq Orientation sensor acquisition timing.
 @param[out] hdoe_interval HDOE decimator.
 */
int16 GetInterval(
	AKMD_LOOP_TIME* acc_mes,
	AKMD_LOOP_TIME* mag_mes,
	AKMD_LOOP_TIME* acc_acq,
	AKMD_LOOP_TIME* mag_acq,
	AKMD_LOOP_TIME* fusion_acq,
	int16* hdoe_dec)
{
	/* Accelerometer, Magnetometer, Orientation */
	/* Delay is in nano second unit. */
	/* Negative value means the sensor is disabled.*/
	int64_t delay[AKM_NUM_SENSORS];
	int64_t acc_last_interval = 0;

	if (AKD_GetDelay(delay) != AKD_SUCCESS) {
		return AKRET_PROC_FAIL;
	}

#ifdef AKMD_ACC_EXTERNAL
	/* Always disabled */
	delay[0] = -1;
#else
	/* Accelerometer's interval limit */
	if ((0 <= delay[0]) && (delay[0] <= AKMD_ACC_MIN_INTERVAL)) {
		delay[0] = AKMD_ACC_MIN_INTERVAL;
	}    
#endif
	/* Magnetmeter's frequency should be discrete value */
	if ((0 <= delay[1]) && (delay[1] <= AKMD_MAG_MIN_INTERVAL)) {
		delay[1] = AKMD_MAG_MIN_INTERVAL;
	}    
	/* Fusion sensor's interval limit */
	if ((0 <= delay[2]) && (delay[2] <= AKMD_FUSION_MIN_INTERVAL)) {
		delay[2] = AKMD_FUSION_MIN_INTERVAL;
	}    

	if ((delay[0] != acc_acq->interval) ||
			(delay[1] != mag_acq->interval) ||
			(delay[2] != fusion_acq->interval)) {

		/* reserve previous value */
		acc_last_interval = acc_mes->interval;

		/* Copy new value */
		acc_acq->duration = acc_acq->interval = delay[0];
		mag_acq->duration = mag_acq->interval = delay[1];
		fusion_acq->duration = fusion_acq->interval = delay[2];

		if (fusion_acq->interval < 0) {
			/* NO relation between fusion sensor and physical sensor */
			acc_mes->interval = acc_acq->interval;
			mag_mes->interval = mag_acq->interval;
		} else {
			/* Solve dependency */
			if ((acc_acq->interval >= 0) &&
				(acc_acq->interval < fusion_acq->interval)) {
				acc_mes->interval = acc_acq->interval;
			} else {
				acc_mes->interval = fusion_acq->interval;
			}
			if ((mag_acq->interval >= 0) &&
				(mag_acq->interval < fusion_acq->interval)) {
				mag_mes->interval = mag_acq->interval;
			} else {
				mag_mes->interval = fusion_acq->interval;
			}
		}
		acc_mes->duration = 0;
		mag_mes->duration = 0;

		if (mag_mes->interval >= 0) {
			/* Magnetometer measurement interval should be discrete value */
			GetHDOEDecimator(&(mag_mes->interval), hdoe_dec);
		}

		if (acc_last_interval != acc_mes->interval) {
			if (acc_mes->interval >= 0) {
				/* Acc is enabled */
				if (AKD_AccSetEnable(AKD_ENABLE) != AKD_SUCCESS) {
					AKMERROR;
					return AKRET_PROC_FAIL;
				}
				/* Then set interval */
				if (AKD_AccSetDelay(acc_acq->interval) != AKD_SUCCESS) {
					AKMERROR;
					return AKRET_PROC_FAIL;
				}
			} else {
				/* Acc is disabled */
				if (AKD_AccSetEnable(AKD_DISABLE) != AKD_SUCCESS) {
					AKMERROR;
					return AKRET_PROC_FAIL;
				}
			}
		}

		AKMDEBUG(AKMDBG_GETINTERVAL,
				"%s:\n"
				"  AcqInterval(M,A,Fusion)=%8.2f,%8.2f,%8.2f\n"
				"  MesInterval(M,A)=%8.2f,%8.2f\n",
				__FUNCTION__,
				mag_acq->interval/1000000.0f,
				acc_acq->interval/1000000.0f,
				fusion_acq->interval/1000000.0f,
				mag_mes->interval/1000000.0f,
				acc_mes->interval/1000000.0f);
	}

	return AKRET_PROC_SUCCEED;
}

/*!
 Calculate loop duration
 @return If it is time to fire the event, the return value is 1, otherwise 0.
 @param[in,out] tm An event.
 @param[in] execTime The time to execute main loop for one time.
 @param[in,out] minDuration The minimum sleep time in all events.
 */
int SetLoopTime(
	AKMD_LOOP_TIME* tm,
	int64_t execTime,
	int64_t* minDuration)
{
	int ret = 0;
	if (tm->interval >= 0) {
		tm->duration -= execTime;
		if (tm->duration <= AKMD_LOOP_MARGIN) {
			tm->duration = tm->interval;
			ret = 1;
		} else if (tm->duration < *minDuration) {
			*minDuration = tm->duration;
		}
	}
	return ret;
}

/*!
 Read hard coded value (Fuse ROM) from AKM E-Compass. Then set the read value
 to calculation parameter.
 @return If parameters are read successfully, the return value is
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No
 error code is reserved to show which operation has failed.
 @param[out] prms A pointer to #AKSCPRMS structure.
 */
int16 ReadFUSEROM(AKSCPRMS* prms)
{
	BYTE	info[AKM_SENSOR_INFO_SIZE];
	BYTE	conf[AKM_SENSOR_CONF_SIZE];
	
	prms->akm_device=0;

	// Get information
	if (AKD_GetSensorInfo(info) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	if((info[1]==0x05)&&(info[0]==0x48))
	{
		prms->akm_device=MAG_AKM09911;
		// Get configuration
		if (AKD_GetSensorConf(conf) != AKD_SUCCESS) {
			AKMERROR;
			return AKRET_PROC_FAIL;
		}
	}
	else if((info[1]==0x09)&&(info[0]==0x48))
	{
		
		prms->akm_device=MAG_AKM09916;
		conf[0] = conf[1] = conf[2] = 0x80;
	}

	prms->m_asa.u.x = (int16)conf[0];
	prms->m_asa.u.y = (int16)conf[1];
	prms->m_asa.u.z = (int16)conf[2];

	AKMDEBUG(AKMDBG_DEBUG, "%s: asa(dec)=%d,%d,%d\n", __FUNCTION__,
			prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);

	// Set keywords for SmartCompassLibrary certification
	prms->m_key[0] = AKSC_GetVersion_Device();

//#ifdef AKMD_AK099XX
	/* This definition is used by AK099XX. */ 
	prms->m_key[1] =0x548;;
//#else
	/* This definition is used by AK89XX.  */
//	prms->m_key[1] = (int16)info[0];
//#endif
	prms->m_key[2] = (int16)conf[0];
	prms->m_key[3] = (int16)conf[1];
	prms->m_key[4] = (int16)conf[2];
	strncpy((char *)prms->m_licenser, CSPEC_CI_LICENSER, AKSC_CI_MAX_CHARSIZE);
	strncpy((char *)prms->m_licensee, CSPEC_CI_LICENSEE, AKSC_CI_MAX_CHARSIZE);

	AKMDEBUG(AKMDBG_DEBUG, "%s: key=%d, licenser=%s, licensee=%s\n",
			__FUNCTION__, prms->m_key[1], prms->m_licenser, prms->m_licensee);

		 

	AKMDEBUG(AKMDBG_DEBUG, "%s: device=%d, DOEPlus=%d\n",
			__FUNCTION__, prms->akm_device, prms->m_en_doeplus);

	return AKRET_PROC_SUCCEED;
}


/*!
 Set initial values for SmartCompass library.
 @return If parameters are read successfully, the return value is
 #AKRET_PROC_SUCCEED. Otherwise the return value is #AKRET_PROC_FAIL. No
 error code is reserved to show which operation has failed.
 @param[in,out] prms A pointer to a #AKSCPRMS structure.
 */
int16 Init_Measure(AKSCPRMS* prms)
{
#ifdef AKMD_FOR_AK09912
	BYTE	i2cData[AKM_SENSOR_DATA_SIZE];
#endif

	// Reset device.
	if (AKD_Reset() != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

#ifdef AKMD_FOR_AK09912
	// Set to Temperature mode and Noise Suppression Filter mode.
	i2cData[0] = CSPEC_TEMPERATURE | CSPEC_NSF;
	if (AKD_TxData(AK09912_REG_CNTL1, i2cData, 1) != AKD_SUCCESS) {
		AKMERROR;
		return 0;
	}
#endif
	prms->m_form = checkForm();

    prms->m_pre_usb_online = check_usb_online();
    prms->m_usb_changed = 0;
	prms->m_usb_online = 0;
	// Restore the value when succeeding in estimating of HOffset.
	prms->m_ho   = prms->HSUC_HO[prms->m_form];
	prms->m_ho32.u.x = (int32)prms->HSUC_HO[prms->m_form].u.x;
	prms->m_ho32.u.y = (int32)prms->HSUC_HO[prms->m_form].u.y;
	prms->m_ho32.u.z = (int32)prms->HSUC_HO[prms->m_form].u.z;

	prms->m_hdst = prms->HSUC_HDST[prms->m_form];
	prms->m_hbase = prms->HSUC_HBASE[prms->m_form];

	// Initialize the decompose parameters
	AKSC_InitDecomp(prms->m_hdata);
	// Initialize DOEPlus parameters
	if (prms->m_en_doeplus == 1) {
		AKSC_InitDOEPlus(prms->m_doep_var);
		prms->m_doep_lv = AKSC_LoadDOEPlus(
							prms->DOEP_PRMS[prms->m_form],
							prms->m_doep_var);
		AKSC_InitDecomp(prms->m_hdata_plus);
	}
#if 0
	// Initialize HDOE parameters
	AKSC_InitHDOEProcPrmsS3(
							&prms->m_hdoev,
							1,
							&prms->m_ho,
							prms->m_hdst
							);
#else							
		// Initialize HDOEEx parameters
		AKSC_InitHDOEEX(
			//&prms->m_doeex_var,
			prms->m_doeex_var,
			1,
			&prms->m_ho,
			prms->m_hdst
			);
#endif		
	AKSC_InitHFlucCheck(
						&(prms->m_hflucv),
						&(prms->HFLUCV_HREF[prms->m_form]),
						HFLUCV_TH
						);

	AKSC_InitPseudoGyro(
						&prms->m_pgcond,
						&prms->m_pgvar
						);

	prms->m_pgcond.fmode=1;
	prms->m_pgcond.th_rdif=2400;
	prms->m_pgcond.th_rmax=2400;
	prms->m_pgcond.th_rmin=0;
	prms->m_pgcond.ihave=24;
	prms->m_pgcond.iaave=24;
	prms->m_pgcond.ocoef=0.1;//90;//103;

	switch(prms->PG_filter){
	
	case 0:
	prms->m_pgcond.ihave=24;
	prms->m_pgcond.iaave=24;
	prms->m_pgcond.ocoef=0.1;//103;
	break;
	case 1:
	prms->m_pgcond.ihave=24;
	prms->m_pgcond.iaave=24;
	prms->m_pgcond.ocoef=0.2;//205;		
	break;

	case 2:
	prms->m_pgcond.ihave=24;
	prms->m_pgcond.iaave=24;
	prms->m_pgcond.ocoef=0.3;//307;		
	break;

	case 3:
	prms->m_pgcond.ihave=32;
	prms->m_pgcond.iaave=32;
	prms->m_pgcond.ocoef=0.2;//205;		
	break;

	case 4:
	prms->m_pgcond.ihave=32;
	prms->m_pgcond.iaave=32;
	prms->m_pgcond.ocoef=0.3;//307;		
	break;

	case 5:
	prms->m_pgcond.ihave=12;
	prms->m_pgcond.iaave=12;
	prms->m_pgcond.ocoef=0.3;//307;		
	break;

    case 6:
	prms->m_pgcond.ihave=12;
	prms->m_pgcond.iaave=12;
	prms->m_pgcond.ocoef=0.2;//205;		
	break;

	case 7:
	prms->m_pgcond.ihave=12;
	prms->m_pgcond.iaave=12;
	prms->m_pgcond.ocoef=0.1;//103;		
	break;
	
	}

	// Reset counter
	prms->m_cntSuspend = 0;
	prms->m_callcnt = 0;

#ifdef AKM_EXTERN_HDATA_CHECK
    // AKM Test
    // Reset hdata counter
    ALOGE("akm_log , %s,%d, reset counter.", __FUNCTION__, __LINE__);
    prms->m_hdataCnt = 0;
    AKM_HData_CheckInit(
        prms->m_hflucv.href,
        prms->m_ho,
        prms->m_hbase,
        &(prms->m_hCenter),
        &(prms->m_refNorm)
    );
#endif
	return AKRET_PROC_SUCCEED;
}


/*!
 This is the main routine of measurement.
 @param[in,out] prms A pointer to a #AKSCPRMS structure.
 */
void MeasureSNGLoop(AKSCPRMS* prms)
{
	int16 mag_x,mag_y,mag_z;
	BYTE    i2cData[AKM_SENSOR_DATA_SIZE];
	int16   bData[AKM_SENSOR_DATA_SIZE];  // Measuring block data
	int16   adata[3];
	int16   ret;
	int16   i;
	int16	hdoe_interval = 1;
	int		current[1];

	/* Acceleration interval */
	AKMD_LOOP_TIME acc_acq = { -1, 0 };
	/* Magnetic field interval */
	AKMD_LOOP_TIME mag_acq = { -1, 0 };
	/* Orientation interval */
	AKMD_LOOP_TIME fusion_acq = { -1, 0 };
	/* Magnetic acquisition interval */
	AKMD_LOOP_TIME mag_mes = { -1, 0 };
	/* Acceleration acquisition interval */
	AKMD_LOOP_TIME acc_mes = { -1, 0 };
	/* Magnetic measurement interval */
	AKMD_LOOP_TIME mag_int = { AKM_MEASUREMENT_TIME_NS, 0 };
	/* Setting interval */
	AKMD_LOOP_TIME setting = { AKMD_SETTING_INTERVAL, 0 };

	/* 0x0001: Acceleration execute flag (data output) */
	/* 0x0002: Magnetic execute flag (data output) */
	/* 0x0004: Fusion execute flag (data output) */
	/* 0x0100: Magnetic measurement flag */
	/* 0x0200: Magnetic interrupt flag */
	/* 0x0400: Acceleration measurement flag */
	/* 0x0800: Acceleration interrupt flag */
	/* 0x0400: Setting execute flag */
	uint16 exec_flags;

	struct timespec currTime = { 0, 0 }; /* Current time */
	struct timespec lastTime = { 0, 0 }; /* Previous time */
	struct timespec prevGtm = { 0, 0 };

	int64_t execTime; /* Time between two points */
	int64_t minVal; /* The minimum duration to the next event */
	int measuring = 0; /* The value is 1, if while measuring. */

	if (openForm() < 0) {
		AKMERROR;
		return;
	}

	/* Initialize */
	if (Init_Measure(prms) != AKRET_PROC_SUCCEED) {
		goto MEASURE_SNG_END;
	}

	/* Get initial interval */
	if (GetInterval(
				&acc_mes, &mag_mes,
				&acc_acq, &mag_acq, &fusion_acq,
				&hdoe_interval) != AKRET_PROC_SUCCEED) {
		AKMERROR;
		goto MEASURE_SNG_END;
	}

	/* Beginning time */
	if (clock_gettime(CLOCK_MONOTONIC, &currTime) < 0) {
		AKMERROR;
		goto MEASURE_SNG_END;
	}
	/* Set initial value */
	prevGtm = currTime;

	//TODO: Define stop flag
	while (g_stopRequest != 1) {
		exec_flags = 0;
		minVal = 1000000000; /*1sec*/

		/* Copy the last time */
		lastTime = currTime;

        prms->m_usb_online = check_usb_online();
		/* Get current time */
		if (clock_gettime(CLOCK_MONOTONIC, &currTime) < 0) {
			AKMERROR;
			break;
		}

		/* Calculate the difference */
		execTime = CalcDuration(&currTime, &lastTime);

		AKMDEBUG(AKMDBG_EXECTIME,
				"Executing(%6.2f)\n", (double)execTime / 1000000.0);

		/* Subtract the differential time from each event.
		 If subtracted value is negative turn event flag on. */
		exec_flags |= (SetLoopTime(&setting, execTime, &minVal)
					   << (SETTING_FLAG_POS));

		exec_flags |= (SetLoopTime(&acc_acq, execTime, &minVal)
					   << (ACC_ACQ_FLAG_POS));

		exec_flags |= (SetLoopTime(&mag_acq, execTime, &minVal)
					   << (MAG_ACQ_FLAG_POS));

		exec_flags |= (SetLoopTime(&fusion_acq, execTime, &minVal)
					   << (FUSION_ACQ_FLAG_POS)); 

		exec_flags |= (SetLoopTime(&acc_mes, execTime, &minVal)
					   << (ACC_MES_FLAG_POS));

		/* Magnetometer needs special care. While the device is
		 under measuring, measurement start flag should not be turned on.*/
		if (mag_mes.interval >= 0) {
			mag_mes.duration -= execTime;
			if (!measuring) {
				/* Not measuring */
				if (mag_mes.duration <= AKMD_LOOP_MARGIN) {
					exec_flags |= (1 << (MAG_MES_FLAG_POS));
				} else if (mag_mes.duration < minVal) {
					minVal = mag_mes.duration;
				}
			} else {
				/* While measuring */
				mag_int.duration -= execTime;
				/* NO_MARGIN! */
				if (mag_int.duration <= 0) {
					exec_flags |= (1 << (MAG_INT_FLAG_POS));
				} else if (mag_int.duration < minVal) {
					minVal = mag_int.duration;
				}
			}
		}

		/* If all flag is off, go sleep */
		if (exec_flags == 0) {
			AKMDEBUG(AKMDBG_EXECTIME, "Sleeping(%6.2f)...\n",
					(double)minVal / 1000000.0);
			if (minVal > 0) {
				struct timespec doze = { 0, 0 };
				doze = int64_to_timespec(minVal);
				nanosleep(&doze, NULL);
			}
		} else {
			AKMDEBUG(AKMDBG_EXECTIME, "ExecFlags=0x%04X\n", exec_flags);

			if (exec_flags & (1 << (MAG_MES_FLAG_POS))) {
				/* Set to SNG measurement pattern (Set CNTL register) */
				if (AKD_SetMode(AKM_MODE_SNG_MEASURE) != AKD_SUCCESS) {
					AKMERROR;
				} else {
					mag_mes.duration = mag_mes.interval;
					mag_int.duration = mag_int.interval;
					measuring = 1;
				}
			}

			if(exec_flags & (1 << (MAG_INT_FLAG_POS))) 
			{
				/* Get magnetometer measurement data */
				if (AKD_GetMagneticData(i2cData) != AKD_SUCCESS) 
				{
					AKMERROR;
					// Reset driver
					AKD_Reset();
					// Unset flag
					exec_flags &= ~(1 << (MAG_INT_FLAG_POS));
				} 
				else 
				{
					// Copy to local variable
					for (i=0; i<AKM_SENSOR_DATA_SIZE; i++)
					{
						bData[i] = i2cData[i];
					}
					//ALOGE("akm_log mag,%d\n",prms->akm_device);
					if(prms->akm_device == MAG_AKM09916)
					{
						mag_x = (int16)(bData[1] | (((uint16)bData[2]) << 8));
						mag_y = (int16)(bData[3] | (((uint16)bData[4]) << 8));
						mag_z = (int16)(bData[5] | (((uint16)bData[6]) << 8));
						if(check_usb_online())
						{
							AKD_GetUsbCurrent(current);
							/*ALOGE("MagXYZ= %d %d %d UsbOnCurrent: %d\n",mag_x,mag_y,mag_z,current[0]);*/
							#if 0
								/*  1616 pro  */
							mag_x = mag_x - (int16)(0.0083*current[0]);
							mag_y = mag_y + (int16)(0.0526*current[0]);
							mag_z = mag_z - (int16)(0.0300*current[0]);
							
									
								/*  1627 pro new */
							mag_x = mag_x-(int16)(0.0104*current[0]);
							mag_y = mag_y-(int16)(0.0156*current[0]);
							mag_z = mag_z+(int16)(0.0124*current[0]);
							#endif
								/*  1618 pro  */
							mag_x = mag_x + (int16)(0.0013*current[0]);
							mag_y = mag_y + (int16)(0.0008*current[0]);
							mag_z = mag_z - (int16)(0.0167*current[0]);
							
							
						}
						else
						{
							AKD_GetUsbCurrent(current);
							/*ALOGE("MagXYZ= %d %d %d UsbOffCurrent: %d\n",mag_x,mag_y,mag_z,current[0]);*/
						}
					
						mag_x /= 4;
						bData[1] = mag_x & 0xFF;
						bData[2] = (mag_x >> 8) &0xFF;
						mag_y /= 4;
						bData[3] = mag_y & 0xFF;
						bData[4] = (mag_y >> 8) &0xFF;
						mag_z /= 4;
						bData[5] = mag_z & 0xFF;
						bData[6] = (mag_z >> 8) &0xFF;
					}	
					ret = GetMagneticVector(
							bData,
							prms,
							checkForm(),
							hdoe_interval);

					// Check the return value
					if ((ret != AKRET_PROC_SUCCEED) && (ret != AKRET_FORMATION_CHANGED)) {
						ALOGE("GetMagneticVector has failed (0x%04X).\n", ret);
					}

					AKMDEBUG(AKMDBG_VECTOR, "mag(dec)=%6d,%6d,%6d\n",
							prms->m_hvec.u.x, prms->m_hvec.u.y, prms->m_hvec.u.z);
				}
				measuring = 0;
			}

			if (exec_flags & (1 << (ACC_MES_FLAG_POS))) {
				/* Get accelerometer data */
				if (AKD_GetAccelerationData(adata) != AKD_SUCCESS) {
					AKMERROR;
					break;
				}
				AKD_GetAccelerationVector(adata, prms->m_AO.v, prms->m_avec.v);

#ifdef ENABLE_ACC_FILTER
	         phone_moving = phone_move(prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z);
	         //ALOGE("AKM ocean phone_moving = %d\n",phone_moving);
#endif
				AKMDEBUG(AKMDBG_VECTOR, "acc(dec)=%6d,%6d,%6d\n",
						prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z);
			}
			if (exec_flags & (1 << (FUSION_ACQ_FLAG_POS))) {
				int64_t tmpDuration;
				tmpDuration = CalcDuration(&currTime, &prevGtm);
				/*  Limit to 16-bit value */
				if (tmpDuration > 2047000000) {
					tmpDuration = 2047000000;
				}
				prms->m_pgdt = (tmpDuration * 16) / 1000000;
				prevGtm = currTime;
				if (CalcDirection(prms) != AKRET_PROC_SUCCEED) {
					exec_flags &= ~(1 << (FUSION_ACQ_FLAG_POS));
					AKMERROR;
				}
				#ifdef AKM_PG_ENABLE
				/* Calculate angular rate */
				if (CalcAngularRate(prms) != AKRET_PROC_SUCCEED) {
					exec_flags &= ~(1 << (FUSION_ACQ_FLAG_POS));
					AKMERROR;
				}
				#endif
			}

			/* Calculate direction angle */
			if (exec_flags & 0x0F) {
				/* If any ACQ flag is on, report the data to device driver */
				Disp_MeasurementResultHook(prms, (uint16)(exec_flags & 0x0F));
			}

			if (exec_flags & (1 << (SETTING_FLAG_POS))) {
				/* Get measurement interval from device driver */
				GetInterval(
						&acc_mes, &mag_mes,
						&acc_acq, &mag_acq, &fusion_acq,
						&hdoe_interval);
			}
		}
	}

MEASURE_SNG_END:
	// Disable all sensors
	if (AKD_SetMode(AKM_MODE_POWERDOWN) != AKD_SUCCESS) {AKMERROR;}
	if (AKD_AccSetEnable(AKD_DISABLE)   != AKD_SUCCESS) {AKMERROR;}

	closeForm();
}



/*!
 SmartCompass main calculation routine. This function will be processed
 when INT pin event is occurred.
 @retval AKRET_
 @param[in] bData An array of register values which holds,
 ST1, HXL, HXH, HYL, HYH, HZL, HZH and ST2 value respectively.
 @param[in,out] prms A pointer to a #AKSCPRMS structure.
 @param[in] curForm The index of hardware position which represents the
 index when this function is called.
 @param[in] hDecimator HDOE will execute once while this function is called
 this number of times.
 */
int16 GetMagneticVector(
	const int16	bData[],
	AKSCPRMS*	prms,
	const int16	curForm,
	const int16	hDecimator)
{
	const int16vec hrefZero = {{0, 0, 0}};
	int16vec	have, hvec;
	int16		i;
	int16		temperature, dor, derr, hofl, cb, dc;
	int32vec	preHbase;
	int16		overflow;
	int16		hfluc;
	int16		hdSucc;
	int16		aksc_ret;
	int16		ret;
	int16		doep_ret;

	have.u.x = 0;
	have.u.y = 0;
	have.u.z = 0;
	temperature = 0;
	dor = 0;
	derr = 0;
	hofl = 0;
	cb = 0;
	dc = 0;

	preHbase = prms->m_hbase;
	overflow = 0;
	ret = AKRET_PROC_SUCCEED;

	// Subtract the formation suspend counter
	if (prms->m_cntSuspend > 0) {
		prms->m_cntSuspend--;

		// Check the suspend counter
		if (prms->m_cntSuspend == 0) {
			// Restore the value when succeeding in estimating of HOffset.
			prms->m_ho   = prms->HSUC_HO[prms->m_form];
			prms->m_ho32.u.x = (int32)prms->HSUC_HO[prms->m_form].u.x;
			prms->m_ho32.u.y = (int32)prms->HSUC_HO[prms->m_form].u.y;
			prms->m_ho32.u.z = (int32)prms->HSUC_HO[prms->m_form].u.z;

			prms->m_hdst = prms->HSUC_HDST[prms->m_form];
			prms->m_hbase = prms->HSUC_HBASE[prms->m_form];

			// Initialize the decompose parameters
			AKSC_InitDecomp(prms->m_hdata);

			// Initialize DOEPlus parameters
			if (prms->m_en_doeplus == 1) {
				AKSC_InitDOEPlus(prms->m_doep_var);
				prms->m_doep_lv = AKSC_LoadDOEPlus(
									prms->DOEP_PRMS[prms->m_form],
									prms->m_doep_var);
				AKSC_InitDecomp(prms->m_hdata_plus);
			}

#if 0
	// Initialize HDOE parameters
	AKSC_InitHDOEProcPrmsS3(
							&prms->m_hdoev,
							1,
							&prms->m_ho,
							prms->m_hdst
							);
#else							
		// Initialize HDOEEx parameters
		AKSC_InitHDOEEX(
			//&prms->m_doeex_var,
			prms->m_doeex_var,
			1,
			&prms->m_ho,
			prms->m_hdst
			);
#endif		
			// Initialize HFlucCheck parameters
			AKSC_InitHFlucCheck(
				&(prms->m_hflucv),
				&(prms->HFLUCV_HREF[prms->m_form]),
				HFLUCV_TH
			);
		}
	}
	// Decompose one block data into each Magnetic sensor's data
	aksc_ret = AKSC_DecompS3(
					AKSC_GetVersion_Device(),
					bData,
					prms->m_hnave,
					&prms->m_asa,
					prms->m_pdcptr,
					prms->m_hdata,
					&prms->m_hbase,
					&prms->m_hn,
					&have,
					&temperature,
					&dor,
					&derr,
					&hofl,
					&cb,
					&dc
				);
				
		if(g_akmlog_enable)
		{
		ALOGE("%s: ST1, HXH&HXL, HYH&HYL, HZH&HZL, ST2,"
				" hdata[0].u.x, hdata[0].u.y, hdata[0].u.z,"
				" asax, asay, asaz ="
				" %d, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d\n",
				__FUNCTION__,
				bData[0],
				(int16)(((uint16)bData[2])<<8|bData[1]),
				(int16)(((uint16)bData[4])<<8|bData[3]),
				(int16)(((uint16)bData[6])<<8|bData[5]), bData[AKMD_ST2_POS],
				prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z,
				prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z);
				
		}
	/*	ALOGE("AKMD2 oceanbdata  ST1=%6d, ST2=%6d, XYZ(dec)=%6d,%6d,%6d,%6d,%6d,%6d,asa(dec)=%d,%d,%d,hbase(dec)=%ld,%ld,%ld\n",
				bData[0], bData[AKMD_ST2_POS],
				bData[1], bData[2], bData[3], bData[4], bData[5], bData[6],
				prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,
				prms->m_hbase.u.x, prms->m_hbase.u.y, prms->m_hbase.u.z);
	*/
	if (aksc_ret == 0) {
		AKMERROR;
		ALOGE("AKSC_DecompS3 failed.\n"
				"  ST1=0x%02X, ST2=0x%02X\n"
				"  XYZ(HEX)=%02X,%02X,%02X,%02X,%02X,%02X\n"
				"  asa(dec)=%d,%d,%d\n"
				"  pdc(addr)=0x%p\n"
				"  hbase(dec)=%ld,%ld,%ld\n",
				bData[0], bData[AKMD_ST2_POS],
				bData[1], bData[2], bData[3], bData[4], bData[5], bData[6],
				prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,
				prms->m_pdcptr,
				prms->m_hbase.u.x, prms->m_hbase.u.y, prms->m_hbase.u.z);
		return AKRET_PROC_FAIL;
	}
	// Check the formation change
	if (prms->m_form != curForm) {
		prms->m_form = curForm;
		prms->m_cntSuspend = 0;//CSPEC_CNTSUSPEND_SNG;
		prms->m_callcnt = 0;
		ret |= AKRET_FORMATION_CHANGED;
		return ret;
	}

	// Check derr
	if (derr == 1) {
		ret |= AKRET_DATA_READERROR;
		return ret;
	}

	// Check hofl
	if (hofl == 1) {
		if (prms->m_cntSuspend <= 0) {

		#if 0
			// Set a HDOE level as "HDST_UNSOLVED"
			AKSC_SetHDOELevel(
				&prms->m_hdoev,
				&prms->m_ho,
				AKSC_HDST_UNSOLVED,
				1
			);
		#else	
           	// Set a HDOEEx level as "HDST_UNSOLVED"	
			AKSC_SetHDOEEXLevel(
			//&prms->m_doeex_var,
			prms->m_doeex_var,
				&prms->m_ho,
                		AKSC_HDST_UNSOLVED,
                		1
			);
		#endif
			prms->m_hdst = AKSC_HDST_UNSOLVED;
		}
		ret |= AKRET_DATA_OVERFLOW;
		return ret;
	}

	// Check hbase
	if (cb == 1) {
		// Translate HOffset
		AKSC_TransByHbase(
			&(preHbase),
			&(prms->m_hbase),
			&(prms->m_ho),
			&(prms->m_ho32),
			&overflow
		);
		if (overflow == 1) {
			ret |= AKRET_OFFSET_OVERFLOW;
		}

		// Set hflucv.href to 0
		AKSC_InitHFlucCheck(
			&(prms->m_hflucv),
			&(hrefZero),
			HFLUCV_TH
		);

		if (prms->m_cntSuspend <= 0) {
		
		#if 0
			// Set a HDOE level as "HDST_UNSOLVED"
			AKSC_SetHDOELevel(
				&prms->m_hdoev,
				&prms->m_ho,
				AKSC_HDST_UNSOLVED,
				1
			);
		#else	
           	// Set a HDOEEx level as "HDST_UNSOLVED"	
			AKSC_SetHDOEEXLevel(
			    prms->m_doeex_var,
				&prms->m_ho,
                AKSC_HDST_UNSOLVED,
                1
			);
		#endif
			prms->m_hdst = AKSC_HDST_UNSOLVED;
		}

		ret |= AKRET_HBASE_CHANGED;
		return ret;
	}
	
#ifdef AKM_EXTERN_HDATA_CHECK
    // AKM Test
    // Check hdata
    if (prms->m_hdataCnt++ < CSPEC_HDATA_CNT) {
        ret = AKM_HData_Check(
            prms->m_hdata[0],
            prms->m_hbase,
            prms->m_hCenter,
            prms->m_refNorm,
            CSPEC_HDATA_CHANGE_TH
        );
        if (ret != AKM_HDATA_CHECK_SUCCESS) {
			AKSC_SetHDOEEXLevel(
			    prms->m_doeex_var,
				&prms->m_ho,
                AKSC_HDST_UNSOLVED,
                1
			);
            prms->m_hdst = AKSC_HDST_UNSOLVED;
            ret |= AKRET_EXTERN_HCHECK_FAILED;
            ALOGE("akm_log, hdata changing checked when restart.");
            return ret;
        }
    }
#endif

//	prms->m_cntSuspend = 0;
	if (prms->m_cntSuspend <= 0) {
		// Detect a fluctuation of magnetic field.
		//ALOGE("AKMD2 ocean before m_hdata[0] = %d,%d,%d\n",prms->m_hdata[0].u.x,prms->m_hdata[0].u.y,prms->m_hdata[0].u.z);
		//ALOGE("AKMD2 ocean before m_hflucv.href = %d,%d,%d,m_hflucv.th = %d\n",
		//	prms->m_hflucv.href.u.x,
		//	prms->m_hflucv.href.u.y,
		//	prms->m_hflucv.href.u.z,
		//	prms->m_hflucv.th);
		hfluc = AKSC_HFlucCheck(&(prms->m_hflucv), &(prms->m_hdata[0]));
		if (hfluc == 1) {
		//ALOGE("AKMD2 ocean after m_hdata[0] = %d,%d,%d\n",prms->m_hdata[0].u.x,prms->m_hdata[0].u.y,prms->m_hdata[0].u.z);
		//ALOGE("AKMD2 ocean after m_hflucv.href = %d,%d,%d,m_hflucv.th = %d\n",
		//	prms->m_hflucv.href.u.x,
		//	prms->m_hflucv.href.u.y,
		//	prms->m_hflucv.href.u.z,
		//	prms->m_hflucv.th);
		#if 0
			// Set a HDOE level as "HDST_UNSOLVED"
			AKSC_SetHDOELevel(
				&prms->m_hdoev,
				&prms->m_ho,
				AKSC_HDST_UNSOLVED,
				1
			);
		#else	
           	// Set a HDOEEx level as "HDST_UNSOLVED"
			AKSC_SetHDOEEXLevel(
			prms->m_doeex_var,
				&prms->m_ho,
                		AKSC_HDST_UNSOLVED,
                		1
			);
		#endif
			prms->m_hdst = AKSC_HDST_UNSOLVED;
			ret |= AKRET_HFLUC_OCCURRED;
			return ret;
		}
		else {
		
			prms->m_callcnt--;
			if (prms->m_callcnt <= 0) {
			//if (1) {
				if (prms->m_en_doeplus == 1) {
					// Compensate Magnetic Distortion by DOEPlus
					doep_ret = AKSC_DOEPlus(&prms->m_hdata[0], prms->m_doep_var, &prms->m_doep_lv);

					// Save DOEPlus parameters
					if ((doep_ret == 1) && (prms->m_doep_lv == 3)) {
						AKSC_SaveDOEPlus(prms->m_doep_var, prms->DOEP_PRMS[prms->m_form]);
					}

					// Calculate compensated vector for DOE
					for(i = 0; i < prms->m_hn; i++) {
						AKSC_DOEPlus_DistCompen(&prms->m_hdata[i], prms->m_doep_var, &prms->m_hdata_plus[i]);
					}

					AKMDEBUG(AKMDBG_DOEPLUS,"DOEP: %7d, %7d, %7d ",
											prms->m_hdata[0].u.x,
											prms->m_hdata[0].u.y,
											prms->m_hdata[0].u.z);
					AKMDEBUG(AKMDBG_DOEPLUS,"|%7d, %7d, %7d \n",
											prms->m_hdata_plus[0].u.x,
											prms->m_hdata_plus[0].u.y,
											prms->m_hdata_plus[0].u.z);
				}else{
					// Copy magnetic vector for DOE
					for(i = 0; i < prms->m_hn; i++) {
						prms->m_hdata_plus[i] = prms->m_hdata[i];
					}
				}
				//Calculate Magnetic sensor's offset by DOE
				if (prms->m_en_doeplus == 1) {
				#if 0
					hdSucc = AKSC_HDOEProcessS3(
								prms->m_licenser,
								prms->m_licensee,
								prms->m_key,
								&prms->m_hdoev,
								prms->m_hdata_plus,
								prms->m_hn,
								&prms->m_ho,
								&prms->m_hdst
							 );
				#else				
				//Calculate Magnetic sensor's offset by DOEEx
				hdSucc = AKSC_HDOEEX(
						prms->m_licenser,
						prms->m_licensee,
						prms->m_key,
						prms->m_hdata_plus,
						prms->m_hn,
						prms->m_doeex_var,
						&prms->m_ho,
						&prms->m_hdst
				);
				#endif
				} else {
				#if 0
				hdSucc = AKSC_HDOEProcessS3(
							prms->m_licenser,
							prms->m_licensee,
							prms->m_key,
							&prms->m_hdoev,
							prms->m_hdata,
							prms->m_hn,
							&prms->m_ho,
							&prms->m_hdst
						 );
				#else				
				//Calculate Magnetic sensor's offset by DOEEx
				hdSucc = AKSC_HDOEEX(
						prms->m_licenser,
						prms->m_licensee,
						prms->m_key,
						prms->m_hdata,
						prms->m_hn,
			            prms->m_doeex_var,
						&prms->m_ho,
						&prms->m_hdst
				);
				/*
				ALOGE("AKMD2 ocean AKSC_HDOEEX bdata=%02X,%02X,%02X,%02X,%02X,%02X,%02X,%02X\n",
					bData[0], bData[AKMD_ST2_POS],
					bData[1], bData[2], bData[3], bData[4], bData[5], bData[6]);
				for(i = 0; i < 32; i++)
				{
				ALOGE("AKMD2 ocean AKSC_HDOEEX m_hdata[%d]=%6d,%6d,%6d\n",i,prms->m_hdata[i].u.x, prms->m_hdata[i].u.y, prms->m_hdata[i].u.z);
				}
				ALOGE("AKMD2 ocean AKSC_HDOEEX ho=%6d,%6d,%6d\n",prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z);
				*/
				/*
				ALOGE("AKMD2 oceanbdata  ST1=%6d, ST2=%6d, XYZ(dec)=%6d,%6d,%6d,%6d,%6d,%6d,asa(dec)=%d,%d,%d,hbase(dec)=%ld,%ld,%ld\n",
				bData[0], bData[AKMD_ST2_POS],
				bData[1], bData[2], bData[3], bData[4], bData[5], bData[6],
				prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,
				prms->m_hbase.u.x, prms->m_hbase.u.y, prms->m_hbase.u.z);
*/
/*
				ALOGE("AKMD2 oceanbdata  ST1: %6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%d,%d,%d,%ld,%ld,%ld,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d,%6d\n",
				bData[0], bData[AKMD_ST2_POS],
				bData[1], bData[2], bData[3], bData[4], bData[5], bData[6],
				prms->m_asa.u.x, prms->m_asa.u.y, prms->m_asa.u.z,
				prms->m_hbase.u.x, prms->m_hbase.u.y, prms->m_hbase.u.z,
				prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z,prms->m_hdata[1].u.x, prms->m_hdata[1].u.y, prms->m_hdata[1].u.z,prms->m_hdata[2].u.x, prms->m_hdata[2].u.y, prms->m_hdata[2].u.z,prms->m_hdata[3].u.x, prms->m_hdata[3].u.y, prms->m_hdata[3].u.z,
				prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,prms->m_hdst);
				*/
/*
				ALOGE("AKMD2 oceanhdata m_hdata[0] = %6d,%6d,%6d,m_hdata[1] = %6d,%6d,%6d,m_hdata[2] = %6d,%6d,%6d,m_hdata[3] = %6d,%6d,%6d,ho = %6d,%6d,%6d,m_hdst = %6d\n",
				prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z,prms->m_hdata[1].u.x, prms->m_hdata[1].u.y, prms->m_hdata[1].u.z,prms->m_hdata[2].u.x, prms->m_hdata[2].u.y, prms->m_hdata[2].u.z,prms->m_hdata[3].u.x, prms->m_hdata[3].u.y, prms->m_hdata[3].u.z,
				prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,prms->m_hdst);
*/
				#endif
				}
		
				//ALOGE("AKMD2 ocean DOEEX m_hdst = %d\n",prms->m_hdst);
				
				if (hdSucc == AKSC_CERTIFICATION_DENIED) {
					AKMERROR;
					return AKRET_PROC_FAIL;
				}
				//if (hdSucc > 0) {
					prms->HSUC_HO[prms->m_form] = prms->m_ho;
					prms->m_ho32.u.x = (int32)prms->m_ho.u.x;
					prms->m_ho32.u.y = (int32)prms->m_ho.u.y;
					prms->m_ho32.u.z = (int32)prms->m_ho.u.z;

					prms->HSUC_HDST[prms->m_form] = prms->m_hdst;
#ifdef AKM_EXTERN_HDATA_CHECK
                    prms->HFLUCV_HREF[prms->m_form] = prms->m_hdata[0];
#else
                    prms->HFLUCV_HREF[prms->m_form] = prms->m_hflucv.href;
#endif
					prms->HSUC_HBASE[prms->m_form] = prms->m_hbase;
				//}

				//Set decimator counter
				prms->m_callcnt = hDecimator;//1;
			//ALOGE("AKMD2 ocean DOEEX hDecimator = %d\n",hDecimator);

			}
		}
	}

	if (prms->m_en_doeplus == 1) {
		// Calculate compensated vector
		AKSC_DOEPlus_DistCompen(&have, prms->m_doep_var, &have);
	}

	// Subtract offset and normalize magnetic field vector.
	aksc_ret = AKSC_VNorm(
						  &have,
						  &prms->m_ho,
						  &prms->m_hs,
						  AKSC_HSENSE_TARGET,
						  &hvec
						  );
	if (aksc_ret == 0) {
		AKMERROR;
		ALOGE("AKSC_VNorm failed.\n"
				"  have=%6d,%6d,%6d  ho=%6d,%6d,%6d  hs=%6d,%6d,%6d\n",
				have.u.x, have.u.y, have.u.z,
				prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,
				prms->m_hs.u.x, prms->m_hs.u.y, prms->m_hs.u.z);
		//ret |= AKRET_VNORM_ERROR;
		//return ret;
	}
	//ALOGE("akm_log usb status m_usb_online %d ,m_pre_usb_online %d ,prms->m_usb_changed %d\n",prms->m_usb_online,prms->m_pre_usb_online,prms->m_usb_changed);
	if(prms->m_usb_online != prms->m_pre_usb_online)
	//else if((prms->m_usb_online != prms->m_pre_usb_online)&&(prms->m_usb_changed == 0))
	{	
		 prms->m_pre_usb_online = prms->m_usb_online;
		//ALOGE("akm_log usb_online the changed!!!!!"); 
		//		AKSC_SetHDOEEXLevel(
		//			prms->m_doeex_var,
		//			&prms->m_ho,
		//            AKSC_HDST_UNSOLVED,
		//            1
		//		);
		//	prms->m_hdst = AKSC_HDST_UNSOLVED;
			ALOGE("akm_log usb_changed set lv 0!!!");
	}
	// hvec is updated only when VNorm function is succeeded.
	prms->m_hvec = hvec;

	// Bias of Uncalibrated Magnetic Field
	prms->m_bias.u.x = (int32)(prms->m_ho.u.x) + prms->m_hbase.u.x;
	prms->m_bias.u.y = (int32)(prms->m_ho.u.y) + prms->m_hbase.u.y;
	prms->m_bias.u.z = (int32)(prms->m_ho.u.z) + prms->m_hbase.u.z;

	//Convert layout from sensor to Android by using PAT number.
	// Magnetometer
	ConvertCoordinate(prms->m_hlayout, &prms->m_hvec);
	// Bias of Uncalibrated Magnetic Field
	ConvertCoordinate32(prms->m_hlayout, &prms->m_bias);
	// Magnetic Field
	prms->m_calib.u.x = prms->m_hvec.u.x;
	prms->m_calib.u.y = prms->m_hvec.u.y;
	prms->m_calib.u.z = prms->m_hvec.u.z;

	// Uncalibrated Magnetic Field
	prms->m_uncalib.u.x = (int32)(prms->m_calib.u.x) + prms->m_bias.u.x;
	prms->m_uncalib.u.y = (int32)(prms->m_calib.u.y) + prms->m_bias.u.y;
	prms->m_uncalib.u.z = (int32)(prms->m_calib.u.z) + prms->m_bias.u.z;
    //ALOGE("MagUC : 00:00:00.000, %f, %f, %f\n",prms->m_uncalib.u.x*0.06f,prms->m_uncalib.u.y*0.06f,prms->m_uncalib.u.z*0.06f);
	AKMDEBUG(AKMDBG_VECTOR,
			"mag(dec)=%6d,%6d,%6d\n"
			"maguc(dec),bias(dec)=%7ld,%7ld,%7ld,%7ld,%7ld,%7ld\n",
			prms->m_calib.u.x, prms->m_calib.u.y, prms->m_calib.u.z,
			prms->m_uncalib.u.x, prms->m_uncalib.u.y, prms->m_uncalib.u.z,
			prms->m_bias.u.x, prms->m_bias.u.y, prms->m_bias.u.z);
			
	//prms->m_uncalib.u.x = DISP_CONV_AKSCF(prms->m_uncalib.u.x);
	//prms->m_uncalib.u.y = DISP_CONV_AKSCF(prms->m_uncalib.u.y);
	//prms->m_uncalib.u.z = DISP_CONV_AKSCF(prms->m_uncalib.u.z);
	//ALOGE("maguc(dec) = %d, %d, %d\n",prms->m_uncalib.u.x, prms->m_uncalib.u.y, prms->m_uncalib.u.z);

	return AKRET_PROC_SUCCEED;
}

/*!
 Calculate Yaw, Pitch, Roll angle.
 m_hvec, m_avec and m_gvec should be Android coordination.
 @return Always return #AKRET_PROC_SUCCEED.
 @param[in,out] prms A pointer to a #AKSCPRMS structure.
 */
int16 CalcDirection(AKSCPRMS* prms)
{
	/* Conversion matrix from Android to SmartCompass coordination */
	int16 preThe, swp;
	const I16MATRIX hlayout = {{
								 0, 1, 0,
								-1, 0, 0,
								 0, 0, 1}};
	const I16MATRIX alayout = {{
								 0,-1, 0,
								 1, 0, 0,
								 0, 0,-1}};

	preThe = prms->m_theta;

	prms->m_d6dRet = AKSC_DirectionS3(
			prms->m_licenser,
			prms->m_licensee,
			prms->m_key,
			&prms->m_hvec,
			&prms->m_avec,
			&prms->m_dvec,
			&hlayout,
			&alayout,
			&prms->m_theta,
			&prms->m_delta,
			&prms->m_hr,
			&prms->m_hrhoriz,
			&prms->m_ar,
			&prms->m_phi180,
			&prms->m_phi90,
			&prms->m_eta180,
			&prms->m_eta90,
			&prms->m_mat,
			&prms->m_quat);
		/*
		ALOGE("AKSC_Direction6D hvec=%d,%d,%d  avec=%d,%d,%d  dvec=%d,%d,%d\n",
				prms->m_hvec.u.x, prms->m_hvec.u.y, prms->m_hvec.u.z,
				prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z,
				prms->m_dvec.u.x, prms->m_dvec.u.y, prms->m_dvec.u.z);
		*/
	prms->m_theta =	AKSC_ThetaFilter(
			prms->m_theta,
			preThe,
			THETAFILTER_SCALE);

	if (prms->m_d6dRet == AKSC_CERTIFICATION_DENIED) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	if (prms->m_d6dRet != 3) {
		AKMDEBUG(AKMDBG_DUMP,
				"AKSC_Direction6D failed (0x%02x).\n"
				"  hvec=%d,%d,%d  avec=%d,%d,%d  dvec=%d,%d,%d\n",
				prms->m_d6dRet,
				prms->m_hvec.u.x, prms->m_hvec.u.y, prms->m_hvec.u.z,
				prms->m_avec.u.x, prms->m_avec.u.y, prms->m_avec.u.z,
				prms->m_dvec.u.x, prms->m_dvec.u.y, prms->m_dvec.u.z);
	}

	/* Convert Yaw, Pitch, Roll angle to Android coordinate system */
	if (prms->m_d6dRet & 0x02) {
		/*
		 from: AKM coordinate, AKSC units
		 to  : Android coordinate, AKSC units. */
		prms->m_eta180 = -prms->m_eta180;
		prms->m_eta90  = -prms->m_eta90;
		/*
		 from: AKM coordinate, AKSC units
		 to  : Android coordinate, AKSC units. */
		swp = prms->m_quat.u.x;
		prms->m_quat.u.x = prms->m_quat.u.y;
		prms->m_quat.u.y = -(swp);
		prms->m_quat.u.z = -(prms->m_quat.u.z);
		#ifdef ENABLE_ACC_FILTER
    if (phone_moving == 0) {
       prms->m_theta = akm_sensor_avg(prms->m_theta);
	//ALOGE("AKMD2 ocean akm_sensor_avg prms->m_theta = %d\n",prms->m_theta);
    } else {
        akm_sensor_deinit();
    }
#endif

/*
		ALOGE("AKSC_Direction6D (0x%02x):\n"
				"  Yaw, Pitch, Roll=%6.1f,%6.1f,%6.1f\n",
				prms->m_d6dRet,
				DISP_CONV_Q6F(prms->m_theta),
				DISP_CONV_Q6F(prms->m_phi180),
				DISP_CONV_Q6F(prms->m_eta90));
*/
	}

	return AKRET_PROC_SUCCEED;
}

int16 SimpleCalibration(AKSCPRMS* prms)
{
	/* Boot up device */
	if (AKD_AccSetEnable(AKD_ENABLE) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}
	if (AKD_AccSetDelay(AKMD_ACC_INTERVAL) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	/* Wait for a while until device boot up */
	msleep(100);

	AKD_GetAccelerationOffset(prms->m_AO.v);

	if (AKD_AccSetEnable(AKD_DISABLE) != AKD_SUCCESS) {
		AKMERROR;
		return AKRET_PROC_FAIL;
	}

	return AKRET_PROC_SUCCEED;
}



/*!
 Calculate angular speed.
 m_hvec and m_avec should be Android coordination.
 @return
 @param[in,out] prms A pointer to a #AKSCPRMS structure.
 */
int16 CalcAngularRate(AKSCPRMS* prms)
{
	/* Conversion matrix from Android to SmartCompass coordination */
	const I16MATRIX hlayout = {{
								 0, 1, 0,
								-1, 0, 0,
								 0, 0, 1}};
	const I16MATRIX alayout = {{
								 0,-1, 0,
								 1, 0, 0,
								 0, 0,-1}};

	AKSC_FVEC	dhvec;
	AKSC_FVEC	davec;
	AKSC_FLOAT	swp;
	int16vec tmp_hvec;
	int16 aksc_ret;

	// Subtract offset from non-averaged value.
	aksc_ret = AKSC_VNorm(
						  &prms->m_hdata[0],
						  &prms->m_ho,
						  &prms->m_hs,
						  AKSC_HSENSE_TARGET,
						  &tmp_hvec
						  );
	if (aksc_ret == 0) {
		AKMERROR;
		AKMDEBUG(AKMDBG_DUMP,"AKSC_VNorm failed.\n"
				"  have=%6d,%6d,%6d  ho=%6d,%6d,%6d  hs=%6d,%6d,%6d\n",
				prms->m_hdata[0].u.x, prms->m_hdata[0].u.y, prms->m_hdata[0].u.z,
				prms->m_ho.u.x, prms->m_ho.u.y, prms->m_ho.u.z,
				prms->m_hs.u.x, prms->m_hs.u.y, prms->m_hs.u.z);
		return AKRET_PROC_FAIL;
	}

	// Convert to Android coordination
	ConvertCoordinate(prms->m_hlayout, &tmp_hvec);

	dhvec.u.x = tmp_hvec.u.x;
	dhvec.u.y = tmp_hvec.u.y;
	dhvec.u.z = tmp_hvec.u.z;

	davec.u.x = prms->m_avec.u.x;
	davec.u.y = prms->m_avec.u.y;
	davec.u.z = prms->m_avec.u.z;
	
	prms->m_pgRet = AKSC_PseudoGyro(
						&prms->m_pgcond,
						prms->m_pgdt,
						&dhvec,
						&davec,
						&hlayout,
						&alayout,
						&prms->m_pgvar,
						&prms->m_pgout,
						&prms->m_pgquat,
						&prms->m_pgGravity,
						&prms->m_pgLinAcc
					);

	if(prms->m_pgRet != 1) {
		AKMERROR;
		AKMDEBUG(AKMDBG_DUMP,"AKSC_PseudoGyro failed: freq=%d\n"
				"  hvec=%8.1f,%8.1f,%8.1f  avec=%8.1f,%8.1f,%8.1f\n",
				prms->m_pgdt,
				dhvec.u.x, dhvec.u.y, dhvec.u.z,
				davec.u.x, davec.u.y, davec.u.z);
		return AKRET_PROC_FAIL;
	} else {
		/* Convertion: */

		/*
		 from: AKM coordinate, AKSC units
		 to  : Android coordinate, Android units. */
		swp = prms->m_pgout.u.x;
		prms->m_pgout.u.x = -DEG2RAD(prms->m_pgout.u.y);
		prms->m_pgout.u.y = DEG2RAD(swp);
		prms->m_pgout.u.z = DEG2RAD(prms->m_pgout.u.z);
		/*
		 from: AKM coordinate
		 to  : Android coordinate */
		swp = prms->m_pgquat.u.x;
		prms->m_pgquat.u.x = prms->m_pgquat.u.y;
		prms->m_pgquat.u.y = -(swp);
		prms->m_pgquat.u.z = -(prms->m_pgquat.u.z);
		/*
		 from: AKM coordinate, AKSC units
		 to  : Android coordinate, Android units. */
		swp = prms->m_pgGravity.u.x;
		prms->m_pgGravity.u.x = AKSC2SI(prms->m_pgGravity.u.y);
		prms->m_pgGravity.u.y = -AKSC2SI(swp);
		prms->m_pgGravity.u.z = -AKSC2SI(prms->m_pgGravity.u.z);
		/*
		 from: AKM coordinate, AKSC units
		 to  : Android coordinate, Android units. */
		swp = prms->m_pgLinAcc.u.x;
		prms->m_pgLinAcc.u.x = AKSC2SI(prms->m_pgLinAcc.u.y);
		prms->m_pgLinAcc.u.y = -AKSC2SI(swp);
		prms->m_pgLinAcc.u.z = -AKSC2SI(prms->m_pgLinAcc.u.z);

		AKMDEBUG(AKMDBG_DUMP, "AKSC_PseudoGyro:\n"
			"  dt=%d rate=%8.5f,%8.5f,%8.5f quat=%8.5f,%8.5f,%8.5f,%8.5f\n",
			prms->m_pgdt,
			prms->m_pgout.u.x,
			prms->m_pgout.u.y,
			prms->m_pgout.u.z,
			prms->m_pgquat.u.x,
			prms->m_pgquat.u.y,
			prms->m_pgquat.u.z,
			prms->m_pgquat.u.w);

	}

	return AKRET_PROC_SUCCEED;
}

