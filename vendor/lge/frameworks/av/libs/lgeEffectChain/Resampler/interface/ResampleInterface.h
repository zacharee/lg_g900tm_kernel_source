#pragma once

#include <stdio.h>
#include <stdlib.h>

typedef char BOOL;

class ResampleInterface{

public:

    ResampleInterface(){}
    virtual ~ResampleInterface(){}
    /*resample_m : use this function.  inCount : number of input samples to convert */
    virtual int resample_m(short* inputbuffer,short* outputbuffer,int inCount,BOOL lastChunk) = 0;
    virtual int initResample(double factor, int nChans, int qualityHigh)= 0;
    virtual int initResample(double factor, int nChans, int qualityHigh, int bitsPerSample)= 0; //8.24 input
    virtual int deInitResample()= 0;
    virtual int getMinResampleInputSampleNo()= 0;
    virtual int getMaxResampleInputBufferSize()= 0;
    virtual int getResampleXoffVal()= 0;
    //interface for 24bits
    virtual int initResample24(double factor, int nChans, int qualityHigh) = 0;
    virtual int resample_m(int* inputbuffer,int* outputbuffer,int inCount,BOOL lastChunk) = 0;
};

