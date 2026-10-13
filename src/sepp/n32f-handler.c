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

#include "sbi-path.h"

#include "n32f-handler.h"

bool sepp_n32f_parse_forward_request(
        sepp_n32f_forward_t *forward, ogs_sbi_request_t *request)
{
    cJSON *item = NULL, *method = NULL, *path = NULL;
    cJSON *params = NULL, *param = NULL, *body = NULL;

    ogs_assert(forward);
    ogs_assert(request);

    memset(forward, 0, sizeof(*forward));

    if (!request->http.content) {
        ogs_error("No N32-f forward content");
        return false;
    }

    item = cJSON_Parse(request->http.content);
    if (!item) {
        ogs_error("JSON parse error [%s]", request->http.content);
        return false;
    }

    method = cJSON_GetObjectItemCaseSensitive(item, "method");
    path = cJSON_GetObjectItemCaseSensitive(item, "path");
    if (!cJSON_IsString(method) || !method->valuestring ||
        !cJSON_IsString(path) || !path->valuestring) {
        ogs_error("No method/path in N32-f forward");
        cJSON_Delete(item);
        return false;
    }

    forward->method = ogs_strdup(method->valuestring);
    ogs_assert(forward->method);
    forward->path = ogs_strdup(path->valuestring);
    ogs_assert(forward->path);

    forward->params = ogs_hash_make();
    ogs_assert(forward->params);
    params = cJSON_GetObjectItemCaseSensitive(item, "params");
    cJSON_ArrayForEach(param, params) {
        if (cJSON_IsString(param) && param->string && param->valuestring)
            ogs_sbi_header_set(forward->params,
                    param->string, param->valuestring);
    }

    body = cJSON_GetObjectItemCaseSensitive(item, "body");
    if (cJSON_IsString(body) && body->valuestring) {
        forward->body = ogs_strdup(body->valuestring);
        ogs_assert(forward->body);
    }

    cJSON_Delete(item);

    return true;
}

void sepp_n32f_forward_free(sepp_n32f_forward_t *forward)
{
    ogs_assert(forward);

    if (forward->method)
        ogs_free(forward->method);
    if (forward->path)
        ogs_free(forward->path);
    if (forward->params)
        ogs_sbi_http_hash_free(forward->params);
    if (forward->body)
        ogs_free(forward->body);

    memset(forward, 0, sizeof(*forward));
}

bool sepp_n32f_parse_forward_response(
        ogs_sbi_response_t *response, int *status, char **body)
{
    cJSON *item = NULL, *jstatus = NULL, *jbody = NULL;

    ogs_assert(response);
    ogs_assert(status);
    ogs_assert(body);

    *status = 0;
    *body = NULL;

    if (!response->http.content) {
        ogs_error("No N32-f forward content");
        return false;
    }

    item = cJSON_Parse(response->http.content);
    if (!item) {
        ogs_error("JSON parse error [%s]", response->http.content);
        return false;
    }

    jstatus = cJSON_GetObjectItemCaseSensitive(item, "status");
    if (!cJSON_IsNumber(jstatus)) {
        ogs_error("No status in N32-f forward response");
        cJSON_Delete(item);
        return false;
    }
    *status = jstatus->valueint;

    jbody = cJSON_GetObjectItemCaseSensitive(item, "body");
    if (cJSON_IsString(jbody) && jbody->valuestring) {
        *body = ogs_strdup(jbody->valuestring);
        ogs_assert(*body);
    }

    cJSON_Delete(item);

    return true;
}

sepp_node_t *sepp_n32f_find_peer_by_guami(
        ogs_sbi_discovery_option_t *discovery_option)
{
    if (!discovery_option || !discovery_option->guami_presence)
        return NULL;

    if (!ogs_plmn_id_mcc(&discovery_option->guami.plmn_id) ||
        !ogs_plmn_id_mnc(&discovery_option->guami.plmn_id))
        return NULL;

    return sepp_node_find_by_plmn_id(
            ogs_plmn_id_mcc(&discovery_option->guami.plmn_id),
            ogs_plmn_id_mnc(&discovery_option->guami.plmn_id));
}

void sepp_n32f_handle_nf_discover(
        ogs_pool_id_t stream_id, ogs_sbi_stream_t *stream,
        ogs_sbi_message_t *recvmsg, ogs_sbi_request_t *request)
{
    sepp_node_t *sepp_node = NULL;
    sepp_assoc_t *assoc = NULL;
    ogs_sbi_request_t *forward = NULL;

    ogs_assert(stream);
    ogs_assert(recvmsg);
    ogs_assert(request);

    sepp_node = sepp_n32f_find_peer_by_guami(recvmsg->param.discovery_option);
    if (!sepp_node) {
        ogs_error("No SEPP Peer Node for the GUAMI PLMN");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_NOT_FOUND, recvmsg,
                "No SEPP Peer Node for the GUAMI PLMN", NULL, NULL));
        return;
    }

    if (!OGS_FSM_CHECK(&sepp_node->sm, sepp_handshake_state_established)) {
        ogs_error("[%s] N32-c handshake is not established",
                sepp_node->receiver);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_SERVICE_UNAVAILABLE, recvmsg,
                "N32-c handshake is not established",
                sepp_node->receiver, NULL));
        return;
    }

    forward = sepp_n32f_build_forward_request(
            request->h.method, SEPP_N32F_NF_DISCOVERY_PATH,
            request->http.params, request->http.content);
    if (!forward) {
        ogs_error("sepp_n32f_build_forward_request() failed");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR, recvmsg,
                "sepp_n32f_build_forward_request() failed", NULL, NULL));
        return;
    }

    assoc = sepp_assoc_add(stream_id);
    if (!assoc) {
        ogs_error("sepp_assoc_add() failed");
        ogs_sbi_request_free(forward);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_SERVICE_UNAVAILABLE, recvmsg,
                "sepp_assoc_add() failed", NULL, NULL));
        return;
    }
    assoc->target_plmn_id_presence = true;
    memcpy(&assoc->target_plmn_id,
            &recvmsg->param.discovery_option->guami.plmn_id,
            sizeof(assoc->target_plmn_id));

    ogs_debug("[%s] N32-f NF-discover forward [PLMN_ID:0x%x]",
            sepp_node->receiver, ogs_plmn_id_hexdump(&assoc->target_plmn_id));

    if (sepp_n32f_send_forward_request(sepp_node, assoc, forward) == false) {
        ogs_error("[%s] sepp_n32f_send_forward_request() failed",
                sepp_node->receiver);
        sepp_assoc_remove(assoc);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_SERVICE_UNAVAILABLE, recvmsg,
                "sepp_n32f_send_forward_request() failed",
                sepp_node->receiver, NULL));
    }

    ogs_sbi_request_free(forward);
}

void sepp_n32f_handle_forward(
        ogs_pool_id_t stream_id, ogs_sbi_stream_t *stream,
        ogs_sbi_request_t *request)
{
    sepp_n32f_forward_t forward;
    sepp_assoc_t *assoc = NULL;
    ogs_sbi_request_t *nrf_request = NULL;

    ogs_assert(stream);
    ogs_assert(request);

    if (sepp_n32f_parse_forward_request(&forward, request) == false) {
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_BAD_REQUEST, NULL,
                "Invalid N32-f forward request", NULL, NULL));
        return;
    }

    /* Only the NF-discovery relay is supported */
    if (strcmp(forward.method, OGS_SBI_HTTP_METHOD_GET) != 0 ||
        strcmp(forward.path, SEPP_N32F_NF_DISCOVERY_PATH) != 0) {
        ogs_error("Not supported N32-f forward [%s:%s]",
                forward.method, forward.path);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_FORBIDDEN, NULL,
                "Not supported N32-f forward", forward.path, NULL));
        sepp_n32f_forward_free(&forward);
        return;
    }

    nrf_request = sepp_n32f_build_nrf_request(
            forward.method, forward.path, forward.params, forward.body);
    sepp_n32f_forward_free(&forward);
    if (!nrf_request) {
        ogs_error("sepp_n32f_build_nrf_request() failed");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL,
                "sepp_n32f_build_nrf_request() failed", NULL, NULL));
        return;
    }

    assoc = sepp_assoc_add(stream_id);
    if (!assoc) {
        ogs_error("sepp_assoc_add() failed");
        ogs_sbi_request_free(nrf_request);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_SERVICE_UNAVAILABLE, NULL,
                "sepp_assoc_add() failed", NULL, NULL));
        return;
    }

    if (sepp_n32f_send_nrf_request(assoc, nrf_request) == false) {
        ogs_error("sepp_n32f_send_nrf_request() failed");
        sepp_assoc_remove(assoc);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_SERVICE_UNAVAILABLE, NULL,
                "sepp_n32f_send_nrf_request() failed", NULL, NULL));
    }

    ogs_sbi_request_free(nrf_request);
}
