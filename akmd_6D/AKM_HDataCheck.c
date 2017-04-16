#include "AKM_HDataCheck.h"
#include <math.h>

#define NORMALIZE(x, y, z)  ((x*x)+(y*y)+(z*z))

void AKM_HData_CheckInit(
   const int16vec href,
   const int16vec ho,
   const int32vec hbase,
   int32vec* hcenter,
   int32*    ref_norm
) {
    int16vec vec;

    hcenter->u.x = (int32)ho.u.x + hbase.u.x;
    hcenter->u.y = (int32)ho.u.y + hbase.u.y;
    hcenter->u.z = (int32)ho.u.z + hbase.u.z;

    vec.u.x = href.u.x - ho.u.x;
    vec.u.y = href.u.y - ho.u.y;
    vec.u.z = href.u.z - ho.u.z;
    *ref_norm = sqrt(NORMALIZE(vec.u.x, vec.u.y, vec.u.z));

    return;
}

int16 AKM_HData_Check(
    const int16vec hdata,
    const int32vec hbase,
    const int32vec hcenter,
    const int32    refNorm,
    const int16    th
) {
    int32vec r = {{0, 0, 0}};
    int32vec temp_data  = {{0, 0, 0}};
    uint32   r_norm = 0;
    uint32   normal_ho = 0;
    int32    normal_diff  = 0;
    int16    ret = -1;


    temp_data.u.x = (int32)hdata.u.x + hbase.u.x;
    temp_data.u.y = (int32)hdata.u.y + hbase.u.y;
    temp_data.u.z = (int32)hdata.u.z + hbase.u.z;
    
    r.u.x = temp_data.u.x - hcenter.u.x;
    r.u.y = temp_data.u.y - hcenter.u.y;
    r.u.z = temp_data.u.z - hcenter.u.z;
    r_norm = sqrt(NORMALIZE(r.u.x, r.u.y, r.u.z));

    if (r_norm < CSPEC_MAG_NORMAL_MIN || r_norm > CSPEC_MAG_NORMAL_MAX) {
        return AKM_HDATA_CHECK_RANGE_EXCEEDED;
    }

    normal_diff = abs(r_norm - refNorm);

    if (normal_diff < th) {
        ret = AKM_HDATA_CHECK_SUCCESS;
    } else {
        ret = AKM_HDATA_CHECK_RADIUS_NOT_MATCH;
    }

    ALOGE("ret,%d, "
          "hdata,%d,%d,%d, "
          "temp,%ld,%ld,%ld, "
          "hcenter,%ld,%ld,%ld, "
          "r,%ld,%ld,%ld,%ld, "
          "refNorm:%ld, "
          "diff,%ld, th,%d\n",
          ret,
          hdata.u.x, hdata.u.y, hdata.u.z,
          temp_data.u.x, temp_data.u.y, temp_data.u.z,
          hcenter.u.x, hcenter.u.y, hcenter.u.z,
          r.u.x, r.u.y, r.u.z, r_norm,
          refNorm,
          normal_diff, th
    );

    return ret;
}
