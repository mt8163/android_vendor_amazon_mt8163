
#ifndef __MTKOMXVENC_PROPERTIES_H__
#define __MTKOMXVENC_PROPERTIES_H__

#include <stdlib.h>
#include <string.h>
#include <cutils/properties.h>

#define SELF_DEFINE_SYSTEM_PROPERTY(TYPE, FUNC, VAR, DEFAULT) \
    TYPE FUNC(){return FUNC(DEFAULT);} \
    TYPE FUNC(const char* VAR)

#define DEFINE_SYSTEM_PROPERTY(TYPE, FUNC, KEY, DEFAULT) \
    TYPE FUNC(){return FUNC(DEFAULT);} \
    TYPE FUNC(const char* d) { \
        char value[PROPERTY_VALUE_MAX]; \
        property_get(KEY, value, d); \
        return (TYPE) atoi(value); \
    }

#define DECLARE_SYSTEM_PROPERTY(TYPE, FUNC) \
    TYPE FUNC(); \
    TYPE FUNC(const char*)

namespace MtkVenc {

DECLARE_SYSTEM_PROPERTY(bool, EnableMoreLog);
DECLARE_SYSTEM_PROPERTY(int, MaxScaledWide);
DECLARE_SYSTEM_PROPERTY(int, MaxScaledNarrow);
DECLARE_SYSTEM_PROPERTY(int, DumpCCNum);
DECLARE_SYSTEM_PROPERTY(int, WatchdogTimeout);
DECLARE_SYSTEM_PROPERTY(bool, EnableVencLog);
DECLARE_SYSTEM_PROPERTY(bool, InputScalingMode);
DECLARE_SYSTEM_PROPERTY(bool, DrawStripe);
DECLARE_SYSTEM_PROPERTY(bool, DumpInputFrame);
DECLARE_SYSTEM_PROPERTY(bool, DumpCts);
DECLARE_SYSTEM_PROPERTY(bool, RTDumpInputFrame);
DECLARE_SYSTEM_PROPERTY(bool, DumpColorConvertFrame);
DECLARE_SYSTEM_PROPERTY(bool, EnableDummy);
DECLARE_SYSTEM_PROPERTY(bool, DumpDLBS);
DECLARE_SYSTEM_PROPERTY(bool, IsMtklog);
DECLARE_SYSTEM_PROPERTY(bool, IsViLTE);
DECLARE_SYSTEM_PROPERTY(bool, AVPFEnable);
DECLARE_SYSTEM_PROPERTY(bool, RecordBitstream);
DECLARE_SYSTEM_PROPERTY(bool, WFDLoopbackMode);
DECLARE_SYSTEM_PROPERTY(bool, DumpSecureInputFlag);
DECLARE_SYSTEM_PROPERTY(bool, DumpSecureTmpInFlag);
DECLARE_SYSTEM_PROPERTY(bool, DumpSecureOutputFlag);
DECLARE_SYSTEM_PROPERTY(bool, DumpSecureYv12Flag);

DECLARE_SYSTEM_PROPERTY(bool, DumpLog);
DECLARE_SYSTEM_PROPERTY(int, BufferCountActual);
DECLARE_SYSTEM_PROPERTY(int, BufferSize);
DECLARE_SYSTEM_PROPERTY(long, MaxDramSize);
DECLARE_SYSTEM_PROPERTY(bool, SvpSupport);
DECLARE_SYSTEM_PROPERTY(bool, TrustTonicTeeSupport);
DECLARE_SYSTEM_PROPERTY(bool, InHouseReady);
DECLARE_SYSTEM_PROPERTY(bool, MicroTrustTeeSupport);
DECLARE_SYSTEM_PROPERTY(bool, TeeGpSupport);
DECLARE_SYSTEM_PROPERTY(int, MBAFF);

DECLARE_SYSTEM_PROPERTY(int, BsSize);
DECLARE_SYSTEM_PROPERTY(bool, DumpBs);
DECLARE_SYSTEM_PROPERTY(bool, DumpVtBs);
DECLARE_SYSTEM_PROPERTY(bool, UnsupportPrepend);
DECLARE_SYSTEM_PROPERTY(bool, DisableANWInMetadata);

} //NS MtkVenc

#endif //__MTKOMXVENC_PROPERTIES_H__