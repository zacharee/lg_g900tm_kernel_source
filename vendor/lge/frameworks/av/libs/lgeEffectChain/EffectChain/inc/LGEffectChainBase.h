/*
* Multimedia Team, Mobile Communication Company, LG ELECTRONICS INC., SEOUL, KOREA
* Copyright(c) 2016 by LG Electronics Inc.
*
* All rights reserved. No part of this work may be reproduced, stored in a
* retrieval system, or transmitted by any means without prior written
* Permission of LG Electronics Inc.
*/
#pragma once
#include "LGEffectChainType.h"
#include "LGEffectChainUtil.h"

#include <string.h>
class LGEffectChainBase
{
private:
public:
    virtual bool init() = 0;
    virtual void setParamAudioInfo(int, int, int, int) = 0;
    virtual void setParamChainInfo(ChainInfo *) = 0;

    virtual int process(void*, void*, int) = 0;
    virtual void deinit() = 0;
    virtual ~LGEffectChainBase() {};
};