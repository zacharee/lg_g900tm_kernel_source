#pragma once

#include "stdefs.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include "smallfilter.h"
#include "largefilter.h"
#include "ResampleInterface.h"
//#include "filterkit.h"

#define IBUFFSIZE 4096*8                         /* Input buffer size per channel */
#define OBUFFSIZE 4096 * 4                       /*use only for file*/

#define fileread 1
#define resample_Mem2 1


/* Conversion constants */
#define Nhc       8
#define Na        7
#define Np       (Nhc+Na)
#define Npc      (1<<Nhc)
#define Amask    ((1<<Na)-1)
#define Pmask    ((1<<Np)-1)
#define Nh       16
#define Nb       16
#define Nhxn     14
#define Nhg      (Nh-Nhxn)
#define NLpScl   13

typedef char BOOL;


typedef enum
{
    R_MODE_NONE     = 0,
    R_MODE_16       = 1,
    R_MODE_24       = 2,
    R_MODE_F        = 3
}R_MODE;



/* Description of constants:
 *
 * Npc - is the number of look-up values available for the lowpass filter
 *    between the beginning of its impulse response and the "cutoff time"
 *    of the filter.  The cutoff time is defined as the reciprocal of the
 *    lowpass-filter cut off frequence in Hz.  For example, if the
 *    lowpass filter were a sinc function, Npc would be the index of the
 *    impulse-response lookup-table corresponding to the first zero-
 *    crossing of the sinc function.  (The inverse first zero-crossing
 *    time of a sinc function equals its nominal cutoff frequency in Hz.)
 *    Npc must be a power of 2 due to the details of the current
 *    implementation. The default value of 512 is sufficiently high that
 *    using linear interpolation to fill in between the table entries
 *    gives approximately 16-bit accuracy in filter coefficients.
 *
 * Nhc - is log base 2 of Npc.
 *
 * Na - is the number of bits devoted to linear interpolation of the
 *    filter coefficients.
 *
 * Np - is Na + Nhc, the number of bits to the right of the binary point
 *    in the integer "time" variable. To the left of the point, it indexes
 *    the input array (X), and to the right, it is interpreted as a number
 *    between 0 and 1 sample of the input X.  Np must be less than 16 in
 *    this implementation.
 *
 * Nh - is the number of bits in the filter coefficients. The sum of Nh and
 *    the number of bits in the input data (typically 16) cannot exceed 32.
 *    Thus Nh should be 16.  The largest filter coefficient should nearly
 *    fill 16 bits (32767).
 *
 * Nb - is the number of bits in the input data. The sum of Nb and Nh cannot
 *    exceed 32.
 *
 * Nhxn - is the number of bits to right shift after multiplying each input
 *    sample times a filter coefficient. It can be as great as Nh and as
 *    small as 0. Nhxn = Nh-2 gives 2 guard bits in the multiply-add
 *    accumulation.  If Nhxn=0, the accumulation will soon overflow 32 bits.
 *
 * Nhg - is the number of guard bits in mpy-add accumulation (equal to Nh-Nhxn)
 *
 * NLpScl - is the number of bits allocated to the unity-gain normalization
 *    factor.  The output of the lowpass filter is multiplied by LpScl and
 *    then right-shifted NLpScl bits. To avoid overflow, we must have
 *    Nb+Nhg+NLpScl < 32.
 */

class ResampleClass: public ResampleInterface{
private:
    R_MODE rMode;

//resamplesub.c
    int _firstChunk;
    int _Xoff;
    int _XoffSizeByte;
    int _offsetSizeX2inBytes;
    int _Time;
    int _writtenSamples;
    int _inputSamples;
    int _highQuality;
    int _nChannel;
    float _resampleFactor;

    int _creepNo;
    int _creepNoBytes;

    short * _X1;
    short * _X2;
    short * _Y1;
    short * _Y2;
    short * _X1_Remain;
    short * _X2_Remain;

/*24*/
    int _firstChunk24;
    int _Xoff24;
    int _XoffSizeByte24;
    int _offsetSizeX2inBytes24;
    int _Time24;
    int _writtenSamples24;
    int _inputSamples24;
    int _highQuality24;
    int _nChannel24;
    float _resampleFactor24;

    int _creepNo24;
    int _creepNoBytes24;

    int* _X1_24;
    int* _X2_24;
    int* _Y1_24;
    int* _Y2_24;
    int* _X1_Remain_24;
    int* _X2_Remain_24;
/*24 end*/

//resamplesub.c
    HWORD WordToHword(WORD v, int scl);
    int SrcUp(HWORD X[], HWORD Y[], double factor, UWORD *Time,
                     UHWORD Nx, UHWORD Nwing, UHWORD LpScl,
                     HWORD Imp[], HWORD ImpD[], BOOL Interp);
    int SrcUp(WORD X[], WORD Y[], double factor, UWORD *Time,
                  UHWORD Nx, UHWORD Nwing, UHWORD LpScl,
                  HWORD Imp[], HWORD ImpD[], BOOL Interp);
    int SrcUD(HWORD X[], HWORD Y[], double factor, UWORD *Time,
                     UHWORD Nx, UHWORD Nwing, UHWORD LpScl,
                     HWORD Imp[], HWORD ImpD[], BOOL Interp);
    int SrcUD(WORD X[], WORD Y[], double factor, UWORD *Time,
                 UHWORD Nx, UHWORD Nwing, UHWORD LpScl,
                 HWORD Imp[], HWORD ImpD[], BOOL Interp);

    int resampleWithFilter_mem(  /* number of output samples returned */
        double factor,              /* factor = outSampleRate/inSampleRate */
        short * inbuffer,                   /* input and output file descriptors */
        short *outputBuffer,
        int inCount,                /* number of input samples to convert */
        int outCount,               /* number of output samples to compute */
        int nChans,                 /* number of sound channels (1 or 2) */
        BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
        HWORD Imp[], HWORD ImpD[],
        UHWORD LpScl, UHWORD Nmult, UHWORD Nwing,BOOL lastChunk);
    int resampleWithFilter_mem2Ch(  /* number of output samples returned */
        double factor,              /* factor = outSampleRate/inSampleRate */
        short * inbuffer,
        short *outputBuffer,
        int inCount,                /* number of input samples to convert */
        int outCount,               /* number of output samples to compute */
        int nChans,                 /* number of sound channels (1 or 2) */
        BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
        HWORD Imp[], HWORD ImpD[],
        UHWORD LpScl, UHWORD Nmult, UHWORD Nwing,BOOL lastChunk);
    int resample_mem(           /* number of output sample returned */
        double factor,      /* factor = Sndout/Sndin */
        short*    inputbuffer,      /* input and output buffer size */
        short*    outputbuffer,
        int inCount,        /* number of input samples to convert */
        int outCount,       /* number of output samples to compute */
        int nChans,         /* number of sound channels (1 or 2) */
        BOOL interpFilt,        /* TRUE means interpolate filter coeffs */
        int fastMode,       /* 0 = highest quality, slowest speed */
        BOOL largeFilter,       /* TRUE means use 65-tap FIR filter */
        char *filterFile,       /* NULL for internal filter, else filename */
        BOOL lastChunk);
//filterkit.c
    WORD FilterUp(HWORD Imp[], HWORD ImpD[],
                 UHWORD Nwing, BOOL Interp,
                 HWORD *Xp, HWORD Ph, HWORD Inc);
    WORD FilterUp(HWORD Imp[], HWORD ImpD[],
             UHWORD Nwing, BOOL Interp,
             WORD *Xp, HWORD Ph, HWORD Inc);
    WORD FilterUD( HWORD Imp[], HWORD ImpD[],
             UHWORD Nwing, BOOL Interp,
             HWORD *Xp, HWORD Ph, HWORD Inc, UHWORD dhb);
    int FilterUD( HWORD Imp[], HWORD ImpD[],
                 UHWORD Nwing, BOOL Interp,
                 WORD *Xp, HWORD Ph, HWORD Inc, UHWORD dhb);
    WORD longlongTo24bit(long long v, int scl);

    int resampleWithFilter_mem(
        double factor,
        int* inbuffer,
        int *outputBuffer,
        int inCount,
        int outCount,
        int nChans,
        BOOL interpFilt,
        HWORD Imp[], HWORD ImpD[],
        UHWORD LpScl, UHWORD Nmult, UHWORD Nwing,BOOL lastChunk);

    int resample_mem(
        double factor,
        int* inputbuffer,
        int* outputbuffer,
        int inCount,
        int outCount,
        int nChans,
        BOOL interpFilt,
        int fastMode,
        BOOL largeFilter,
        char *filterFile,
        BOOL lastChunk);

    int resampleWithFilter_mem2Ch(
        double factor,
        int * inbuffer,
        int *outputBuffer,
        int inCount,
        int outCount,
        int nChans,
        BOOL interpFilt,
        HWORD Imp[], HWORD ImpD[],
        UHWORD LpScl, UHWORD Nmult, UHWORD Nwing,BOOL lastChunk);


public:

   ResampleClass();
    virtual ~ResampleClass();
    virtual int resample_m(short* inputbuffer,short* outputbuffer,int inCount,BOOL lastChunk);
    virtual int initResample(double factor, int nChans, int qualityHigh);
    virtual int deInitResample();
    virtual int getMinResampleInputSampleNo();
    virtual int getMaxResampleInputBufferSize();
    virtual int getResampleXoffVal();
    virtual int initResample24(double factor, int nChans, int qualityHigh);
    virtual int resample_m(int* inputbuffer,int* outputbuffer,int inCount,BOOL lastChunk);
    virtual int initResample(double factor, int nChans, int qualityHigh, int bitsPerSample);
};

extern "C" ResampleInterface * createResampler();
extern "C" void  destroyResampler(ResampleInterface * rsmpl);

