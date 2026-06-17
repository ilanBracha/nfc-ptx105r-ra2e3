/** \file
    ---------------------------------------------------------------
    SPDX-License-Identifier: BSD-3-Clause

    Copyright (c) 2024, Renesas Electronics Corporation and/or its affiliates


    Redistribution and use in source and binary forms, with or without modification,
    are permitted provided that the following conditions are met:

    1. Redistributions of source code must retain the above copyright notice, this
       list of conditions and the following disclaimer.

    2. Redistributions in binary form must reproduce the above copyright notice, this list of
       conditions and the following disclaimer in the documentation and/or other
       materials provided with the distribution.

    3. Neither the name of Renesas nor the names of its
       contributors may be used to endorse or promote products derived from this
       software without specific prior written permission.



    THIS SOFTWARE IS PROVIDED BY Renesas "AS IS" AND ANY EXPRESS
    OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
    OF MERCHANTABILITY, NONINFRINGEMENT, AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL RENESAS OR CONTRIBUTORS BE
    LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
    CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
    GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
    HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
    LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
    OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    ---------------------------------------------------------------

    Project     : PTX1K
    Module      : PLAT
    File        : ptxPLAT_EXT.h

    Description : Common include for External #includes / 3rd Party Code
*/

#ifndef COMPS_PLAT_PTXPLATEXT_H_
#define COMPS_PLAT_PTXPLATEXT_H_


/*
 * ####################################################################################################################
 * INCLUDES
 * ####################################################################################################################
 */

#if defined(__GNUC__) || defined(__GNUG__)
    #pragma GCC diagnostic push
    #pragma GCC diagnostic ignored "-Wmissing-prototypes"
    #pragma GCC diagnostic ignored "-Wstrict-prototypes"
        #ifdef RM_NFC_READER_PTX_H_
            #include "bsp_api.h"
            #include "r_ioport_api.h"
            #include "r_timer_api.h"
            #include "rm_comms_api.h"
        #else
            #include "hal_data.h"
        #endif
        #include "r_external_irq_api.h"
        #include "cmsis_gcc.h"
    #pragma GCC system_header
    #pragma GCC diagnostic pop
#else
    #ifdef RM_NFC_READER_PTX_H_
        #include "bsp_api.h"
        #include "r_ioport_api.h"
        #include "r_timer_api.h"
        #include "rm_comms_api.h"
    #else
        #include "hal_data.h"
    #endif
    #include "r_external_irq_api.h"
    #include "cmsis_gcc.h"
#endif

#endif /* Guard */

