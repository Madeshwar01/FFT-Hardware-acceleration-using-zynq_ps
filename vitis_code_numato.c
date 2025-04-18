/******************************************************************************


* Copyright (C) 2023 Advanced Micro Devices, Inc. All Rights Reserved.


* SPDX-License-Identifier: MIT


******************************************************************************/


/*


* helloworld.c: simple test application


*


* This application configures UART 16550 to baud rate 9600.


* PS7 UART (Zynq) is not initialized by this application, since


* bootrom/bsp configures it to baud rate 115200


*


* ------------------------------------------------


* | UART TYPE   BAUD RATE                        |


* ------------------------------------------------


*   uartns550   9600


*   uartlite    Configurable only in HW design


*   ps7_uart    115200 (configured by bootrom/bsp)


*/


/***************************** Include Files *********************************/


#include <stdio.h>
#include <string.h>
#include "platform.h"
#include "xil_printf.h"
#include "xparameters.h"
#include "xaxidma.h"
#include "xaxidma_hw.h"
#include "xil_cache.h"
#include "xstatus.h"


/************************** Constant Definitions *****************************/


#define DMA_DEV_ID                  0
#define INTERMEDIATE_BRAM_BASE_ADDR XPAR_AXI_BRAM_CTRL_0_BASEADDR
#define OUTPUT_BRAM_BASE_ADDR       XPAR_AXI_BRAM_CTRL_1_BASEADDR
#define DDR_SOURCE_ADDR             0x01000000


#define FFT_N_POINTS                1024
#define BYTES_PER_SAMPLE            4
#define TRANSFER_LENGTH             (FFT_N_POINTS * BYTES_PER_SAMPLE)


#ifndef XIL_CACHE_LINE_SIZE
#define XIL_CACHE_LINE_SIZE 32
#endif


/************************** Variable Definitions *****************************/


XAxiDma AxiDma;


u32 DDR_Source_Buffer[FFT_N_POINTS] __attribute__ ((aligned (XIL_CACHE_LINE_SIZE)));


const u32 input_data_hex[] = {
   0x00000000, 0x00003426, 0x0000533B, 0x000052E7, 0x0000387E, 0x000015A3, 0x0000FE5C, 0x0000FE26,
   0x000011CC, 0x00002981, 0x000031EA, 0x00001F45, 0x0000F479, 0x0000C238, 0x00009E98, 0x0000999A,
   0x0000B4CD, 0x0000E297, 0x00000D77, 0x00002356, 0x00001ED9, 0x000009A4, 0x0000F669, 0x0000F614,
   0x00000DFD, 0x0000347C, 0x000055D8, 0x00005ED7, 0x0000474A, 0x000016CC, 0x0000E127, 0x0000BC3A,
   0x0000B4D6, 0x0000C87D, 0x0000E75E, 0x0000FD6E, 0x0000FD93, 0x0000E8DC, 0x0000CDD4, 0x0000C04B,
   0x0000CE16, 0x0000F6EF, 0x00002BE7, 0x000056C6, 0x00006578, 0x00005355, 0x00002B56, 0x00000205,
   0x0000EA78, 0x0000EC4A
};


const u32 num_provided_samples = sizeof(input_data_hex) / sizeof(input_data_hex[0]);


/************************** Function Prototypes ******************************/


int InitAxiDma(u16 DeviceId);
int PrepareInputData(u32* ddrBuffer, u32 numSamplesToLoad);
int DoCpuCopyToBram(UINTPTR SourceDdrAddr, UINTPTR DestBramAddr, u32 Length);
int DoDmaFftTransfer(XAxiDma *AxiDmaInst, UINTPTR IntermedBramAddr, UINTPTR OutputBramAddr, u32 Length);
int ReadAndPrintResults(UINTPTR OutputBramAddr, u32 NumSamples);


/************************** Main Function ************************************/


int main()
{
   int Status;


   init_platform();


   xil_printf("\r\n--- Entering FFT DMA Test (DDR Input via CPU Copy) --- \r\n");
   xil_printf("Intermediate BRAM Addr: 0x%08lX\r\n", (u32)INTERMEDIATE_BRAM_BASE_ADDR);
   xil_printf("Output BRAM Addr:       0x%08lX\r\n", (u32)OUTPUT_BRAM_BASE_ADDR);
   xil_printf("DDR Source Addr:        0x%08lX\r\n", (u32)DDR_SOURCE_ADDR);
   xil_printf("Transfer Size:          %lu bytes (%d samples)\r\n", (u32)TRANSFER_LENGTH, FFT_N_POINTS);


   Status = PrepareInputData(DDR_Source_Buffer, FFT_N_POINTS);
   if (Status != XST_SUCCESS) return XST_FAILURE;


   Status = InitAxiDma(DMA_DEV_ID);
   if (Status != XST_SUCCESS) {
       xil_printf("AXI DMA Initialization failed.\r\n");
       cleanup_platform();
       return XST_FAILURE;
   }


   Status = DoCpuCopyToBram((UINTPTR)DDR_Source_Buffer, INTERMEDIATE_BRAM_BASE_ADDR, TRANSFER_LENGTH);
   if (Status != XST_SUCCESS) {
       xil_printf("Phase 1 CPU Copy failed.\r\n");
       cleanup_platform();
       return XST_FAILURE;
   }


   Status = DoDmaFftTransfer(&AxiDma, INTERMEDIATE_BRAM_BASE_ADDR, OUTPUT_BRAM_BASE_ADDR, TRANSFER_LENGTH);
   if (Status != XST_SUCCESS) {
       xil_printf("Phase 2 DMA Transfer failed.\r\n");
       cleanup_platform();
       return XST_FAILURE;
   }


   Status = ReadAndPrintResults(OUTPUT_BRAM_BASE_ADDR, FFT_N_POINTS);
   if (Status != XST_SUCCESS) {
       xil_printf("Reading results failed.\r\n");
   }


   xil_printf("--- Exiting FFT DMA Test --- \r\n");


   cleanup_platform();
   return XST_SUCCESS;
}


/************************** Support Functions ******************************/


int InitAxiDma(u16 DeviceId)
{
   XAxiDma_Config *CfgPtr = XAxiDma_LookupConfig(DeviceId);
   if (!CfgPtr) {
       xil_printf("DMA config not found.\r\n");
       return XST_FAILURE;
   }


   if (XAxiDma_CfgInitialize(&AxiDma, CfgPtr) != XST_SUCCESS) return XST_FAILURE;


   if (XAxiDma_HasSg(&AxiDma)) {
       xil_printf("WARNING: DMA configured for Scatter-Gather, but code uses Simple mode.\r\n");
   }


   XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DEVICE_TO_DMA);
   XAxiDma_IntrDisable(&AxiDma, XAXIDMA_IRQ_ALL_MASK, XAXIDMA_DMA_TO_DEVICE);
   XAxiDma_Reset(&AxiDma);
   while (!XAxiDma_ResetIsDone(&AxiDma)) {}


   return XST_SUCCESS;
}


int PrepareInputData(u32* ddrBuffer, u32 numTotalSamples)
{
   xil_printf("Preparing input data in DDR...\r\n");


   if (num_provided_samples > numTotalSamples) return XST_FAILURE;


   memcpy(ddrBuffer, input_data_hex, num_provided_samples * sizeof(u32));
   memset(&ddrBuffer[num_provided_samples], 0, (numTotalSamples - num_provided_samples) * sizeof(u32));
   Xil_DCacheFlushRange((UINTPTR)ddrBuffer, numTotalSamples * sizeof(u32));


   return XST_SUCCESS;
}


int DoCpuCopyToBram(UINTPTR SourceDdrAddr, UINTPTR DestBramAddr, u32 Length)
{
   volatile u32 *Src = (volatile u32 *)SourceDdrAddr;
   volatile u32 *Dst = (volatile u32 *)DestBramAddr;
   u32 words = Length / sizeof(u32);


   for (u32 i = 0; i < words; i++) {
       Dst[i] = Src[i];
   }


   return XST_SUCCESS;
}


int DoDmaFftTransfer(XAxiDma *AxiDmaInst, UINTPTR IntermedBramAddr, UINTPTR OutputBramAddr, u32 Length)
{
   int Status;


   Status = XAxiDma_SimpleTransfer(AxiDmaInst, OutputBramAddr, Length, XAXIDMA_DEVICE_TO_DMA);
   if (Status != XST_SUCCESS) return XST_FAILURE;


   Status = XAxiDma_SimpleTransfer(AxiDmaInst, IntermedBramAddr, Length, XAXIDMA_DMA_TO_DEVICE);
   if (Status != XST_SUCCESS) return XST_FAILURE;


   while (XAxiDma_Busy(AxiDmaInst, XAXIDMA_DMA_TO_DEVICE)) {}
   while (XAxiDma_Busy(AxiDmaInst, XAXIDMA_DEVICE_TO_DMA)) {}


   return XST_SUCCESS;
}


int ReadAndPrintResults(UINTPTR OutputBramAddr, u32 NumSamples)
{
   volatile u32 *OutputPtr = (volatile u32 *)OutputBramAddr;


   xil_printf("FFT Output Samples:\r\n");
   for (u32 i = 0; i < NumSamples; i++) {
       u32 val = OutputPtr[i];
       u16 real = (u16)(val >> 16);
       u16 imag = (u16)(val & 0xFFFF);
       xil_printf("  [%3u] Real: %6d, Imag: %6d\r\n", i, (s16)real, (s16)imag);


   }


   return XST_SUCCESS;
}





