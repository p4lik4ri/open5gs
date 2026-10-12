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
 * Only "GET /nnrf-disc/v1/nf-instances" is relayed. The original HTTP
 * method, path, query parameters and body are wrapped in a small JSON
 * object and carried in "POST /n32f-forward/v1/forward":
 *
 *   Request : {"method":"GET","path":"/nnrf-disc/v1/nf-instances",
 *              "params":{"target-nf-type":"AMF",...},"body":"..."}
 *   Response: {"status":200,"body":"{\"nfInstances\":[...]}"}
 */
#define SEPP_N32F_NF_DISCOVERY_PATH "/nnrf-disc/v1/nf-instances"

ogs_sbi_request_t *sepp_n32f_build_forward_request(
        const char *method, const char *path, ogs_hash_t *params,
        const char *body);
ogs_sbi_response_t *sepp_n32f_build_forward_response(
        int status, const char *body);

ogs_sbi_request_t *sepp_n32f_build_nrf_request(
        const char *method, const char *path, ogs_hash_t *params,
        const char *body);
ogs_sbi_response_t *sepp_n32f_build_response(int status, const char *body);

#ifdef __cplusplus
}
#endif

#endif /* SEPP_N32F_BUILD_H */
