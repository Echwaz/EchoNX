#pragma once
#ifdef __SWITCH__
#include <switch.h>
#endif
namespace echo {
inline void prefer_cpu(int preferred){
#ifdef __SWITCH__
    u64 allowed=0;if(R_FAILED(svcGetInfo(&allowed,InfoType_CoreMask,CUR_PROCESS_HANDLE,0)))return;
    allowed&=15;if(!allowed)return;if(!(allowed&(1ULL<<preferred))){for(preferred=0;preferred<4;preferred++)if(allowed&(1ULL<<preferred))break;}
    svcSetThreadCoreMask(CUR_THREAD_HANDLE,preferred,u32(allowed));
#else
    (void)preferred;
#endif
}
}