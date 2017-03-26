#ifndef __REMIX_FLAG_PRIVATE_H__
#define __REMIX_FLAG_PRIVATE_H__

#ifdef REMIX_SEMGROUPFLAG

#define REMIXFLAGWAITTYPEMASK        0x0000000F
#define REMIXFLAGCONSUMEMASK         0x00000010
#define REMIXFLAGSCHEDMASK           0x00000060
#define REMIXFLAGSETMASK             0x00000001

extern U32 REMIX_FlagFlushValue(REMIX_FLAG * pstrFlag, U32 uiRtnValue);

#endif

#endif
