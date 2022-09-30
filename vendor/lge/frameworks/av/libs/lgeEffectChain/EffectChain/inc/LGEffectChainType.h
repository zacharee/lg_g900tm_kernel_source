/*
* Multimedia Team, Mobile Communication Company, LG ELECTRONICS INC., SEOUL, KOREA
* Copyright(c) 2016 by LG Electronics Inc.
*
* All rights reserved. No part of this work may be reproduced, stored in a
* retrieval system, or transmitted by any means without prior written
* Permission of LG Electronics Inc.
*/
#pragma once

#include <stdio.h>
#include "LGEffectChainUtil.h"

/****************************************************************************************
Macro defs
*****************************************************************************************/
#define GET_MAX(a, b) a>b ? a: b
#define GET_BYTE_SIZE(a) a<4 ? a:4

/****************************************************************************************
Basic Type defs
*****************************************************************************************/
typedef char                            S8;
typedef unsigned char                   U8;
typedef short                           S16;
typedef int                             S24;
typedef int                             S32;
typedef long long                       S64;
typedef float                           F32;
typedef double                          F64;
typedef bool                            Bool;


/****************************************************************************************
Basic Audio Information defs
*****************************************************************************************/

typedef enum
{
    LGE_EFFECTCHAIN_FS_NOT_DEFINED = -1,
    LGE_EFFECTCHAIN_FS_8000 = 8000,
    LGE_EFFECTCHAIN_FS_11025 = 11025,
    LGE_EFFECTCHAIN_FS_12000 = 12000,
    LGE_EFFECTCHAIN_FS_16000 = 16000,
    LGE_EFFECTCHAIN_FS_22050 = 22050,
    LGE_EFFECTCHAIN_FS_24000 = 24000,
    LGE_EFFECTCHAIN_FS_32000 = 32000,
    LGE_EFFECTCHAIN_FS_44100 = 44100,
    LGE_EFFECTCHAIN_FS_48000 = 48000,
    LGE_EFFECTCHAIN_FS_88200 = 88200,
    LGE_EFFECTCHAIN_FS_96000 = 96000,
    LGE_EFFECTCHAIN_FS_176400 = 176400,
    LGE_EFFECTCHAIN_FS_192000 = 192000,
    LGE_EFFECTCHAIN_FS_352800 = 352800,
    LGE_EFFECTCHAIN_FS_384000 = 384000,
    LGE_EFFECTCHAIN_FS_768000 = 768000,             //dummy for time stretch
} LGE_EFFECTCHAIN_FS;

typedef enum
{
    LGE_EFFECTCHAIN_CH_NUM_NOT_DEFINED = -1,
    LGE_EFFECTCHAIN_CH_NUM_MONO = 1,
    LGE_EFFECTCHAIN_CH_NUM_STEREO = 2,
    LGE_EFFECTCHAIN_CH_NUM_6 = 6,
    LGE_EFFECTCHAIN_CH_NUM_8 = 8
} LGE_EFFECTCHAIN_CH_NUM;

typedef enum
{
    LGE_EFFECTCHAIN_CH_TYPE_NONE = 0,
    LGE_EFFECTCHAIN_CH_TYPE_INTERLACED,
    LGE_EFFECTCHAIN_CH_TYPE_DEINTERLACED,
} LGE_EFFECTCHAIN_CH_TYPE;

typedef enum
{
    LGE_EFFECTCHAIN_AUDIO_FORMAT_NOT_DEFINED = -1,
    LGE_EFFECTCHAIN_AUDIO_FORMAT_NONE = 0,
    LGE_EFFECTCHAIN_AUDIO_FORMAT_8_BIT,
    LGE_EFFECTCHAIN_AUDIO_FORMAT_16_BIT,
    LGE_EFFECTCHAIN_AUDIO_FORMAT_24_BIT_PACKED,
    LGE_EFFECTCHAIN_AUDIO_FORMAT_32_BIT,
    LGE_EFFECTCHAIN_AUDIO_FORMAT_8_24_BIT,
    LGE_EFFECTCHAIN_AUDIO_FORMAT_24_8_BIT,
    LGE_EFFECTCHAIN_AUDIO_FORMAT_FLOAT
} LGE_EFFECTCHAIN_AUDIO_FORMAT;

/****************************************************************************************
Audio Format Converter defs
*****************************************************************************************/

#define MAX_CONVERTER_FUNC 3 // resampler, rechannel, bit depth change

typedef enum
{
    LGE_EFFECTCHAIN_NOT_DEFINED = -1,
    LGE_EFFECTCHAIN_RESAMPLE = 0,
    LGE_EFFECTCHAIN_RECHANNEL,
    LGE_EFFECTCHAIN_BITDEPTHCHANGE,
    LGE_EFFECTCHAIN_CONVETERTER_MAX
} AudioFormatConvertType;

typedef struct
{
    LGE_EFFECTCHAIN_AUDIO_FORMAT mAudioFormat;
    LGE_EFFECTCHAIN_CH_NUM              mChNum;
    LGE_EFFECTCHAIN_FS                          mSampleRate;
} AudioInfo;

typedef struct
{
    AudioFormatConvertType                  mConverterType;
    LGE_EFFECTCHAIN_AUDIO_FORMAT mAudioFormat;
    LGE_EFFECTCHAIN_CH_NUM              mChNum;
    LGE_EFFECTCHAIN_FS                          mSampleRate;
} TargetFormatInfo;

typedef struct
{
    AudioInfo               mOriginalAudioFormat;
    TargetFormatInfo    mTargetAudioFormat;
} ConverterInfo;

/****************************************************************************************
Effect Chain type defs
*****************************************************************************************/
typedef short LGEffectChainDataType;

typedef enum
{
    LGE_EFFECTCHAIN_TYPE_NONE = 0,
    LGE_EFFECTCHAIN_TYPE_AUDIOEFFECT,        //deprecated_audioeffect
    LGE_EFFECTCHAIN_TYPE_TIMESTRETCH,
    LGE_EFFECTCHAIN_TYPE_BGMMIX,
    LGE_EFFECTCHAIN_TYPE_AUDIOFORMATCONVERTER,
    LGE_EFFECTCHAIN_TYPE_DYNAMICRANGECOMPRESSOR,        //deprecated_DRC
    LGE_EFFECTCHAIN_TYPE_MAX
} LGE_EFFECTCHAIN_TYPE;

typedef enum
{
    LGE_EFFECTCHAIN_ORIGINAL_RATE = 0,
    LGE_EFFECTCHAIN_ONEEIGHTH_RATE,
    LGE_EFFECTCHAIN_QUARTER_RATE,
    LGE_EFFECTCHAIN_HALF_RATE,
    LGE_EFFECTCHAIN_DOUBLE_RATE,
    LGE_EFFECTCHAIN_QUAD_RATE,
    LGE_EFFECTCHAIN_OCTA_RATE
}LGE_EFFECTCHAIN_SPEED_RATE;

typedef enum
{
    LGE_EFFECTCHAIN_MIXER_MIX = 0,
    LGE_EFFECTCHAIN_MIXER_LAST_CHUNK ,
    LGE_EFFECTCHAIN_MIXER_PAUSE,
    LGE_EFFECTCHAIN_MIXER_RESUME,
    LGE_EFFECTCHAIN_MIXER_STOP,
    LGE_EFFECTCHAIN_MIXER_FLUSH                //erase
}LGE_EFFECTCHAIN_MXER_CONTROL;

typedef struct
{
    S32 mPath;
    S32 mMedia;
    S32 mAudioEffectType;
} effectInfo_type;

typedef struct
{
    LGE_EFFECTCHAIN_SPEED_RATE mTSRate;
} TimeStretchInfo_type;

typedef struct
{
    F32 mMakeupGain;
    Bool mPreFilter;
    S16 mGateThreshold;
    S16 mExpanderThreshold;
    F32 mExpanderSlope;
    S16 mCompressorThreshold;
    F32 mCompressorSlope;
    S16 mLimiterThreshold;
    F32 mLimiterSlope;
    F32 mGainBias;
} DRCInfo_type;

typedef struct
{
    S32 useMixerGain;
    S32 srcGain;
    S32 bgmGain;
    S32 masterGain;
    S32 mixRatio;   //mix ratio and gain are exclusive.
    S32 mixerControl;
} MixerInfo_type;

typedef struct _ChainInfo {
    S32 mAudioFormat;
    S32 mChType;
    S32 mChannel;
    S32 mSampleRate;
    S32 mEffectChainType;
    S32 mPath;
    S32 mMedia;
    S32 mAudioEffectType;
    S32 mTSRate;
    void* mAuxObj;
    TargetFormatInfo mTargetAudioFormat;
    DRCInfo_type mDRCInfo;              //for DRC
    MixerInfo_type mMixerGain;
} ChainInfo;

typedef struct
{
    F32 ratio;
    LGE_EFFECTCHAIN_FS sampleRate;
    LGE_EFFECTCHAIN_CH_NUM numChannel;
    short* BGMBuff;
} BGMInfo_type;

typedef struct
{
    LGE_EFFECTCHAIN_AUDIO_FORMAT mAudioFormat;
    LGE_EFFECTCHAIN_CH_NUM mChannel;
    LGE_EFFECTCHAIN_CH_TYPE mChType;
    LGE_EFFECTCHAIN_FS mSampleRate;
    LGE_EFFECTCHAIN_TYPE mEffectType;
} PCMInfo_type;

