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
#include "LGAudioFormatConverter.h"

typedef struct
{
    S32 mSampleRate;
    S32 mChannel;
    S32 mRate;
    F32 speed;
    F32 volume;
    F32 pitch;
    F32 rate;
    S32 ShiftIndex;
}TSInfo_type;

class LGTimeStretch : public LGEffectChainBase
{
private:
    TSInfo_type mTSInfo;
    float resamplingFactor;
    float prevresamplingFactor;
    LGE_EFFECTCHAIN_AUDIO_FORMAT mAudioFormat;
    LGAudioFormatConverter *mConverterManager;

    float getRate(int);
    int getShiftIndex(int);

    bool IsFirstFrame;
    int GetDesiredOutputLength(LGE_EFFECTCHAIN_SPEED_RATE, int);
    S32 processReal(S8* in, S8* out, S32);
    S32 processReal(S16* in, S16* out, S32);
    S32 processReal(S32* in, S32* out, S32);
    S32 processReal(F32* in, F32* out, S32);
    S32 processReal24Bit(S24* in, S24* out, S32, LGE_EFFECTCHAIN_AUDIO_FORMAT);

public:
    LGTimeStretch();
    ~LGTimeStretch();

    bool init();
    void setParamAudioInfo(S32, S32, S32, S32);
    void setParamChainInfo(ChainInfo *);
    S32 process(void*, void*, S32);
    void deinit();
};