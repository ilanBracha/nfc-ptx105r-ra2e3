/*
* Copyright (c) 2020 - 2026 Renesas Electronics Corporation and/or its affiliates
*
* SPDX-License-Identifier: BSD-3-Clause
*/

/*******************************************************************************************************************//**
 * @addtogroup BSP_MCU
 * @{
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Includes   <System Includes> , "Project Includes"
 **********************************************************************************************************************/
#include "bsp_api.h"

/***********************************************************************************************************************
 * Macro definitions
 **********************************************************************************************************************/
#if BSP_TZ_SECURE_BUILD
 #define BSP_TZ_STACK_SEAL_SIZE    (8U)
#else
 #define BSP_TZ_STACK_SEAL_SIZE    (0U)
#endif

/***********************************************************************************************************************
 * Typedef definitions
 **********************************************************************************************************************/

/* Defines function pointers to be used with vector table. */
typedef void (* exc_ptr_t)(void);

/***********************************************************************************************************************
 * Exported global variables (to be accessed by other files)
 **********************************************************************************************************************/

/***********************************************************************************************************************
 * Private global variables and functions
 **********************************************************************************************************************/
void    Reset_Handler(void);
void    Default_Handler(void);
int32_t main(void);

/*******************************************************************************************************************//**
 * MCU starts executing here out of reset. Main stack pointer is set up already.
 **********************************************************************************************************************/
void Reset_Handler (void)
{
    /* Initialize system using BSP. */
    SystemInit();

    /* Call user application. */
    main();

    while (1)
    {
        /* Infinite Loop. */
    }
}

/*******************************************************************************************************************//**
 * Default exception handler — diagnostic version.
 * When the debugger breaks here, inspect the g_fault_info struct to determine which exception
 * fired and what caused it.
 **********************************************************************************************************************/

/* Fault diagnostic information — inspect in debugger when stopped at BKPT */
volatile struct {
    uint32_t icsr;          /* SCB->ICSR  — VECTACTIVE[8:0] = exception number that brought us here */
    uint32_t shcsr;         /* SCB->SHCSR — System Handler Control and State Register */
    uint32_t lr;            /* LR at exception entry (EXC_RETURN value) */
    uint32_t stacked_pc;    /* PC from the exception stack frame — faulting instruction */
    uint32_t stacked_lr;    /* LR from the exception stack frame — caller of faulting function */
    uint32_t stacked_r0;    /* R0 from exception frame */
    uint32_t stacked_r1;    /* R1 from exception frame */
    uint32_t stacked_r2;    /* R2 from exception frame */
    uint32_t stacked_r3;    /* R3 from exception frame */
    uint32_t stacked_r12;   /* R12 from exception frame */
    uint32_t stacked_xpsr;  /* xPSR from exception frame */
    uint32_t vect_active;   /* Exception number extracted from ICSR */
} g_fault_info;

void Default_Handler (void)
{
    /* Capture available fault-status registers (Cortex-M23 has no CFSR/HFSR/MMFAR/BFAR) */
    g_fault_info.icsr        = SCB->ICSR;
    g_fault_info.shcsr       = SCB->SHCSR;
    g_fault_info.vect_active = (SCB->ICSR & SCB_ICSR_VECTACTIVE_Msk);

    /* Retrieve EXC_RETURN from LR and determine which stack pointer was in use */
    uint32_t exc_return;
    __ASM volatile ("mov %0, lr" : "=r" (exc_return));
    g_fault_info.lr = exc_return;

    /* Get the stack frame pointer: bit 2 of EXC_RETURN indicates PSP (1) or MSP (0) */
    uint32_t *frame_ptr;
    if (exc_return & 0x04U)
    {
        frame_ptr = (uint32_t *) __get_PSP();
    }
    else
    {
        frame_ptr = (uint32_t *) __get_MSP();
    }

    /* HW exception frame layout: R0, R1, R2, R3, R12, LR, PC, xPSR */
    g_fault_info.stacked_r0  = frame_ptr[0];
    g_fault_info.stacked_r1  = frame_ptr[1];
    g_fault_info.stacked_r2  = frame_ptr[2];
    g_fault_info.stacked_r3  = frame_ptr[3];
    g_fault_info.stacked_r12 = frame_ptr[4];
    g_fault_info.stacked_lr  = frame_ptr[5];   /* LR of the faulting context */
    g_fault_info.stacked_pc  = frame_ptr[6];   /* PC — the faulting instruction address */
    g_fault_info.stacked_xpsr = frame_ptr[7];

    /*
     * ===== HOW TO READ g_fault_info IN THE DEBUGGER =====
     *
     * vect_active: Exception number that triggered Default_Handler:
     *   2  = NMI (shouldn't land here, has its own handler)
     *   3  = HardFault
     *   4  = MemManage
     *   5  = BusFault
     *   6  = UsageFault
     *   7  = SecureFault
     *  11  = SVCall          (SVC_Handler not linked — FreeRTOS port.c missing?)
     *  12  = DebugMon
     *  14  = PendSV          (PendSV_Handler not linked — FreeRTOS port.c missing?)
     *  15  = SysTick         (SysTick_Handler not linked — FreeRTOS port.c missing?)
     *
     * stacked_pc: Address of the instruction that caused the fault.
     *             Look it up in the .map file or disassembly.
     *
     * stacked_lr: Return address of the function that was executing.
     *
     * shcsr: System Handler Control and State Register
     *   Bit 0 = SVCALLPENDED, Bit 15 = SVCALLACT
     *
     * On Cortex-M23 there is no CFSR/HFSR — HardFault is the only
     * configurable fault. If vect_active=3, the HardFault was likely
     * caused by an invalid memory access or undefined instruction.
     */

    BSP_CFG_HANDLE_UNRECOVERABLE_ERROR(0);
}

/* Main stack */
uint8_t g_main_stack[BSP_CFG_STACK_MAIN_BYTES + BSP_TZ_STACK_SEAL_SIZE] BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);

/* Heap */
BSP_DONT_REMOVE uint8_t g_heap[BSP_CFG_HEAP_BYTES] BSP_ALIGN_VARIABLE(BSP_STACK_ALIGNMENT);

/* All system exceptions in the vector table are weak references to Default_Handler. If the user wishes to handle
 * these exceptions in their code they should define their own function with the same name.
 */
#if defined(__ICCARM__)
 #define WEAK_REF_ATTRIBUTE

 #pragma weak HardFault_Handler                        = Default_Handler
 #pragma weak MemManage_Handler                        = Default_Handler
 #pragma weak BusFault_Handler                         = Default_Handler
 #pragma weak UsageFault_Handler                       = Default_Handler
 #pragma weak SecureFault_Handler                      = Default_Handler
 #pragma weak SVC_Handler                              = Default_Handler
 #pragma weak DebugMon_Handler                         = Default_Handler
 #pragma weak PendSV_Handler                           = Default_Handler
 #pragma weak SysTick_Handler                          = Default_Handler
#elif defined(__GNUC__)

 #define WEAK_REF_ATTRIBUTE    __attribute__((weak, alias("Default_Handler")))
#endif

void NMI_Handler(void);                // NMI has many sources and is handled by BSP
void HardFault_Handler(void) WEAK_REF_ATTRIBUTE;
void MemManage_Handler(void) WEAK_REF_ATTRIBUTE;
void BusFault_Handler(void) WEAK_REF_ATTRIBUTE;
void UsageFault_Handler(void) WEAK_REF_ATTRIBUTE;
void SecureFault_Handler(void) WEAK_REF_ATTRIBUTE;
void SVC_Handler(void) WEAK_REF_ATTRIBUTE;
void DebugMon_Handler(void) WEAK_REF_ATTRIBUTE;
void PendSV_Handler(void) WEAK_REF_ATTRIBUTE;
void SysTick_Handler(void) WEAK_REF_ATTRIBUTE;

/* Vector table. */
BSP_DONT_REMOVE const exc_ptr_t __VECTOR_TABLE[BSP_CORTEX_VECTOR_TABLE_ENTRIES] BSP_PLACE_IN_SECTION(
    BSP_SECTION_FIXED_VECTORS) =
{
    (exc_ptr_t) (&g_main_stack[0] + BSP_CFG_STACK_MAIN_BYTES), /*      Initial Stack Pointer     */
    Reset_Handler,                                             /*      Reset Handler             */
    NMI_Handler,                                               /*      NMI Handler               */
    HardFault_Handler,                                         /*      Hard Fault Handler        */
    MemManage_Handler,                                         /*      MPU Fault Handler         */
    BusFault_Handler,                                          /*      Bus Fault Handler         */
    UsageFault_Handler,                                        /*      Usage Fault Handler       */
    SecureFault_Handler,                                       /*      Secure Fault Handler      */
    0,                                                         /*      Reserved                  */
    0,                                                         /*      Reserved                  */
    0,                                                         /*      Reserved                  */
    SVC_Handler,                                               /*      SVCall Handler            */
    DebugMon_Handler,                                          /*      Debug Monitor Handler     */
    0,                                                         /*      Reserved                  */
    PendSV_Handler,                                            /*      PendSV Handler            */
    SysTick_Handler,                                           /*      SysTick Handler           */
};

/** @} (end addtogroup BSP_MCU) */
