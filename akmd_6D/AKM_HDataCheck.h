#ifndef INC_AKM_HDATACHECK_H
#define INC_AKM_HDATACHECK_H

#include "AKCompass.h"

#define AKM_EXTERN_HDATA_CHECK

/* counter of hdata checking */
#define CSPEC_HDATA_CNT         30
#define CSPEC_HDATA_CHANGE_TH   100

#define CSPEC_MAG_NORMAL_MIN    400
#define CSPEC_MAG_NORMAL_MAX    1000

#define AKM_HDATA_CHECK_SUCCESS             0
#define AKM_HDATA_CHECK_RANGE_EXCEEDED      1
#define AKM_HDATA_CHECK_RADIUS_NOT_MATCH    2

void AKM_HData_CheckInit(
   const int16vec href,
   const int16vec ho,
   const int32vec hbase,
   int32vec *hcenter,
   int32    *ref_norm
);

int16 AKM_HData_Check(
    const int16vec hdata,
    const int32vec hbase,
    const int32vec hcenter,
    const int32    refNorm,
    const int16    th
);

#endif
