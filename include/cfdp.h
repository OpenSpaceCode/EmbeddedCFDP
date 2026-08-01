/**
 * @file    cfdp.h
 * @brief   Umbrella header for the EmbeddedCFDP library
 *
 * Aggregates the public CFDP modules: common definitions, the PDU header and
 * File Data codec, the File Directive codecs, the LV/TLV parameter codecs and
 * the file checksum.
 * Implements a basic subset of CCSDS 727.0-B-5 (CCSDS File Delivery Protocol).
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#ifndef CFDP_H
#define CFDP_H

#include "cfdp_checksum.h"
#include "cfdp_common.h"
#include "cfdp_directive.h"
#include "cfdp_pdu.h"
#include "cfdp_tlv.h"

#endif /* CFDP_H */
