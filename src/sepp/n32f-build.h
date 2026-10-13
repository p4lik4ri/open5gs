/*
 * Copyright (C) 2023 by Sukchan Lee <acetcom@gmail.com>
 *
 * This file is part of Open5GS.
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef SEPP_N32F_BUILD_H
#define SEPP_N32F_BUILD_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * NOTE: This is a deliberately minimal, NON fully 3GPP-compliant N32-f subset
 * for lab/testing only. TS 29.573 N32fContextInfo, reroute-indication,
 * cIwK/PRINS protection and error handling are NOT implemented.
 *
 * Only the following operations are relayed (strict allowlist):
 *  - GET  /nnrf-disc/v1/nf-instances (NF discovery, to the peer NRF)
 *  - GET|POST /namf-comm/v1/ue-contexts/{ueContextId}/transfer
 *    (Namf_Communication_UEContextTransfer, to the peer AMF).
 *    TS 29.518 defines this operation as POST; GET is also accepted.
 *    {ueContextId} MUST be the 5G-GUTI form built by the AMF
 *    ("5g-guti-" + MCC/MNC (5 or 6 digits) + AMF-ID (6 hex) + 5G-TMSI
 *    (8 hex), TS 29.518 6.1.3.2.2). The peer SEPP is selected by the
 *    PLMN embedded in it; the peer SEPP finds its local AMF through the
 *    local NRF using the AMF-ID of the GUTI. A SUPI based {ueContextId}
 *    is not relayed.
 *
 * The original HTTP
 * method, path, query parameters and body are wrapped in a small JSON
 * object and carried in "POST /n32f-forward/v1/forward":
 *
 *   Request : {"method":"GET","path":"/nnrf-disc/v1/nf-instances",
 *              "params":{"target-nf-type":"AMF",...},"body":"..."}
 *   Response: {"status":200,"body":"{\"nfInstances\":[...]}"}
 */
#define SEPP_N32F_NF_DISCOVERY_PATH "/nnrf-disc/v1/nf-instances"
#define SEPP_N32F_UE_CONTEXTS_PATH_PREFIX "/namf-comm/v1/ue-contexts/"
#define SEPP_N32F_UE_CONTEXT_TRANSFER_SUFFIX "/transfer"

ogs_sbi_request_t *sepp_n32f_build_forward_request(
        const char *method, const char *path, ogs_hash_t *params,
        const char *body);
ogs_sbi_response_t *sepp_n32f_build_forward_response(
        int status, const char *body);

ogs_sbi_request_t *sepp_n32f_build_nrf_request(
        const char *method, const char *path, ogs_hash_t *params,
        const char *body);
ogs_sbi_request_t *sepp_n32f_build_amf_request(
        const char *method, const char *path, const char *body);
ogs_sbi_response_t *sepp_n32f_build_response(int status, const char *body);

#ifdef __cplusplus
}
#endif

#endif /* SEPP_N32F_BUILD_H */
