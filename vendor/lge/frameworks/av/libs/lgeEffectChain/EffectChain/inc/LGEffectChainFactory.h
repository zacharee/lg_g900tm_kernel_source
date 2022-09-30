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

class LGEffectChainFactory
{
private:
    bool IsAudioEffectCreated;
    bool IsTimeStretchCreated;
    bool IsBGMMixCreated;
    bool IsAudioFormatConverterCreated;
    bool IsDynamicRangeCompressorCreated;

public:
    LGEffectChainFactory() : IsAudioEffectCreated(false), IsTimeStretchCreated(false), IsBGMMixCreated(false), IsAudioFormatConverterCreated(false), IsDynamicRangeCompressorCreated(false){};
    ~LGEffectChainFactory() {};

    bool createEffectChain(int, LGEffectChainBase**);
};