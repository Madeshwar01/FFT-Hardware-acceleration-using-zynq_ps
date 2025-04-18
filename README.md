# FFT-Hardware-acceleration-using-zynq-ps
## Overview
Implemented the Fast Fourier Transform (FFT) using a hardware accelerator in the Zynq PL. Generated input data is first loaded into the PS DDR memory by the controlling Vitis software. The CPU then copies this data into an intermediate Block RAM (BRAM) in the PL. Subsequently, an AXI DMA controller reads the data from the intermediate BRAM, streams it to the FFT IP core, receives the results from the FFT, and writes them to a final Output BRAM in the PL. The PS can then read the results from the Output BRAM.
## Architecture: 
Processing System (PS): ARM Cortex-A9 runs control software, generates input data in DDR, performs the initial CPU copy to PL BRAM, configures/monitors AXI DMA.
Programmable Logic (PL): Contains hardware accelerators and memory:
Intermediate BRAM (blk_mem_gen_0, Dual-Port) + AXI BRAM Controller (axi_bram_ctrl_0).
AXI DMA Controller (axi_dma_0) with MM2S and S2MM channels.
FFT IP Core (xfft_0).
Output BRAM (blk_mem_gen_1, Dual-Port) + AXI BRAM Controller (axi_bram_ctrl_1).
AXI SmartConnect (axi_smc) for routing AXI Memory-Mapped traffic.
Processor System Reset Module (proc_sys_reset_0).
## Data Flow:
CPU Copy: PS DDR (Input Data) -> PS (M_AXI_GP0) -> AXI SmartConnect -> Intermediate BRAM Controller (axi_bram_ctrl_0) -> Intermediate BRAM (blk_mem_gen_0).
DMA Transfer: Intermediate BRAM (blk_mem_gen_0) -> Intermediate BRAM Controller (axi_bram_ctrl_0) -> AXI SmartConnect <- AXI DMA (MM2S Read) -> AXI DMA (MM2S Stream Out) -> FFT IP Core -> FFT IP Core (Stream Out) -> AXI DMA (S2MM Stream In) -> AXI DMA (S2MM Write) -> AXI SmartConnect -> Output BRAM Controller (axi_bram_ctrl_1) -> Output BRAM (blk_mem_gen_1).
Result Retrieval: PS (M_AXI_GP0) -> AXI SmartConnect -> Output BRAM Controller (axi_bram_ctrl_1) -> Output BRAM (blk_mem_gen_1).
## Control Flow: 
The PS generates data, performs the CPU copy, configures and starts the AXI DMA for the main transfer, polls for DMA completion, and finally reads the results from the Output BRAM.
