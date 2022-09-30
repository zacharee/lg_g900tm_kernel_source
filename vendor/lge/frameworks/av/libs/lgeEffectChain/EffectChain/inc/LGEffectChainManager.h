/*
* Multimedia Team, Mobile Communication Company, LG ELECTRONICS INC., SEOUL, KOREA
* Copyright(c) 2016 by LG Electronics Inc.
*
* All rights reserved. No part of this work may be reproduced, stored in a
* retrieval system, or transmitted by any means without prior written
* Permission of LG Electronics Inc.
*/
#pragma once
#include "LGEffectChainBase.h"
#include "LGEffectChainType.h"
#include "LGEffectChainQueue.h"
#include "LGEffectChainFactory.h"

//#include "LGAudioEffectUtil.h"        //deprecated_audioeffect
#include "LGEffectChainType.h"
#include "LGEffectChainAux.h"           //For BGM, not belong to effect chain.

class LGEffectChainManager
{
private:
    LGEffectChainFactory mChainFactory;
    ChainInfo mCurChainInfo;
    LGEffectChainQueue<LGE_EFFECTCHAIN_TYPE> mChainQueue;

    LGEffectChainBase *mCurChain[LGE_EFFECTCHAIN_TYPE_MAX-1];
    LGEffectChainBase *mAux;                    //for bgm
    // Compulsory param number in mCurChainInfo
    const int NumCompulsaryParams;

    bool createNewChain(LGE_EFFECTCHAIN_TYPE, LGEffectChainBase**);

    bool IsValidChainInfo(const  ChainInfo *Info);

    bool IsAudioEffectEnabled;
    bool IsBGMMixEnabled;
    bool IsTimeStretchEnabled;
    bool IsAudioFormatConverterEnabled;
    bool IsDynamicRangeCompressorEnabled;
    bool deInitComplete;
    int getShiftIndex(LGE_EFFECTCHAIN_SPEED_RATE);

public:
    LGEffectChainManager();
    ~LGEffectChainManager();

    void setParamAudioInfo(int, int, int, int);
    void setParamChainType(int,  void*);
    void setParamInProcess(int efType,int mode);
    void setParamAuxInfo(int, int, int, int);       //for bgm
    int writeAux(U8* auxData, int samples, bool lastData);
    int processEffectChain(void*, void*, int);
    void deinit();

    void EnQueueChain(LGE_EFFECTCHAIN_TYPE);
    U8* allocateBuff(U8*, int, int, LGE_EFFECTCHAIN_SPEED_RATE);
    U8* allocateBuff_ex(U8*  buff, int lengthperCh, int numCh, LGE_EFFECTCHAIN_SPEED_RATE rate);
};
