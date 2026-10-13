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

#ifndef SEPP_N32F_HANDLER_H
#define SEPP_N32F_HANDLER_H

#include "context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* See the NOTE in n32f-build.h: minimal, non fully 3GPP-compliant subset */

typedef struct sepp_n32f_forward_s {
    char *method;
    char *path;
    ogs_hash_t *params;
    char *body;
} sepp_n32f_forward_t;

bool sepp_n32f_parse_forward_request(
        sepp_n32f_forward_t *forward, ogs_sbi_request_t *request);
void sepp_n32f_forward_free(sepp_n32f_forward_t *forward);
bool sepp_n32f_parse_forward_response(
        ogs_sbi_response_t *response, int *status, char **body);

sepp_node_t *sepp_n32f_find_peer_by_guami(
        ogs_sbi_discovery_option_t *discovery_option);

void sepp_n32f_handle_nf_discover(
        ogs_pool_id_t stream_id, ogs_sbi_stream_t *stream,
        ogs_sbi_message_t *recvmsg, ogs_sbi_request_t *request);
void sepp_n32f_handle_forward(
        ogs_pool_id_t stream_id, ogs_sbi_stream_t *stream,
        ogs_sbi_request_t *request);

#ifdef __cplusplus
}
#endif

#endif /* SEPP_N32F_HANDLER_H */
