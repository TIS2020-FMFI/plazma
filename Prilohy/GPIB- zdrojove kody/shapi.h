
/*++

Copyright (c) 2011 Test Equipment Plus.

Module Name:

    SHAPI.h

Abstract:

    Native Signal Hound / USB device driver for Signal Hound Firmware version 1.23 or later
    Signal Hound API library definitions


Revision History:

	10/2011	Re-created from the DLL.

	
--*/

#pragma once

// PLEASE NOTE: To support multiple devices operating simultaneously, several functions have a new parameter:
//  int deviceNum = -1.  You can omit this and it will use the currently selected device, or you may pass a device number as a reference


//////////////////////////////////////////////////////////
// int SHAPI_Initialize()
// Initializes USB and loads cal table.
// TAKES 15 seconds to complete!!!
// Arguments: none 
// Returns 0 for success, otherwise returns error code.
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_Initialize();

//////////////////////////////////////////////////////////
// SHAPI_Configure[Fast](double attenVal, );
// Sets attenuator to nearest value of 0,5,10, or 15 dB
// Selects mixer as either low band (value=0); or high band (value=1);
// Selects sensitivity as 0=low, 1=mid, or 2=high sensitivity
// Selects decimation as [1..16]
// Selects IF path as either 2.92 (value=1); or 10.7 MHz (value=0);
// Selects clock as 23.33 MHz (value=0); or 22.5 MHz (value=1); 
// Arguments: double attenVal=10, int mixerBand=1, int sensitivity=0,  
//												   int decimation=1, int useIF2_9=0, int ADCclock=0
// Returns 0 for success, otherwise returns error code.
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_Configure(double attenVal=10.0, int mixerBand=1, int sensitivity=0,  
												   int decimation=1, int useIF2_9=0, int ADCclock=0);
extern "C" __declspec( dllimport ) int SHAPI_ConfigureFast(double attenVal=10.0, int mixerBand=1, int sensitivity=0,  
												   int decimation=1, int useIF2_9=0, int ADCclock=0);
//////////////////////////////////////////////////////////
// SHAPI_GetSlowSweepCount(double startFreq, double stopFreq, int FFTSize );
// Returns count of samples required for specified sweep
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_GetSlowSweepCount(double startFreq, double stopFreq, int FFTSize);
extern "C" __declspec( dllimport ) int SHAPI_GetSlowSweep(double * dBArray, double startFreq, double stopFreq,  
													  int &returnCount, int FFTSize=1024, 
													  int avgCount=16, int imageHandling=0);
extern "C" __declspec( dllimport ) int SHAPI_GetFastSweepCount(double startFreq, double stopFreq, int FFTSize);
extern "C" __declspec( dllimport ) int SHAPI_CyclePort();
extern "C" __declspec( dllimport ) int SHAPI_GetFastSweep(double * dBArray, double startFreq, double stopFreq,  
													  int &returnCount, int FFTSize=16, int imageHandling=0);

extern "C" __declspec( dllimport ) float SHAPI_GetTemperature();
extern "C" __declspec( dllimport ) int SHAPI_LoadTemperatureCorrections(LPCSTR filename);
extern "C" __declspec( dllimport ) void SHAPI_SetPreamp(int val);
extern "C" __declspec( dllimport ) int SHAPI_IsPreampAvailable();
extern "C" __declspec( dllimport ) double SHAPI_GetFMFFTPK();
extern "C" __declspec( dllimport ) double SHAPI_GetAudioFFTSample(int idx);
extern "C" __declspec( dllimport ) unsigned int SHAPI_GetSerNum();
extern "C" __declspec( dllimport ) void SHAPI_SyncTriggerMode(int mode);
extern "C" __declspec( dllimport ) void SHAPI_CyclePowerOnExit();
extern "C" __declspec( dllimport ) double SHAPI_GetAMFFTPK();
extern "C" __declspec( dllimport ) void SHAPI_ActivateAudioFFT();
extern "C" __declspec( dllimport ) void SHAPI_DeactivateAudioFFT();

//////////////////////////////////////////////////////////
// double SHAPI_GetRBW(int FFTSize, int decimation)
// Arguments: FFT size, decimation setting
// Returns calculated estimated RBW.
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) double SHAPI_GetRBW(int FFTSize, int decimation);

extern "C" __declspec( dllimport ) double SHAPI_GetLastChannelPower();
extern "C" __declspec( dllimport ) double SHAPI_GetLastChannelFreq();
extern "C" __declspec( dllimport ) void SHAPI_SetOscRatio(double ratio);


extern "C" __declspec( dllimport ) void SHAPI_WriteCalTable(unsigned char *myTable);
extern "C" __declspec( dllimport ) int SHAPI_InitializeNext();
extern "C" __declspec( dllimport ) int SHAPI_SelectDevice(int deviceToSelect);
extern "C" __declspec( dllimport ) int SHAPI_CopyCalTable(unsigned char * p4KTable, int deviceToSelect=0);
extern "C" __declspec( dllimport ) int SHAPI_InitializeEx(unsigned char * p4KTable, int deviceToSelect=0);

//////////////////////////////////////////////////////////
// int SHAPI_SetAttenuator(double attenVal)
// Sets attenuator to nearest value of 0,5,10, or 15 dB
// Arguments: double attenVal.
// Returns 0 for success, otherwise returns error code.
//////////////////////////////////////////////////////////

extern "C" __declspec( dllimport ) int SHAPI_SetAttenuator(double attenVal);

//////////////////////////////////////////////////////////
// int SHAPI_SelectExt10MHz()
// Selects external reference
// Returns 0 for success, otherwise returns error code.
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_SelectExt10MHz();

//////////////////////////////////////////////////////////
// SHAPI_GetIQDataPacket();
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_GetIQDataPacket(int * pIData, int * pQData, double &centerFreq, int size);

//////////////////////////////////////////////////////////
// SHAPI_Authenticate();. Returns 1 if security is validated
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_Authenticate(int vendorcode=0);
extern "C" __declspec( dllimport ) int SHAPI_SetupLO(double &centerFreq, int mixMode=1, int deviceNum=-1);

//////////////////////////////////////////////////////////
// SHAPI_StartStreamingData();;
// From here, repeatedly use GetStreamingPacket
// Then call StopStreamingData before doing anything else!!!
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_StartStreamingData(int deviceNum=-1);

//////////////////////////////////////////////////////////
// SHAPI_StopStreamingData();;
// After using GetStreamingPacket
//  call StopStreamingData before doing anything else!!!
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_StopStreamingData(int deviceNum=-1);

//////////////////////////////////////////////////////////
// SHAPI_GetStreamingPacket(int *bufI, int *bufQ);;
// Gets 4K tokens
// Then call StopStreamingData before doing anything else!!!
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_GetStreamingPacket(int *bufI, int *bufQ, int deviceNum=-1);
extern "C" __declspec( dllimport ) double SHAPI_GetPhaseStep();
extern "C" __declspec( dllimport ) double SHAPI_GetChannelPower(double cf, int *iBigI, int *iBigQ, int count);

extern "C" __declspec( dllimport ) int SHAPI_GetIntFFT(int FFTSize, int *iBigI, int *iBigQ, double * dFFTOut);

//////////////////////////////////////////////////////////
// SHAPI_SetupFastSweepLoop();
// For up to 16384 data pts per.  that means 32 * 200 = 6.4 MHz per chunk max!!!
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_SetupFastSweepLoop(double startFreq, double stopFreq, int &returnCount, int MaxFFTSize=16, int imageHandling=0, int AvgCount=1, int deviceNum = -1);

//////////////////////////////////////////////////////////
// SHAPI_SetupMultiFreqSweepLoop();
// For up to 16384 data pts per.  that means 32 * 200 = 6.4 MHz per chunk max!!!
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_SetupMultiFreqSweepLoop(int count, double * freqbuf, int deviceNum=-1);

//////////////////////////////////////////////////////////
// SHAPI_GetFSLoopData();  Assume 1 segment looped, or 2 for image rejection on.  Buf size from GetIQSize
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_GetMultiFreqIQ(int count, int size, int * pI1, int * pQ1, int deviceNum=-1);

//////////////////////////////////////////////////////////
// SHAPI_SetupMultiFreqSweepLoop();
// For up to 16384 data pts per.  that means 32 * 200 = 6.4 MHz per chunk max!!!
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_SetupMultiBandSweepLoop(int count, double * freqbuf, int * freqsize, int deviceNum=-1);

//////////////////////////////////////////////////////////
//SHAPI_GetMultiBandIQ
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_GetMultiBandIQ(int count, int size, int * pI1, int * pQ1, int deviceNum=-1);

//////////////////////////////////////////////////////////
// SHAPI_GetFSLoopIQSize();  Allocate this many ints for EACH buffer
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_GetFSLoopIQSize(double startFreq, double stopFreq, int MaxFFTSize=16);

//////////////////////////////////////////////////////////
// SHAPI_GetFSLoopData();  Assume 1 segment looped, or 2 for image rejection on.  Buf size from GetIQSize
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_GetFSLoopIQ(int * pI1, int * pQ1, int * pI2, int * pQ2, double startFreq, double stopFreq, int MaxFFTSize=16, int imageHandling=0, int deviceNum=-1);

//////////////////////////////////////////////////////////
// SHAPI_GetFSLoopData();  Assume 1 segment looped, or 2 for image rejection on
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int SHAPI_ProcessFSLoopData(double * dBArray, int * pI1, int * pQ1, int * pI2, int * pQ2, double startFreq, double stopFreq, 
												int &returnCount, int FFTSize=16,int MaxFFTSize=16, int imageHandling=0);

												//  ************* USING THE MEASURING RECEIVER **********************
//  1); SHAPI_Configure - 2.9 MHz IF strongly recommended
//  2); SHAPI_RunMeasurementReceiver(MEAS_RCVR_STRUCT * LPMRStruct);
//


//////////////////////////////////////////////////////////
// SHAPI_SetupMeasRcvr();  Assume 1 segment looped, or 2 for image rejection on
//////////////////////////////////////////////////////////
extern "C" __declspec( dllimport ) int  SHAPI_RunMeasurementReceiver(void * LPStruct);

//  ************* USING THE Tracking Generator **********************
//  //ALC not enabled - relative measuremetns only
//  Set up for 10.7 MHz IF, decimation = 1


//////////////////////////////////////////////////////////
// SHAPI_ProcTGSweep();  Fills dBArray with power levels at "Count" frequency points 
//////////////////////////////////////////////////////////

extern "C" __declspec( dllimport ) int  SHAPI_ProcTGSweep(double * dBArray, double startFreq, double stepSize, int count, int attenval); // Fill

extern "C" __declspec( dllimport ) int  SHAPI_SetTGFreqAtten(double freq, unsigned char atten); // Fill

extern "C" __declspec( dllimport ) void  SHAPI_TGSyncOff();

extern "C" __declspec( dllimport ) int  SHAPI_BBSPSweep(double * pData, int startfreq, int stopfreq, int stepfreq); //in MHz

extern "C" __declspec( dllimport ) int  SHAPI_ProcessData(double * pAmpData, double * pFreqData, int * pIData, int * pQData, double ctrfreq, int datacount, int fftsz ); //in MHz
//Avgmode=0 for off, 1 for log, 2 for power

extern "C" __declspec( dllimport ) int  SHAPI_ProcessLoopPatch(double * pAmpData, double * pFreqData, int * pIDataH, int * pQDataH,int * pIDataL, int * pQDataL, double ctrfreq, int fftsz, bool rejectImage ); // max fftsz 256;

extern "C" __declspec( dllimport ) int  SHAPI_BuildIQFIR(double * pReal, double * pImag); // max fftsz 256;

extern "C" __declspec( dllimport ) int  SHAPI_ProcIQ(double freq, double * dIout, double * dQout, int * pI, int * pQ, int * pIold, int * pQold, double * pFIRRe, double * pFIRIm); // max fftsz 256;
