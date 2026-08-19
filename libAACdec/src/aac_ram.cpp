/* -----------------------------------------------------------------------------
Software License for The Fraunhofer FDK AAC Codec Library for Android

© Copyright  1995 - 2020 Fraunhofer-Gesellschaft zur Förderung der angewandten
Forschung e.V. All rights reserved.

 1.    INTRODUCTION
The Fraunhofer FDK AAC Codec Library for Android ("FDK AAC Codec") is software
that implements the MPEG Advanced Audio Coding ("AAC") encoding and decoding
scheme for digital audio. This FDK AAC Codec software is intended to be used on
a wide variety of Android devices.

AAC's HE-AAC and HE-AAC v2 versions are regarded as today's most efficient
general perceptual audio codecs. AAC-ELD is considered the best-performing
full-bandwidth communications codec by independent studies and is widely
deployed. AAC has been standardized by ISO and IEC as part of the MPEG
specifications.

Patent licenses for necessary patent claims for the FDK AAC Codec (including
those of Fraunhofer) may be obtained through Via Licensing
(www.vialicensing.com) or through the respective patent owners individually for
the purpose of encoding or decoding bit streams in products that are compliant
with the ISO/IEC MPEG audio standards. Please note that most manufacturers of
Android devices already license these patent claims through Via Licensing or
directly from the patent owners, and therefore FDK AAC Codec software may
already be covered under those patent licenses when it is used for those
licensed purposes only.

Commercially-licensed AAC software libraries, including floating-point versions
with enhanced sound quality, are also available from Fraunhofer. Users are
encouraged to check the Fraunhofer website for additional applications
information and documentation.

2.    COPYRIGHT LICENSE

Redistribution and use in source and binary forms, with or without modification,
are permitted without payment of copyright license fees provided that you
satisfy the following conditions:

You must retain the complete text of this software license in redistributions of
the FDK AAC Codec or your modifications thereto in source code form.

You must retain the complete text of this software license in the documentation
and/or other materials provided with redistributions of the FDK AAC Codec or
your modifications thereto in binary form. You must make available free of
charge copies of the complete source code of the FDK AAC Codec and your
modifications thereto to recipients of copies in binary form.

The name of Fraunhofer may not be used to endorse or promote products derived
from this library without prior written permission.

You may not charge copyright license fees for anyone to use, copy or distribute
the FDK AAC Codec software or your modifications thereto.

Your modified versions of the FDK AAC Codec must carry prominent notices stating
that you changed the software and the date of any change. For modified versions
of the FDK AAC Codec, the term "Fraunhofer FDK AAC Codec Library for Android"
must be replaced by the term "Third-Party Modified Version of the Fraunhofer FDK
AAC Codec Library for Android."

3.    NO PATENT LICENSE

NO EXPRESS OR IMPLIED LICENSES TO ANY PATENT CLAIMS, including without
limitation the patents of Fraunhofer, ARE GRANTED BY THIS SOFTWARE LICENSE.
Fraunhofer provides no warranty of patent non-infringement with respect to this
software.

You may use this FDK AAC Codec software or modifications thereto only for
purposes that are authorized by appropriate patent licenses.

4.    DISCLAIMER

This FDK AAC Codec software is provided by Fraunhofer on behalf of the copyright
holders and contributors "AS IS" and WITHOUT ANY EXPRESS OR IMPLIED WARRANTIES,
including but not limited to the implied warranties of merchantability and
fitness for a particular purpose. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE for any direct, indirect, incidental, special, exemplary,
or consequential damages, including but not limited to procurement of substitute
goods or services; loss of use, data, or profits, or business interruption,
however caused and on any theory of liability, whether in contract, strict
liability, or tort (including negligence), arising in any way out of the use of
this software, even if advised of the possibility of such damage.

5.    CONTACT INFORMATION

Fraunhofer Institute for Integrated Circuits IIS
Attention: Audio and Multimedia Departments - FDK AAC LL
Am Wolfsmantel 33
91058 Erlangen, Germany

www.iis.fraunhofer.de/amm
amm-info@iis.fraunhofer.de
----------------------------------------------------------------------------- */

/**************************** AAC decoder library ******************************

   Author(s):   Josef Hoepfl

   Description:

*******************************************************************************/

/*
 * NOTICE OF MODIFICATION
 *
 * This file has been changed from the Fraunhofer FDK AAC Codec Library as
 * distributed. Modified 2026-08-19: the WorkBufferCore2 and WorkBufferCore5
 * work buffers are sized for two channels rather than eight, as described
 * at AAW_DEC_MAX_CHANNELS below. No other behaviour is altered.
 *
 * Per section 2 of the software license above, this is a Third-Party
 * Modified Version of the Fraunhofer FDK AAC Codec Library for Android.
 */

#include "aac_ram.h"
#include "aac_rom.h"

#define WORKBUFFER1_TAG 0
#define WORKBUFFER2_TAG 1
#define WORKBUFFER5_TAG 6
#define WORKBUFFER6_TAG 7

/*
 * The widest channel configuration these work buffers are built for.
 *
 * Upstream sizes them at 8, which is what a general-purpose decoder needs.
 * WorkBufferCore5 alone is then (8 * 4096 * 2) FIXP_DBL - 256 KB - and
 * WorkBufferCore2 another 32 KB, all of it taken when the decoder opens
 * and none of it usable by anything else.
 *
 * This build decodes CarPlay audio, which is stereo at the most, and it
 * decodes it on a board where the whole heap is under a megabyte. Two
 * streams' worth of 8-channel buffers does not fit beside a screen buffer
 * and an audio ring, and the way that failed was not a clean allocation
 * error - it was the session dying with the heap down to 2 KB free.
 *
 * Two is safe here rather than merely smaller: cp_aac_open refuses any
 * channel count above two before a decoder is ever configured, so a
 * stream that would need these buffers wider cannot reach them.
 */
#define AAW_DEC_MAX_CHANNELS 2

/*! The structure AAC_DECODER_INSTANCE is the top level structure holding all
   decoder configurations, handles and structs.
 */
C_ALLOC_MEM(AacDecoder, struct AAC_DECODER_INSTANCE, 1)

/*!
  \name StaticAacData

  Static memory areas, must not be overwritten in other sections of the decoder
*/
/* @{ */

/*! The structure CAacDecoderStaticChannelInfo contains the static sideinfo
   which is needed for the decoding of one aac channel. <br> Dimension:
   #AacDecoderChannels                                                      */
C_ALLOC_MEM2(AacDecoderStaticChannelInfo, CAacDecoderStaticChannelInfo, 1, (8))

/*! The structure CAacDecoderChannelInfo contains the dynamic sideinfo which is
   needed for the decoding of one aac channel. <br> Dimension:
   #AacDecoderChannels                                                      */
C_AALLOC_MEM2(AacDecoderChannelInfo, CAacDecoderChannelInfo, 1, (8))

/*! Overlap buffer */
C_AALLOC_MEM2(OverlapBuffer, FIXP_DBL, OverlapBufferSize, (8))

C_ALLOC_MEM(DrcInfo, CDrcInfo, 1)

/*! The structure CpePersistentData holds the persistent data shared by both
   channels of a CPE. <br> It needs to be allocated for each CPE. <br>
    Dimension: 1 */
C_ALLOC_MEM(CpePersistentData, CpePersistentData, 1)

/*! The structure CCplxPredictionData holds data for complex stereo prediction.
   <br> Dimension: 1
 */
C_ALLOC_MEM(CplxPredictionData, CCplxPredictionData, 1)

/*! The buffer holds time samples for the crossfade in case of an USAC DASH IPF
   config change Dimension: (8)
 */
C_ALLOC_MEM2(TimeDataFlush, PCM_DEC, TIME_DATA_FLUSH_SIZE, (8))

/* @} */

/*!
  \name DynamicAacData

  Dynamic memory areas, might be reused in other algorithm sections,
  e.g. the sbr decoder
*/

/* Take into consideration to make use of the WorkBufferCore[3/4] for decoder
 * configurations with more than 2 channels */
C_ALLOC_MEM_OVERLAY(WorkBufferCore2, FIXP_DBL,
                    ((AAW_DEC_MAX_CHANNELS) * 1024), SECT_DATA_L2,
                    WORKBUFFER2_TAG)

C_ALLOC_MEM_OVERLAY(WorkBufferCore6, SCHAR,
                    fMax((INT)(sizeof(FIXP_DBL) * WB_SECTION_SIZE),
                         (INT)sizeof(CAacDecoderCommonData)),
                    SECT_DATA_L2, WORKBUFFER6_TAG)

C_ALLOC_MEM_OVERLAY(WorkBufferCore1, CWorkBufferCore1, 1, SECT_DATA_L1,
                    WORKBUFFER1_TAG)

/* double buffer size needed for de-/interleaving */
C_ALLOC_MEM_OVERLAY(WorkBufferCore5, PCM_DEC,
                    (AAW_DEC_MAX_CHANNELS) * (1024 * 4) * 2,
                    SECT_DATA_EXTERN, WORKBUFFER5_TAG)
