#include "ResampleClass.h"

ResampleClass:: ResampleClass()
{
    rMode = R_MODE_NONE;

    //init members from resamplesub.c
    _firstChunk = 0;
    _Xoff = 0;
    _XoffSizeByte = 0;
    _offsetSizeX2inBytes = 0;
    _Time = 0;
    _writtenSamples = 0;
    _inputSamples = 0;
    _highQuality = 0;
    _nChannel = 0;
    _resampleFactor = 0.0;

    _creepNo = 0;
    _creepNoBytes = 0;

    _X1 = NULL;
    _X2 = NULL;
    _Y1 = NULL;
    _Y2 = NULL;
    _X1_Remain = NULL;
    _X2_Remain = NULL;

/*24*/
    _firstChunk24 = 0;
    _Xoff24 = 0;
    _XoffSizeByte24 = 0;
    _offsetSizeX2inBytes24 = 0;
    _Time24 = 0;
    _writtenSamples24 = 0;
    _inputSamples24 = 0;
    _highQuality24 = 0;
    _nChannel24 = 0;
    _resampleFactor24 = 0;

    _creepNo24 = 0;
    _creepNoBytes24 = 0;

    _X1_24 = NULL;
    _X2_24 = NULL;
    _Y1_24 = NULL;
    _Y2_24 = NULL;
    _X1_Remain_24 = NULL;
    _X2_Remain_24 = NULL;
/*24 end*/

}

ResampleClass:: ~ResampleClass()
{
    rMode = R_MODE_NONE;
}

 __inline HWORD ResampleClass:: WordToHword(WORD v, int scl)
{
    HWORD out;
    WORD llsb = (1<<(scl-1));
    v += llsb;
    v >>= scl;
    if (v>MAX_HWORD) {
        v = MAX_HWORD;
    } else if (v < MIN_HWORD) {
        v = MIN_HWORD;
    }
    out = (HWORD) v;
    return out;
}


  __inline WORD ResampleClass:: longlongTo24bit(long long v, int scl)
 {
     WORD llsb = (1<<(scl-1));
     v += llsb;
     v >>= scl;
     if (v>MAX_24WORD) {
         v = MAX_24WORD;
     } else if (v < MIN_24WORD) {
         v = MIN_24WORD;
     }
     return v;
 }

/* Sampling rate up-conversion only subroutine;
 * Slightly faster than down-conversion; */

 //16bit IN 16bit OUT
int ResampleClass:: SrcUp(HWORD X[], HWORD Y[], double factor, UWORD *Time,
                 UHWORD Nx, UHWORD Nwing, UHWORD LpScl,
                 HWORD Imp[], HWORD ImpD[], BOOL Interp)
{
    HWORD *Xp, *Ystart;
    WORD v;

    double dt;                  /* Step through input signal */
    UWORD dtb;                  /* Fixed-point version of Dt */
    UWORD endTime;              /* When Time reaches EndTime, return to user */

    dt = 1.0/factor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */

    Ystart = Y;
    endTime = *Time + (1<<Np)*(WORD)Nx;
    while (*Time < endTime)
    {
        Xp = &X[*Time>>Np];      /* Ptr to current input sample */
        /* Perform left-wing inner product */
        v = FilterUp(Imp, ImpD, Nwing, Interp, Xp, (HWORD)(*Time&Pmask),-1);
        /* Perform right-wing inner product */
        v += FilterUp(Imp, ImpD, Nwing, Interp, Xp+1,
              /* previous (triggers warning): (HWORD)((-*Time)&Pmask),1); */
                      (HWORD)((((*Time)^Pmask)+1)&Pmask),1);

        v >>= Nhg;              /* Make guard bits */
        v *= LpScl;             /* Normalize for unity filter gain */
        *Y++ = WordToHword(v,NLpScl);   /* strip guard bits, deposit output */
        *Time += dtb;           /* Move to next sample by time increment */
    }
    return (Y - Ystart);        /* Return the number of output samples */
}

 // WORD input, fixed point
 //24bit IN 24bit OUT
 int ResampleClass:: SrcUp(WORD X[], WORD Y[], double factor, UWORD *Time,
                  UHWORD Nx, UHWORD Nwing, UHWORD LpScl,
                  HWORD Imp[], HWORD ImpD[], BOOL Interp)
 {
     WORD *Ystart;
     long long v = 0;
     WORD *Xp = 0;

     double dt;
     UWORD dtb;
     UWORD endTime;

     dt = 1.0/factor;
     dtb = dt*(1<<Np) + 0.5;

     Ystart = Y;
     endTime = *Time + (1<<Np)*(WORD)Nx;
     while (*Time < endTime)
     {
         Xp = &X[*Time>>Np];

        /*24bit out. v+v gain is bigger than 1*/
        v = FilterUp(Imp, ImpD, Nwing, Interp, Xp, (HWORD)(*Time&Pmask),-1);
        v += FilterUp(Imp, ImpD, Nwing, Interp, Xp+1,
                      (HWORD)((((*Time)^Pmask)+1)&Pmask),1);

        v = v * 99773;
        *Y = longlongTo24bit(v,17);
        Y++;
        *Time += dtb;
     }
     return (Y - Ystart);
 }


/* Sampling rate conversion subroutine */
int ResampleClass:: SrcUD(HWORD X[], HWORD Y[], double factor, UWORD *Time,
                 UHWORD Nx, UHWORD Nwing, UHWORD LpScl,
                 HWORD Imp[], HWORD ImpD[], BOOL Interp)
{
    HWORD *Xp, *Ystart;
    WORD v;

    double dh;                  /* Step through filter impulse response */
    double dt;                  /* Step through input signal */
    UWORD endTime;              /* When Time reaches EndTime, return to user */
    UWORD dhb, dtb;             /* Fixed-point versions of Dh,Dt */

    dt = 1.0/factor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */

    dh = MIN(Npc, factor*Npc);  /* Filter sampling period */
    dhb = dh*(1<<Na) + 0.5;     /* Fixed-point representation */

    Ystart = Y;
    endTime = *Time + (1<<Np)*(WORD)Nx;
    while (*Time < endTime)
    {
        Xp = &X[*Time>>Np];     /* Ptr to current input sample */
        v = FilterUD(Imp, ImpD, Nwing, Interp, Xp, (HWORD)(*Time&Pmask),
                     -1, dhb);  /* Perform left-wing inner product */
        v += FilterUD(Imp, ImpD, Nwing, Interp, Xp+1,
              /* previous (triggers warning): (HWORD)((-*Time)&Pmask), */
                      (HWORD)((((*Time)^Pmask)+1)&Pmask),
                      1, dhb);  /* Perform right-wing inner product */
        v >>= Nhg;              /* Make guard bits */
        v *= LpScl;             /* Normalize for unity filter gain */
        *Y++ = WordToHword(v,NLpScl);   /* strip guard bits, deposit output */
        *Time += dtb;           /* Move to next sample by time increment */
    }

    return (Y - Ystart);        /* Return the number of output samples */
}


// WORD Input, fixed point
//24bit input, fixed point, 24bit output
int ResampleClass:: SrcUD(WORD X[], WORD Y[], double factor, UWORD *Time,
                 UHWORD Nx, UHWORD Nwing, UHWORD LpScl,
                 HWORD Imp[], HWORD ImpD[], BOOL Interp)
{
    WORD *Xp, *Ystart;

    long long v = 0;
    double LpSclDouble = ((double)0.80126953125f*0.95*factor + (double)0.000030517578125);
    int LpSclInt = (int)(LpSclDouble * (double)262144 + 0.5);

    double dh;                  /* Step through filter impulse response */
    double dt;                  /* Step through input signal */
    UWORD endTime;              /* When Time reaches EndTime, return to user */
    UWORD dhb, dtb;             /* Fixed-point versions of Dh,Dt */

    dt = 1.0/factor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */

    dh = MIN(Npc, factor*Npc);  /* Filter sampling period */
    dhb = dh*(1<<Na) + 0.5;     /* Fixed-point representation */

    Ystart = Y;
    endTime = *Time + (1<<Np)*(WORD)Nx;
    while (*Time < endTime)
    {
        Xp = &X[*Time>>Np];     /* Ptr to current input sample */
        v = FilterUD(Imp, ImpD, Nwing, Interp, Xp, (HWORD)(*Time&Pmask),-1, dhb);
        v += FilterUD(Imp, ImpD, Nwing, Interp, Xp+1,(HWORD)((((*Time)^Pmask)+1)&Pmask),1, dhb);

        v *= LpSclInt;
        *Y = longlongTo24bit(v, 18);
        Y++;
        *Time += dtb;           /* Move to next sample by time increment */
    }

    return (Y - Ystart);        /* Return the number of output samples */
}


/*
_Xoff = offset sample number
_Time = (Xoff<<Np)   //DATA start point (after offset1).
_XoffSizeByte = Xoff *2;
_offsetSizeX2inBytes = Xoff *4


//FIRST
|Offset1| DATA  |Offset2|Offset3|
Input Data = Data + Offset2 + Offset3
NX = DATA + Offset 2 = processing data, Offset1 is zero padded.
RemainBuffer Offset2, Offset3

//Process1
|Offset2|Offset3| DATA  |Offset4|Offset5|
Input Data = Data + Offset4 + Offset5
NX = Offset3+DATA+Offset4
RemainBuffer Offset4, Offset5

//Process2
|Offset4|Offset5| DATA  |Offset6|Offset7|
Input Data = Data + Offset6 + Offset7
NX = Offset5+DATA+Offset6
RemainBuffer Offset6, Offset7

//LAST
|Offset6|Offset7| DATA  |Offset6|
Input Data = Data
NX = Offset7 + DATA. Offset 6 is zero padded
*/
int ResampleClass:: resampleWithFilter_mem(  /* number of output samples returned */
    double factor,              /* factor = outSampleRate/inSampleRate */
    short * inbuffer,                   /* input and output file descriptors */
    short *outputBuffer,
    int inCount,                /* number of input samples to convert */
    int outCount,               /* number of output samples to compute */
    int nChans,                 /* number of sound channels (1 or 2) */
    BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
    HWORD Imp[], HWORD ImpD[],
    UHWORD LpScl, UHWORD Nmult, UHWORD Nwing,BOOL lastChunk)
{
    int Nout;

    short* X1= _X1;
    short* Y1= _Y1;                 /* I/O buffers */
    UWORD Time;                     //= (24<<Np);       //start time. offset
    UHWORD Nx;
    int Ncreep = 0;

    Time = _Time;
    Nx = inCount;                   //NX means resample sample Numbers

    if (factor < 1)
      LpScl = LpScl*factor + 0.5;   //need only one time

    if(_firstChunk)
        {
            memcpy((char *)X1+_XoffSizeByte,inbuffer,inCount*2);                                //MAKE FIRST CHUNK
            Nx = inCount - _Xoff;
            _firstChunk = FALSE;
        }else{
            if(lastChunk)
            {
                Nx = inCount;
                Nx = Nx + _Xoff;
                memset(X1,0x00,sizeof(short)*IBUFFSIZE +_offsetSizeX2inBytes);              //zero padd Offset6
            }else{
                Nx = inCount -_creepNo;
            }
                memcpy ((char *)X1,_X1_Remain,_offsetSizeX2inBytes-_creepNoBytes);          //copy offset2, offset3
                memcpy((char *)X1+_offsetSizeX2inBytes-_creepNoBytes,inbuffer,inCount*2);   //copy data + offset4 + offset5
        }

    if(factor>=1.0)
        Nout=SrcUp(X1,Y1,factor,&Time,Nx,Nwing,LpScl,Imp,ImpD,interpFilt);
    else
        Nout=SrcUD(X1,Y1,factor,&Time,Nx,Nwing,LpScl,Imp,ImpD,interpFilt);

    Time -= (Nx<<Np);       /* Move converter Nx samples back in time */
    _Time = Time ;
    Ncreep = (Time>>Np) - _Xoff; /* Calc time accumulation in Time */
        if (Ncreep) {
            _Time -= (Ncreep<<Np);    /* Remove time accumulation */
            _creepNo= Ncreep;
            _creepNoBytes = _creepNo <<1;
        }else{
            _creepNo = 0;
            _creepNoBytes = 0;
        }

    _writtenSamples = _writtenSamples + Nout;
    _inputSamples = _inputSamples + inCount ;

    if(lastChunk)
    {
        int expectedSamples = _inputSamples * (float)factor;
        if(expectedSamples < _writtenSamples)
        {
            Nout = Nout - (_writtenSamples - expectedSamples);      //detach extra samples.
        }
    }

    memcpy ((char *)_X1_Remain,(char *)inbuffer+inCount*2 -_offsetSizeX2inBytes + _creepNoBytes,_offsetSizeX2inBytes-_creepNoBytes); //make Remain Buffer offset 2,3   4,5   6,7///
    memcpy(outputBuffer,Y1,sizeof(short)*Nout);

    return Nout;
}


//24 int input int_1ch
int ResampleClass:: resampleWithFilter_mem(  /* number of output samples returned */
    double factor,              /* factor = outSampleRate/inSampleRate */
    int* inbuffer,                   /* input and output file descriptors */
    int *outputBuffer,
    int inCount,                /* number of input samples to convert */
    int outCount,               /* number of output samples to compute */
    int nChans,                 /* number of sound channels (1 or 2) */
    BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
    HWORD Imp[], HWORD ImpD[],
    UHWORD LpScl, UHWORD Nmult, UHWORD Nwing,BOOL lastChunk)
{
    int Nout;

    int* X1_24= _X1_24;
    int* Y1_24= _Y1_24;                 /* I/O buffers */
    UWORD Time_24;                     //= (24<<Np);       //start time. offset
    UHWORD Nx_24;
    int Ncreep_24 = 0;

    Time_24 = _Time24;
    Nx_24 = inCount;                   //NX means resample sample Numbers

    if (factor < 1)
      LpScl = LpScl*factor + 0.5;   //need only one time

    if(_firstChunk24)
        {
            memcpy((char *)X1_24+_XoffSizeByte24,inbuffer,inCount*4);                                //MAKE FIRST CHUNK
            Nx_24 = inCount - _Xoff24;
            _firstChunk24 = FALSE;

        }else{
            if(lastChunk)
            {
                Nx_24 = inCount;
                Nx_24 = Nx_24 + _Xoff24;
                memset(X1_24,0x00,sizeof(int)*IBUFFSIZE +_offsetSizeX2inBytes24);              //zero padd Offset6
            }else{
                Nx_24 = inCount -_creepNo24;
            }
                memcpy ((char *)X1_24,_X1_Remain_24,_offsetSizeX2inBytes24-_creepNoBytes24);          //copy offset2, offset3
                memcpy((char *)X1_24+_offsetSizeX2inBytes24 -_creepNoBytes24,inbuffer,inCount*4);   //copy data + offset4 + offset5
        }

    if(factor>=1.0)
        Nout=SrcUp(X1_24,Y1_24,factor,&Time_24,Nx_24,Nwing,LpScl,Imp,ImpD,interpFilt);  //fixed point
    else
        Nout=SrcUD(X1_24,Y1_24,factor,&Time_24,Nx_24,Nwing,LpScl,Imp,ImpD,interpFilt);

    Time_24 -= (Nx_24<<Np);       /* Move converter Nx samples back in time */
    _Time24 = Time_24 ;
    Ncreep_24 = (Time_24>>Np) - _Xoff24; /* Calc time accumulation in Time */
        if (Ncreep_24) {
            _Time24 -= (Ncreep_24<<Np);    /* Remove time accumulation */
            _creepNo24= Ncreep_24;
            _creepNoBytes24 = _creepNo24 <<2;
        }else{
            _creepNo24 = 0;
            _creepNoBytes24 = 0;
        }

    _writtenSamples24 = _writtenSamples24 + Nout;
    _inputSamples24 = _inputSamples24 + inCount ;

    if(lastChunk)
    {
        int expectedSamples = _inputSamples24 * (float)factor;
        if(expectedSamples < _writtenSamples24)
        {
            Nout = Nout - (_writtenSamples24 - expectedSamples);      //detach extra samples.
        }
    }

    memcpy ((char *)_X1_Remain_24, (char *)inbuffer+inCount*4 -_offsetSizeX2inBytes24 + _creepNoBytes24, _offsetSizeX2inBytes24  - _creepNoBytes24); //make Remain Buffer offset 2,3   4,5   6,7///
    memcpy(outputBuffer, Y1_24, sizeof(int) * Nout);

    return Nout;
}


int ResampleClass:: resampleWithFilter_mem2Ch(  /* number of output samples returned */
    double factor,              /* factor = outSampleRate/inSampleRate */
    short * inbuffer,
    short *outputBuffer,
    int inCount,                /* number of input samples to convert */
    int outCount,               /* number of output samples to compute */
    int nChans,                 /* number of sound channels (1 or 2) */
    BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
    HWORD Imp[], HWORD ImpD[],
    UHWORD LpScl, UHWORD Nmult, UHWORD Nwing,BOOL lastChunk)
{

    int Nout;

    short* X1= _X1;
    short* Y1= _Y1;
    short* X2 = _X2;
    short* Y2 = _Y2;
    UWORD Time, Time2;
    UHWORD Nx;
    int Ncreep = 0;
    short* inbufL;
    short* inbufR ;
    int itr = 0;

    inbufL = (short*)malloc(inCount*sizeof(short));
    if(inbufL == NULL){return -90;}
    inbufR = (short*)malloc(inCount*sizeof(short));
    if(inbufR == NULL){free(inbufL);return -91;}

    for(itr = 0;itr < inCount;itr++)
    {
        inbufL[itr] = inbuffer[itr *2] ;
        inbufR[itr] = inbuffer[itr *2+1] ;
    }

    Time = _Time;
    Time2 = _Time;
    Nx = inCount;               //NX means resample sample Numbers

    if (factor < 1)
      LpScl = LpScl*factor + 0.5;           //need only one time

    if(_firstChunk)
        {
            memcpy((char *)X1+_XoffSizeByte,inbufL,inCount*2);                              //MAKE FIRST CHUNK
            memcpy((char *)X2+_XoffSizeByte,inbufR,inCount*2);                              //MAKE FIRST CHUNK
            Nx = inCount - _Xoff;
            _firstChunk = FALSE;
        }else{
            if(lastChunk)
            {
                Nx = inCount;
                Nx = Nx + _Xoff;
                memset(X1,0x00,sizeof(short)*IBUFFSIZE +_offsetSizeX2inBytes);      //zero padd Offset6
                memset(X2,0x00,sizeof(short)*IBUFFSIZE +_offsetSizeX2inBytes);      //zero padd Offset6
            }else{
                Nx = inCount -_creepNo;
            }
                memcpy ((char *)X1,_X1_Remain,_offsetSizeX2inBytes-_creepNoBytes); //copy offset2, offset3
                memcpy ((char *)X2,_X2_Remain,_offsetSizeX2inBytes-_creepNoBytes); //copy offset2, offset3
                memcpy((char *)X1+_offsetSizeX2inBytes-_creepNoBytes,inbufL,inCount*2);  //copy data + offset4 + offset5
                memcpy((char *)X2+_offsetSizeX2inBytes-_creepNoBytes,inbufR,inCount*2);  //copy data + offset4 + offset5
        }

    if(factor>=1.0){
        Nout=SrcUp(X1,Y1,factor,&Time,Nx,Nwing,LpScl,Imp,ImpD,interpFilt);
        Nout=SrcUp(X2,Y2,factor,&Time2,Nx,Nwing,LpScl,Imp,ImpD,interpFilt);
        }
    else{
        Nout=SrcUD(X1,Y1,factor,&Time,Nx,Nwing,LpScl,Imp,ImpD,interpFilt);
        Nout=SrcUD(X2,Y2,factor,&Time2,Nx,Nwing,LpScl,Imp,ImpD,interpFilt);
        }

    Time -= (Nx<<Np);       /* Move converter Nx samples back in time */
    _Time = Time ;
    Ncreep = (Time>>Np) - _Xoff; /* Calc time accumulation in Time */
        if (Ncreep) {
            _Time -= (Ncreep<<Np);    /* Remove time accumulation */
            _creepNo= Ncreep;
            _creepNoBytes = _creepNo <<1;
        }else{
            _creepNo = 0;
            _creepNoBytes = 0;
        }

    _writtenSamples = _writtenSamples + Nout;
    _inputSamples = _inputSamples + inCount ;

    if(lastChunk)
    {
        int expectedSamples = _inputSamples * (float)factor;
        if(expectedSamples < _writtenSamples)
        {
            Nout = Nout - (_writtenSamples - expectedSamples);      //detach extra samples.
        }
    }

    memcpy ((char *)_X1_Remain,(char *)inbufL+inCount*2 -_offsetSizeX2inBytes + _creepNoBytes,_offsetSizeX2inBytes-_creepNoBytes); //make Remain Buffer offset 2,3   4,5   6,7///
    memcpy ((char *)_X2_Remain,(char *)inbufR+inCount*2 -_offsetSizeX2inBytes + _creepNoBytes,_offsetSizeX2inBytes-_creepNoBytes); //make Remain Buffer offset 2,3   4,5   6,7///

    for(itr = 0;itr < Nout;itr++)
    {
        outputBuffer[2*itr] = Y1[itr] ;
        outputBuffer[2*itr+1] = Y2[itr];
    }
    free(inbufL);
    free(inbufR);

    return Nout;

}


//int_2ch
int ResampleClass:: resampleWithFilter_mem2Ch(  /* number of output samples returned */
    double factor,              /* factor = outSampleRate/inSampleRate */
    int * inbuffer,
    int *outputBuffer,
    int inCount,                /* number of input samples to convert */
    int outCount,               /* number of output samples to compute */
    int nChans,                 /* number of sound channels (1 or 2) */
    BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
    HWORD Imp[], HWORD ImpD[],
    UHWORD LpScl, UHWORD Nmult, UHWORD Nwing,BOOL lastChunk)
{
    int Nout24;

    int* X1_24= _X1_24;
    int* Y1_24= _Y1_24;
    int* X2_24 = _X2_24;
    int* Y2_24 = _Y2_24;
    UWORD Time_24, Time2_24;
    UHWORD Nx_24;
    int Ncreep_24 = 0;
    int* inbufL_24;
    int* inbufR_24 ;
    int itr_24 = 0;

    inbufL_24 = (int*)malloc(inCount*sizeof(int));
    if(inbufL_24 == NULL){return -90;}
    inbufR_24 = (int*)malloc(inCount*sizeof(int));
    if(inbufR_24 == NULL){free(inbufL_24);return -91;}

    for(itr_24 = 0;itr_24 < inCount;itr_24++)
    {
        inbufL_24[itr_24] = inbuffer[itr_24 * 2] ;
        inbufR_24[itr_24] = inbuffer[itr_24 * 2+1] ;
    }

    Time_24 = _Time24;
    Time2_24 = _Time24;
    Nx_24 = inCount;               //NX means resample sample Numbers

    if (factor < 1)
      LpScl = LpScl*factor + 0.5;           //need only one time

    if(_firstChunk24)
        {
            memcpy((char *)X1_24+_XoffSizeByte24,inbufL_24,inCount*4);                              //MAKE FIRST CHUNK
            memcpy((char *)X2_24+_XoffSizeByte24,inbufR_24,inCount*4);                              //MAKE FIRST CHUNK
            Nx_24 = inCount - _Xoff24;
            _firstChunk24 = FALSE;
        }else{
            if(lastChunk)
            {
                Nx_24 = inCount;
                Nx_24 = Nx_24 + _Xoff24;
                memset(X1_24,0x00,sizeof(int)*IBUFFSIZE +_offsetSizeX2inBytes24);      //zero padd Offset6
                memset(X2_24,0x00,sizeof(int)*IBUFFSIZE +_offsetSizeX2inBytes24);      //zero padd Offset6
            }else{
                Nx_24 = inCount -_creepNo24;
            }
                memcpy ((char *)X1_24,_X1_Remain_24,_offsetSizeX2inBytes24-_creepNoBytes24); //copy offset2, offset3
                memcpy ((char *)X2_24,_X2_Remain_24,_offsetSizeX2inBytes24-_creepNoBytes24); //copy offset2, offset3
                memcpy((char *)X1_24+_offsetSizeX2inBytes24-_creepNoBytes24,inbufL_24,inCount*4);  //copy data + offset4 + offset5
                memcpy((char *)X2_24+_offsetSizeX2inBytes24-_creepNoBytes24,inbufR_24,inCount*4);  //copy data + offset4 + offset5
        }

    if(factor>=1.0){
            Nout24 = SrcUp(X1_24,Y1_24,factor,&Time_24,Nx_24,Nwing,LpScl,Imp,ImpD,interpFilt);
            Nout24 = SrcUp(X2_24,Y2_24,factor,&Time2_24,Nx_24,Nwing,LpScl,Imp,ImpD,interpFilt);
        }
    else{
            Nout24=SrcUD(X1_24,Y1_24,factor,&Time_24,Nx_24,Nwing,LpScl,Imp,ImpD,interpFilt);
            Nout24=SrcUD(X2_24,Y2_24,factor,&Time2_24,Nx_24,Nwing,LpScl,Imp,ImpD,interpFilt);
        }

    Time_24 -= (Nx_24<<Np);       /* Move converter Nx samples back in time */
    _Time24 = Time_24 ;
    Ncreep_24 = (Time_24>>Np) - _Xoff24; /* Calc time accumulation in Time */
        if (Ncreep_24) {
            _Time24 -= (Ncreep_24<<Np);    /* Remove time accumulation */
            _creepNo24= Ncreep_24;
            _creepNoBytes24 = _creepNo24 <<2;
        }else{
            _creepNo24 = 0;
            _creepNoBytes24 = 0;
        }

    _writtenSamples24 = _writtenSamples24 + Nout24;
    _inputSamples24 = _inputSamples24 + inCount ;

    if(lastChunk)
    {
        int expectedSamples = _inputSamples24 * (float)factor;
        if(expectedSamples < _writtenSamples24)
        {
            Nout24 = Nout24 - (_writtenSamples24 - expectedSamples);      //detach extra samples.
        }
    }

    memcpy ((char *)_X1_Remain_24,(char *)inbufL_24+inCount*4 -_offsetSizeX2inBytes24 + _creepNoBytes24,_offsetSizeX2inBytes24-_creepNoBytes24); //make Remain Buffer offset 2,3   4,5   6,7///
    memcpy ((char *)_X2_Remain_24,(char *)inbufR_24+inCount*4 -_offsetSizeX2inBytes24 + _creepNoBytes24,_offsetSizeX2inBytes24-_creepNoBytes24); //make Remain Buffer offset 2,3   4,5   6,7///

    for(itr_24 = 0;itr_24 < Nout24;itr_24++)
    {
        outputBuffer[2*itr_24] = Y1_24[itr_24] ;
        outputBuffer[2*itr_24+1] = Y2_24[itr_24];
    }
    free(inbufL_24);
    free(inbufR_24);

    return Nout24;
}


int ResampleClass:: resample_m(
                                    short* inputbuffer,
                                    short* outputbuffer,
                                    int inCount,                /* number of input samples to convert */
                                    BOOL lastChunk)
{
    if(rMode != R_MODE_16)
        return -1;

    return resample_mem(_resampleFactor, inputbuffer, outputbuffer, inCount,
                      inCount*_resampleFactor , _nChannel ,FALSE, 0, _highQuality, NULL,lastChunk);
}

int ResampleClass:: resample_m(
                                    int* inputbuffer,
                                    int* outputbuffer,
                                    int inCount,                /* number of input samples to convert */
                                    BOOL lastChunk)
{
    if(rMode != R_MODE_24)
        return -1;

    return resample_mem(_resampleFactor24, inputbuffer, outputbuffer, inCount,
                      inCount*_resampleFactor24 , _nChannel24 ,FALSE, 0, _highQuality24, NULL,lastChunk);
}


int ResampleClass:: resample_mem(                   /* number of output samples returned */
    double factor,              /* factor = Sndout/Sndin */
    short*    inputbuffer,
    short* outputbuffer,
    int inCount,                /* number of input samples to convert */
    int outCount,               /* number of output samples to compute */
    int nChans,                 /* number of sound channels (1 or 2) */
    BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
    int fastMode,               /* 0 = highest quality, slowest speed */
    BOOL largeFilter,           /* TRUE means use 65-tap FIR filter */
    char *filterFile,
    BOOL lastChunk)           /* NULL for internal filter, else filename */
{
    UHWORD LpScl;               /* Unity-gain scale factor */
    UHWORD Nwing;               /* Filter table size */
    UHWORD Nmult;               /* Filter length for up-conversions */
    HWORD *Imp=0;               /* Filter coefficients */
    HWORD *ImpD=0;              /* ImpD[n] = Imp[n+1]-Imp[n] */

    if (largeFilter) {
            Nmult = LARGE_FILTER_NMULT;
            Imp = LARGE_FILTER_IMP;         /* Impulse response */
            ImpD = LARGE_FILTER_IMPD;       /* Impulse response deltas */
            LpScl = LARGE_FILTER_SCALE;     /* Unity-gain scale factor */
            Nwing = LARGE_FILTER_NWING;     /* Filter table length */
        } else {
            Nmult = SMALL_FILTER_NMULT;
            Imp = SMALL_FILTER_IMP;         /* Impulse response */
            ImpD = SMALL_FILTER_IMPD;       /* Impulse response deltas */
            LpScl = SMALL_FILTER_SCALE;     /* Unity-gain scale factor */
            Nwing = SMALL_FILTER_NWING;     /* Filter table length */
        }

        LpScl *= 0.95;
        if(nChans==2)return resampleWithFilter_mem2Ch(factor,inputbuffer,outputbuffer,inCount,outCount,nChans,
                                       interpFilt, Imp, ImpD, LpScl, Nmult, Nwing,lastChunk);
        else return resampleWithFilter_mem(factor,inputbuffer,outputbuffer,inCount,outCount,nChans,
                                       interpFilt, Imp, ImpD, LpScl, Nmult, Nwing,lastChunk);

}

//INT
int ResampleClass:: resample_mem(                   /* number of output samples returned */
    double factor,              /* factor = Sndout/Sndin */
    int* inputbuffer,
    int* outputbuffer,
    int inCount,                /* number of input samples to convert */
    int outCount,               /* number of output samples to compute */
    int nChans,                 /* number of sound channels (1 or 2) */
    BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
    int fastMode,               /* 0 = highest quality, slowest speed */
    BOOL largeFilter,           /* TRUE means use 65-tap FIR filter */
    char *filterFile,
    BOOL lastChunk)           /* NULL for internal filter, else filename */
{
    UHWORD LpScl;               /* Unity-gain scale factor */
    UHWORD Nwing;               /* Filter table size */
    UHWORD Nmult;               /* Filter length for up-conversions */
    HWORD *Imp=0;               /* Filter coefficients */
    HWORD *ImpD=0;              /* ImpD[n] = Imp[n+1]-Imp[n] */

    Nmult = SMALL_FILTER_NMULT;
    Imp = SMALL_FILTER_IMP;         /* Impulse response */
    ImpD = SMALL_FILTER_IMPD;       /* Impulse response deltas */
    LpScl = SMALL_FILTER_SCALE;     /* Unity-gain scale factor */
    Nwing = SMALL_FILTER_NWING;     /* Filter table length */
    LpScl *= 0.95;
    if(nChans==2)return resampleWithFilter_mem2Ch(factor,inputbuffer,outputbuffer,inCount,outCount,nChans,
                                   interpFilt, Imp, ImpD, LpScl, Nmult, Nwing,lastChunk);
    else return resampleWithFilter_mem(factor,inputbuffer,outputbuffer,inCount,outCount,nChans,
                                   interpFilt, Imp, ImpD, LpScl, Nmult, Nwing,lastChunk);

}


int ResampleClass:: getResampleXoffVal()
{
    if(rMode == R_MODE_NONE)
        return 0;
    if(rMode == R_MODE_16)
        return _Xoff ;
    if(rMode == R_MODE_24)
        return _Xoff24;

    return 0;
}

int ResampleClass:: getMinResampleInputSampleNo()
{
    if(rMode == R_MODE_NONE)
        return 0;
    if(rMode == R_MODE_16)
        return _Xoff * 2 ;
    if(rMode == R_MODE_24)
        return _Xoff24 * 2;

    return 0;
}

int ResampleClass:: getMaxResampleInputBufferSize()
{
    if(rMode == R_MODE_NONE)
        return 0;
    if(rMode == R_MODE_16)
        return sizeof(short)*IBUFFSIZE ;
    if(rMode == R_MODE_24)
        return sizeof(int)*IBUFFSIZE ;

    return 0;
}

/*
return val >= 0  Success
return val < 0  Fail
*/
int ResampleClass:: initResample(double factor, int nChans, int qualityHigh)
{
    int obuffSize = (int)(((double)IBUFFSIZE)*factor+2.0 ); //ORG code has 2bytes margine.

    if(rMode != R_MODE_NONE) return -1;
    else rMode = R_MODE_16;

    _highQuality = qualityHigh;
    _resampleFactor = factor;
    _nChannel = nChans;

    if(qualityHigh)
        _Xoff = ((LARGE_FILTER_NMULT+1)/2.0) * MAX(1.0,1.0/factor) + 10;
    else _Xoff = ((SMALL_FILTER_NMULT+1)/2.0) * MAX(1.0,1.0/factor) + 10;

    /* Calc reach of LP filter wing & give some creeping room */
    _Time = (_Xoff<<Np);
    _XoffSizeByte = _Xoff *2;
    _offsetSizeX2inBytes = _Xoff *4;

    _X1 = (short*)malloc(sizeof(short)*IBUFFSIZE +_offsetSizeX2inBytes);     //MAX InputData Size = sizeof(short)*IBUFFSIZE
    if(_X1 == NULL)return -1;
    memset(_X1,0x00,sizeof(short)*IBUFFSIZE+_offsetSizeX2inBytes);

    _X1_Remain = (short*)malloc(_offsetSizeX2inBytes);
    if(_X1_Remain == NULL){free(_X1);return -5;}
    memset(_X1_Remain,0x00,_offsetSizeX2inBytes);

    _Y1 = (short*)malloc(sizeof(short)*obuffSize);
    if(_Y1 == NULL){free(_X1);free(_X1_Remain);return -2;}
    memset(_Y1,0x00,sizeof(short)*obuffSize);

    if(nChans == 2)
    {
        _X2 = (short*)malloc(sizeof(short)*IBUFFSIZE+_offsetSizeX2inBytes);
        if(_X2 == NULL){free(_X1);free(_X1_Remain);free(_Y1);return -3;}
        memset(_X2,0x00,sizeof(short)*IBUFFSIZE+_offsetSizeX2inBytes);

        _X2_Remain = (short*)malloc(_offsetSizeX2inBytes);
        if(_X2_Remain == NULL){free(_X1);free(_X1_Remain);free(_X2);free(_Y1);return -6;}
        memset(_X2_Remain,0x00,_offsetSizeX2inBytes);

        _Y2 = (short*)malloc(sizeof(short)*obuffSize);
        if(_Y2 == NULL){free(_X1);free(_X1_Remain);free(_Y1);free(_X2);free(_X2_Remain);return -4;}
        memset(_Y2,0x00,sizeof(short)*obuffSize);
    }else
    {
        _X2 = NULL;
        _X2_Remain = NULL;
        _Y2 = NULL;
    }

    _creepNo = 0;
    _creepNoBytes = 0;
    _firstChunk = TRUE;
    _writtenSamples = 0;
    _inputSamples = 0;

    return 1;
}


int ResampleClass:: deInitResample()
{
    if(_X1 != NULL)free(_X1);
    if(_X1_Remain != NULL)free(_X1_Remain);
    if(_Y1 != NULL)free(_Y1);
    if(_X2 != NULL)free(_X2);
    if(_X2_Remain != NULL)free(_X2_Remain);
    if(_Y2 != NULL)free(_Y2);

    _creepNo = 0;
    _creepNoBytes = 0;
    _firstChunk = TRUE;
    _writtenSamples = 0;
    _inputSamples = 0;
    _highQuality = 0;
    _resampleFactor = 0.0;

    //need to separate?
    if(_X1_24 != NULL)free(_X1_24);
    if(_X1_Remain_24 != NULL)free(_X1_Remain_24);
    if(_Y1_24 != NULL)free(_Y1_24);
    if(_X2_24 != NULL)free(_X2_24);
    if(_X2_Remain_24 != NULL)free(_X2_Remain_24);
    if(_Y2_24 != NULL)free(_Y2_24);

    _creepNo24= 0;
    _creepNoBytes24 = 0;
    _firstChunk24 = TRUE;
    _writtenSamples24 = 0;
    _inputSamples24 = 0;
    _highQuality24 = 0;
    _resampleFactor24 = 0.0;

    rMode = R_MODE_NONE;

    return 1;
}

WORD ResampleClass:: FilterUp(HWORD Imp[], HWORD ImpD[],
             UHWORD Nwing, BOOL Interp,
             HWORD *Xp, HWORD Ph, HWORD Inc)
{
    HWORD *Hp, *Hdp = NULL, *End;
    HWORD a = 0;
    WORD v, t;

    v=0;
    Hp = &Imp[Ph>>Na];
    End = &Imp[Nwing];

    if (Interp) {
    Hdp = &ImpD[Ph>>Na];
    a = Ph & Amask;

    }
    if (Inc == 1)       /* If doing right wing...              */
    {               /* ...drop extra coeff, so when Ph is  */
    End--;          /*    0.5, we don't do too many mult's */
    if (Ph == 0)        /* If the phase is zero...           */
    {           /* ...then we've already skipped the */
        Hp += Npc;      /*    first sample, so we must also  */
        Hdp += Npc;     /*    skip ahead in Imp[] and ImpD[] */
    }
    }
    if (Interp)
      while (Hp < End) {
      t = *Hp;      /* Get filter coeff */
      t += (((WORD)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
      Hdp += Npc;       /* Filter coeff differences step */
      t *= *Xp;     /* Mult coeff by input sample */
      if (t & (1<<(Nhxn-1)))  /* Round, if needed */
        t += (1<<(Nhxn-1));
      t >>= Nhxn;       /* Leave some guard bits, but come back some */
      v += t;           /* The filter output */
      Hp += Npc;        /* Filter coeff step */
      Xp += Inc;        /* Input signal step. NO CHECK ON BOUNDS */
      }
    else
      while (Hp < End) {
      t = *Hp;      /* Get filter coeff */
      t *= *Xp;     /* Mult coeff by input sample */
      if (t & (1<<(Nhxn-1)))  /* Round, if needed */
        t += (1<<(Nhxn-1));
      t >>= Nhxn;       /* Leave some guard bits, but come back some */
      v += t;           /* The filter output */
      Hp += Npc;        /* Filter coeff step */
      Xp += Inc;        /* Input signal step. NO CHECK ON BOUNDS */
      }

    return(v);          //-71567~71080 2bit more    //5544over 1bit? MAYBE 1bot more
}


WORD ResampleClass:: FilterUp(HWORD Imp[], HWORD ImpD[],
             UHWORD Nwing, BOOL Interp,
             WORD *Xp, HWORD Ph, HWORD Inc)
{
    short *Hp,*End;
    long long v, t;     //need 64
    WORD *Xp_Temp = Xp;

    v = 0;
    Hp = &Imp[Ph>>Na];
    End = &Imp[Nwing];

    if (Inc == 1)
        {
            End--;
            if (Ph == 0)
                {
                    Hp += Npc;
                }
        }

    int coefficientNo = End - Hp;
    int loopCount = coefficientNo >>8;
    int residue = coefficientNo - (loopCount<<8);
    if(residue)loopCount ++;
    residue = 0x00000001& loopCount;

    if(residue)                         //not multiple of 2
        {
            if(loopCount > 1)           // 3, 5, 7
                {
                    loopCount --;
                    while(loopCount)
                        {
                            t = *Hp;
                            t *= *Xp_Temp;
                            Xp_Temp += Inc;
                            v += t;
                            Hp += Npc;

                            t = *Hp;
                            t *= *Xp_Temp;
                            Xp_Temp += Inc;
                            v += t;
                            Hp += Npc;
                            loopCount = loopCount -2;
                        }

                    t = *Hp;
                    t *= *Xp_Temp;
                    Xp_Temp += Inc;
                    v += t;
                    Hp += Npc;
                }
            else{
                    t = *Hp;
                    t *= *Xp_Temp;
                    Xp_Temp += Inc;
                    v += t;
                    Hp += Npc;
                }

        }
    else{                               //multiple of 2
            while (loopCount)
                {
                    t = *Hp;
                    t *= *Xp_Temp;
                    Xp_Temp += Inc;
                    v += t;
                    Hp += Npc;

                    t = *Hp;
                    t *= *Xp_Temp;
                    Xp_Temp += Inc;
                    v += t;
                    Hp += Npc;
                    loopCount = loopCount - 2;
                }
        }

        v = v+0x4000;
    return (v>>15);
}

WORD ResampleClass:: FilterUD( HWORD Imp[], HWORD ImpD[],
             UHWORD Nwing, BOOL Interp,
             HWORD *Xp, HWORD Ph, HWORD Inc, UHWORD dhb)
{
    HWORD a;
    HWORD *Hp, *Hdp, *End;
    WORD v, t;
    UWORD Ho;

    v=0;
    Ho = (Ph*(UWORD)dhb)>>Np;
    End = &Imp[Nwing];
    if (Inc == 1)       /* If doing right wing...              */
    {               /* ...drop extra coeff, so when Ph is  */
    End--;          /*    0.5, we don't do too many mult's */
    if (Ph == 0)        /* If the phase is zero...           */
      Ho += dhb;        /* ...then we've already skipped the */
    }               /*    first sample, so we must also  */
                /*    skip ahead in Imp[] and ImpD[] */
    if (Interp)
      while ((Hp = &Imp[Ho>>Na]) < End) {
      t = *Hp;      /* Get IR sample */
      Hdp = &ImpD[Ho>>Na];  /* get interp (lower Na) bits from diff table*/
      a = Ho & Amask;   /* a is logically between 0 and 1 */
      t += (((WORD)*Hdp)*a)>>Na; /* t is now interp'd filter coeff */
      t *= *Xp;     /* Mult coeff by input sample */
      if (t & 1<<(Nhxn-1))  /* Round, if needed */
        t += 1<<(Nhxn-1);
      t >>= Nhxn;       /* Leave some guard bits, but come back some */
      v += t;           /* The filter output */
      Ho += dhb;        /* IR step */
      Xp += Inc;        /* Input signal step. NO CHECK ON BOUNDS */
      }
    else
      while ((Hp = &Imp[Ho>>Na]) < End) {
      t = *Hp;      /* Get IR sample */
      t *= *Xp;     /* Mult coeff by input sample */
      if (t & 1<<(Nhxn-1))  /* Round, if needed */
        t += 1<<(Nhxn-1);
      t >>= Nhxn;       /* Leave some guard bits, but come back some */
      v += t;           /* The filter output */
      Ho += dhb;        /* IR step */
      Xp += Inc;        /* Input signal step. NO CHECK ON BOUNDS */
      }
    return(v);
}



// word input fixed point
int ResampleClass:: FilterUD( HWORD Imp[], HWORD ImpD[],
             UHWORD Nwing, BOOL Interp,
             WORD *Xp, HWORD Ph, HWORD Inc, UHWORD dhb)
{
    HWORD *Hp, *End;
    long long v, t;
    UWORD Ho;

    v = 0;
    Ho = (Ph*(UWORD)dhb)>>Np;
    End = &Imp[Nwing];

    if (Inc == 1)
        {
            End--;
            if (Ph == 0)
                {
                    Ho +=dhb;
                }
        }

    while ((Hp = &Imp[Ho>>Na]) < End)
        {
            t = *Hp;
            t*= *Xp;
            v += t;
            Ho += dhb;
            Xp += Inc;
        }

    v += 0x4000;
    v = v>>15;

    return(v);
}

/*
return val >= 0  Success
return val < 0  Fail
*/

int ResampleClass:: initResample(double factor, int nChans, int qualityHigh, int bitsPerSample)
{

    if(bitsPerSample == 16)
        {
            return initResample(factor, nChans, qualityHigh);
        }
    else{
            //8.24 input
            return initResample24(factor, nChans, qualityHigh);
        }

}


int ResampleClass:: initResample24(double factor, int nChans, int qualityHigh)
{
    int obuffSize = (int)(((double)IBUFFSIZE)*factor+2.0 ); //ORG code has 2bytes margine.

    if(rMode != R_MODE_NONE) return -1;
    else rMode = R_MODE_24;

    _highQuality24 = qualityHigh;
    _resampleFactor24 = factor;
    _nChannel24 = nChans;

    if(qualityHigh){
            /*_Xoff = ((65+1)/2.0)*(1 upsamplecase) + 10 = 43*/
            _Xoff24 = ((LARGE_FILTER_NMULT+1)/2.0) * MAX(1.0,1.0/factor) + 10;
        }
    else {
            /*_Xoff = ((13+1)/2.0)*(1 upsamplecase) + 10 = 17*/
            _Xoff24 = ((SMALL_FILTER_NMULT+1)/2.0) * MAX(1.0,1.0/factor) + 10;
        }

    /* Calc reach of LP filter wing & give some creeping room */
    /*_Time = 17<<15*/
    _Time24 = (_Xoff24<<Np);
    _XoffSizeByte24 = _Xoff24 *2 *2;
    _offsetSizeX2inBytes24 = _Xoff24 *4*2;

    _X1_24 = (int*)malloc(sizeof(int)*IBUFFSIZE +_offsetSizeX2inBytes24);     //MAX InputData Size = sizeof(short)*IBUFFSIZE
    if(_X1_24 == NULL)return -1;
    memset(_X1_24,0x00,sizeof(int)*IBUFFSIZE+_offsetSizeX2inBytes24);

    _X1_Remain_24 = (int*)malloc(_offsetSizeX2inBytes24);
    if(_X1_Remain_24 == NULL){free(_X1_24);return -5;}
    memset(_X1_Remain_24,0x00,_offsetSizeX2inBytes24);

    _Y1_24 = (int*)malloc(sizeof(int)*obuffSize);
    if(_Y1_24 == NULL){free(_X1_24);free(_X1_Remain_24);return -2;}
    memset(_Y1_24,0x00,sizeof(int)*obuffSize);

    if(nChans == 2)
    {
        _X2_24 = (int*)malloc(sizeof(int)*IBUFFSIZE+_offsetSizeX2inBytes24);
        if(_X2_24 == NULL){free(_X1_24);free(_X1_Remain_24);free(_Y1_24);return -3;}
        memset(_X2_24,0x00,sizeof(int)*IBUFFSIZE+_offsetSizeX2inBytes24);

        _X2_Remain_24 = (int*)malloc(_offsetSizeX2inBytes24);
        if(_X2_Remain_24 == NULL){free(_X1_24);free(_X1_Remain_24);free(_X2_24);free(_Y1_24);return -6;}
        memset(_X2_Remain_24,0x00,_offsetSizeX2inBytes24);

        _Y2_24 = (int*)malloc(sizeof(int)*obuffSize);
        if(_Y2_24 == NULL){free(_X1_24);free(_X1_Remain_24);free(_Y1_24);free(_X2_24);free(_X2_Remain_24);return -4;}
        memset(_Y2_24,0x00,sizeof(int)*obuffSize);
    }else
    {
        _X2_24 = NULL;
        _X2_Remain_24 = NULL;
        _Y2_24 = NULL;
    }

    _creepNo24 = 0;
    _creepNoBytes24 = 0;
    _firstChunk24 = TRUE;
    _writtenSamples24 = 0;
    _inputSamples24 = 0;

    return 1;
}


//ResampleClass * createResampler()
ResampleInterface * createResampler()
{
    return new ResampleClass();
}

//void  destroyResampler(ResampleClass * rsmpl)
void  destroyResampler(ResampleInterface * rsmpl)
{
    delete rsmpl;
}

