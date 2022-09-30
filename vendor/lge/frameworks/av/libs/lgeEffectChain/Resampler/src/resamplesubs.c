/* resamplesubs.c - sampling rate conversion subroutines */
// Altered version

#include "resample.h"
#include <stdlib.h>

#include <stdio.h>
#include <math.h>
#include <string.h>

#define IBUFFSIZE 4096*8                         /* Input buffer size per channel */
#define OBUFFSIZE 4096 * 4                       /*use only for file*/

#include "smallfilter.h"
#include "largefilter.h"
#include "filterkit.h"

#define fileread 1
#define resample_Mem2 1

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

/* CAUTION: Assumes we call this for only one resample job per program run! */
/* return: 0 - notDone */
/*        >0 - index of last sample */
static int
readData(int   infd,          /* input file descriptor */
         int   inCount,       /* _total_ number of frames in input file */
         HWORD *outPtr1,      /* array receiving left chan samps */
         HWORD *outPtr2,      /* array receiving right chan samps */
         int   dataArraySize, /* size of these arrays */
         int   nChans,
         int   Xoff)          /* read into input array starting at this index */
{
   int    i, Nsamps, nret;
   static unsigned int framecount;  /* frames previously read */

  static short *ibufs = NULL;

   if (ibufs == NULL) {             /* first time called, so allocate it */
        ibufs = malloc(sizeof(short)*dataArraySize*nChans);
      if (ibufs == NULL) {
         fprintf(stderr, "readData: Can't allocate input buffers!\n");
         exit(1);
      }
      framecount = 0;               /* init this too */
   }

    memset(ibufs,0x00,sizeof(short)*dataArraySize*nChans);

   Nsamps = dataArraySize - Xoff;   /* Calculate number of samples to get */
   outPtr1 += Xoff;                 /* Start at designated sample number */
   outPtr2 += Xoff;

   nret = fread((void *)ibufs, sizeof(short), Nsamps*nChans , (FILE *)infd);

   if (nret < 0) {
     fprintf(stderr, "readData: Can't read data!\n");
     exit(1);
   }

   /* NB: sndlib pads ibufs with zeros if it reads past EOF. */
   if (nChans == 1) {
      for (i = 0; i < Nsamps; i++)
            *outPtr1++  = ibufs[i];
   }
   else {
      for (i = 0; i < Nsamps; i++) {
            *outPtr1++ = ibufs[2*i];
            *outPtr2++ = ibufs[2*i+1];
      }
   }

   framecount += Nsamps;

   if (framecount >= (unsigned)inCount)     /* return index of last samp */
   return (((Nsamps - (framecount - inCount)) - 1) + Xoff);
   else
      return 0;
}

#ifdef DEBUG
static int pof = 0;             /* positive overflow count */
static int nof = 0;             /* negative overflow count */
#endif

static __inline HWORD WordToHword(WORD v, int scl)
{
    HWORD out;
    WORD llsb = (1<<(scl-1));
    v += llsb;          /* round */
    v >>= scl;
    if (v>MAX_HWORD) {
#ifdef DEBUG
        if (pof == 0)
          fprintf(stderr, "*** resample: sound sample overflow\n");
        else if ((pof % 10000) == 0)
          fprintf(stderr, "*** resample: another ten thousand overflows\n");
        pof++;
#endif
        v = MAX_HWORD;
    } else if (v < MIN_HWORD) {
#ifdef DEBUG
        if (nof == 0)
          fprintf(stderr, "*** resample: sound sample (-) overflow\n");
        else if ((nof % 1000) == 0)
          fprintf(stderr, "*** resample: another thousand (-) overflows\n");
        nof++;
#endif
        v = MIN_HWORD;
    }
    out = (HWORD) v;
    return out;
}

/* Sampling rate conversion using linear interpolation for maximum speed.
 */
static int
  SrcLinear(HWORD X[], HWORD Y[], double factor, UWORD *Time, UHWORD Nx)
{
    HWORD iconst;
    HWORD *Xp, *Ystart;
    WORD v,x1,x2;

    double dt;                  /* Step through input signal */
    UWORD dtb;                  /* Fixed-point version of Dt */
    UWORD endTime;              /* When Time reaches EndTime, return to user */

    dt = 1.0/factor;            /* Output sampling period */
    dtb = dt*(1<<Np) + 0.5;     /* Fixed-point representation */

    Ystart = Y;
    endTime = *Time + (1<<Np)*(WORD)Nx;
    while (*Time < endTime)
    {
        iconst = (*Time) & Pmask;
        Xp = &X[(*Time)>>Np];      /* Ptr to current input sample */
        x1 = *Xp++;
        x2 = *Xp;
        x1 *= ((1<<Np)-iconst);
        x2 *= iconst;
        v = x1 + x2;
        *Y++ = WordToHword(v,Np);   /* Deposit output */
        *Time += dtb;               /* Move to next sample by time increment */
    }
    return (Y - Ystart);            /* Return number of output samples */
}

/* Sampling rate up-conversion only subroutine;
 * Slightly faster than down-conversion;
 */
static int SrcUp(HWORD X[], HWORD Y[], double factor, UWORD *Time,
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


/* Sampling rate conversion subroutine */

static int SrcUD(HWORD X[], HWORD Y[], double factor, UWORD *Time,
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


static int err_ret(char *s)
{
    fprintf(stderr,"resample: %s \n\n",s); /* Display error message  */
    return -1;
}


static int resampleWithFilter(  /* number of output samples returned */
    double factor,              /* factor = outSampleRate/inSampleRate */
    int infd,                   /* input and output file descriptors */
    int outfd,
    int inCount,                /* number of input samples to convert */
    int outCount,               /* number of output samples to compute */
    int nChans,                 /* number of sound channels (1 or 2) */
    BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
    HWORD Imp[], HWORD ImpD[],
    UHWORD LpScl, UHWORD Nmult, UHWORD Nwing)
{
    UWORD Time, Time2;          /* Current time/pos in input sample */
    UHWORD Xp, Ncreep, Xoff, Xread;

    HWORD X1[IBUFFSIZE], Y1[OBUFFSIZE]; /* I/O buffers */
    HWORD X2[IBUFFSIZE], Y2[OBUFFSIZE]; /* I/O buffers */
    UHWORD Nout, Nx;
    int i, Ycount, last;

    short *obufs = malloc(nChans*sizeof(short)* OBUFFSIZE);

    /* Account for increased filter gain when using factors less than 1 */
    if (factor < 1)
      LpScl = LpScl*factor + 0.5;

    /* Calc reach of LP filter wing & give some creeping room */
    Xoff = ((Nmult+1)/2.0) * MAX(1.0,1.0/factor) + 10;

    if (IBUFFSIZE < 2*Xoff)      /* Check input buffer size */
      return err_ret("IBUFFSIZE (or factor) is too small");

    Nx = IBUFFSIZE - 2*Xoff;     /* # of samples to process each iteration */

    last = 0;                   /* Have not read last input sample yet */
    Ycount = 0;                 /* Current sample and length of output file */
    Xp = Xoff;                  /* Current "now"-sample pointer for input */
    Xread = Xoff;               /* Position in input array to read into */
    Time = (Xoff<<Np);          /* Current-time pointer for converter */

    for (i=0; i<Xoff; X1[i++]=0); /* Need Xoff zeros at begining of sample */
    for (i=0; i<Xoff; X2[i++]=0); /* Need Xoff zeros at begining of sample */

    do {
        if (!last)              /* If haven't read last sample yet */
        {
            last = readData(infd, inCount, X1, X2, IBUFFSIZE,
                            nChans, (int)Xread);
            if (last && (last-Xoff<Nx)) { /* If last sample has been read... */
                Nx = last-Xoff; /* ...calc last sample affected by filter */
                if (Nx <= 0)
                  break;
            }
        }
        /* Resample stuff in input buffer */
        Time2 = Time;
        if (factor >= 1) {      /* SrcUp() is faster if we can use it */
            Nout=SrcUp(X1,Y1,factor,&Time,Nx,Nwing,LpScl,Imp,ImpD,interpFilt);
            if (nChans==2)
              Nout=SrcUp(X2,Y2,factor,&Time2,Nx,Nwing,LpScl,Imp,ImpD,
                         interpFilt);
        }
        else {
            Nout=SrcUD(X1,Y1,factor,&Time,Nx,Nwing,LpScl,Imp,ImpD,interpFilt);
            if (nChans==2)
              Nout=SrcUD(X2,Y2,factor,&Time2,Nx,Nwing,LpScl,Imp,ImpD,
                         interpFilt);
        }

        Time -= (Nx<<Np);       /* Move converter Nx samples back in time */
        Xp += Nx;               /* Advance by number of samples processed */
        Ncreep = (Time>>Np) - Xoff; /* Calc time accumulation in Time */
        if (Ncreep) {
            Time -= (Ncreep<<Np);    /* Remove time accumulation */
            Xp += Ncreep;            /* and add it to read pointer */
        }
        for (i=0; i<IBUFFSIZE-Xp+Xoff; i++) { /* Copy part of input signal */
            X1[i] = X1[i+Xp-Xoff]; /* that must be re-used */
            if (nChans==2)
              X2[i] = X2[i+Xp-Xoff]; /* that must be re-used */
        }

        if (last) {             /* If near end of sample... */
            last -= Xp;         /* ...keep track were it ends */
            if (!last)          /* Lengthen input by 1 sample if... */
              last++;           /* ...needed to keep flag TRUE */
        }
        Xread = i;              /* Pos in input buff to read new data into */
        Xp = Xoff;

        Ycount += Nout;
        if (Ycount>outCount) {
            Nout -= (Ycount-outCount);
            Ycount = outCount;
        }

        if (Nout > OBUFFSIZE) /* Check to see if output buff overflowed */
          return err_ret("Output array overflow");

        if (nChans==1) {
            for (i = 0; i < Nout; i++)
            obufs[i]= Y1[i];
        } else {
            for (i = 0; i < Nout; i++) {
            obufs[i*2]= Y1[i];
            obufs[i*2+1]= Y2[i];
            }
        }

        fwrite(obufs, sizeof(short), Nout*nChans , (FILE *)outfd);
        /*printf(".");*/  fflush(stdout);

    } while (Ycount<outCount); /* Continue until done */

    free(obufs);

    return(Ycount);             /* Return # of samples in output file */
}




int resample(                   /* number of output samples returned */
    double factor,              /* factor = Sndout/Sndin */
    int    infd,                /* input and output file descriptors */
    int    outfd,
    int inCount,                /* number of input samples to convert */
    int outCount,               /* number of output samples to compute */
    int nChans,                 /* number of sound channels (1 or 2) */
    BOOL interpFilt,            /* TRUE means interpolate filter coeffs */
    int fastMode,               /* 0 = highest quality, slowest speed */
    BOOL largeFilter,           /* TRUE means use 65-tap FIR filter */
    char *filterFile)           /* NULL for internal filter, else filename */
{
    UHWORD LpScl;               /* Unity-gain scale factor */
    UHWORD Nwing;               /* Filter table size */
    UHWORD Nmult;               /* Filter length for up-conversions */
    HWORD *Imp=0;               /* Filter coefficients */
    HWORD *ImpD=0;              /* ImpD[n] = Imp[n+1]-Imp[n] */

#ifdef DEBUG
    /* Check for illegal constants */
    if (Np >= 16)
      return err_ret("Error: Np>=16");
    if (Nb+Nhg+NLpScl >= 32)
      return err_ret("Error: Nb+Nhg+NLpScl>=32");
    if (Nh+Nb > 32)
      return err_ret("Error: Nh+Nb>32");
#endif

    /* Set defaults */

    if (filterFile != NULL && *filterFile != '\0') {
        if (readFilter(filterFile, &Imp, &ImpD, &LpScl, &Nmult, &Nwing))
          return err_ret("could not find filter file, "
               "or syntax error in contents of filter file");
    } else if (largeFilter) {
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
#if DEBUG
    fprintf(stderr,"Attenuating resampler scale factor by 0.95 "
            "to reduce probability of clipping\n");
#endif
    LpScl *= 0.95;
    return resampleWithFilter(factor,infd,outfd,inCount,outCount,nChans,
                              interpFilt, Imp, ImpD, LpScl, Nmult, Nwing);
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

static int resampleWithFilter_mem(  /* number of output samples returned */
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



static int resampleWithFilter_mem2Ch(  /* number of output samples returned */
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


int resample_m(
                        short*    inputbuffer,
                        short* outputbuffer,
                        int inCount,                /* number of input samples to convert */
                        BOOL lastChunk)
{

    return resample_mem(_resampleFactor, inputbuffer, outputbuffer, inCount,
                      inCount*_resampleFactor , _nChannel ,FALSE, 0, _highQuality, NULL,lastChunk);

}

int resample_mem(                   /* number of output samples returned */
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

int getResampleXoffVal()
{
    return _Xoff;
}

int getMinResampleInputSampleNo()
{
    return _Xoff * 2;       //_Xoff * 2 is limitation.
}

int getMaxResampleInputBufferSize()
{
    return sizeof(short)*IBUFFSIZE ;
}


/*
return val >= 0  Success
return val < 0  Fail
*/
int initResample(double factor, int nChans, int qualityHigh)
{
    int obuffSize = (int)(((double)IBUFFSIZE)*factor+2.0 ); //ORG code has 2bytes margine.

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

    _X1 = malloc(sizeof(short)*IBUFFSIZE +_offsetSizeX2inBytes);     //MAX InputData Size = sizeof(short)*IBUFFSIZE
    if(_X1 == NULL)return -1;
    memset(_X1,0x00,sizeof(short)*IBUFFSIZE+_offsetSizeX2inBytes);

    _X1_Remain = malloc(_offsetSizeX2inBytes);
    if(_X1_Remain == NULL){free(_X1);return -5;}
    memset(_X1_Remain,0x00,_offsetSizeX2inBytes);

    _Y1 = malloc(sizeof(short)*obuffSize);
    if(_Y1 == NULL){free(_X1);free(_X1_Remain);return -2;}
    memset(_Y1,0x00,sizeof(short)*obuffSize);

    if(nChans == 2)
    {
        _X2 = malloc(sizeof(short)*IBUFFSIZE+_offsetSizeX2inBytes);
        if(_X2 == NULL){free(_X1);free(_X1_Remain);free(_Y1);return -3;}
        memset(_X2,0x00,sizeof(short)*IBUFFSIZE+_offsetSizeX2inBytes);

        _X2_Remain = malloc(_offsetSizeX2inBytes);
        if(_X2_Remain == NULL){free(_X1);free(_X1_Remain);free(_X2);free(_Y1);return -6;}
        memset(_X2_Remain,0x00,_offsetSizeX2inBytes);

        _Y2 = malloc(sizeof(short)*obuffSize);
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

int deInitResample()
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

    return 1;
}

