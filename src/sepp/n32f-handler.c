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

#include <ctype.h>

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

static bool is_hex_string(const char *str, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (!isxdigit((unsigned char)str[i]))
            return false;
    }

    return true;
}

static bool is_digit_string(const char *str, size_t len)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (!isdigit((unsigned char)str[i]))
            return false;
    }

    return true;
}

/*
 * "/namf-comm/v1/ue-contexts/{ueContextId}/transfer"
 * {ueContextId} must be a single, non-empty path segment.
 */
bool sepp_n32f_is_ue_context_transfer(const char *method, const char *path)
{
    const size_t prefix_len = strlen(SEPP_N32F_UE_CONTEXTS_PATH_PREFIX);
    const size_t suffix_len = strlen(SEPP_N32F_UE_CONTEXT_TRANSFER_SUFFIX);
    size_t len, i;

    if (!method || !path)
        return false;

    /* TS 29.518 defines POST; GET is also accepted */
    if (strcmp(method, OGS_SBI_HTTP_METHOD_GET) != 0 &&
        strcmp(method, OGS_SBI_HTTP_METHOD_POST) != 0)
        return false;

    len = strlen(path);
    if (len <= prefix_len + suffix_len)
        return false;

    if (strncmp(path, SEPP_N32F_UE_CONTEXTS_PATH_PREFIX, prefix_len) != 0 ||
        strcmp(path + len - suffix_len,
            SEPP_N32F_UE_CONTEXT_TRANSFER_SUFFIX) != 0)
        return false;

    for (i = prefix_len; i < len - suffix_len; i++) {
        if (path[i] == '/')
            return false;
    }

    return true;
}

/* The ONLY operations that are allowed to be relayed over N32-f */
bool sepp_n32f_is_forward_allowed(const char *method, const char *path)
{
    if (!method || !path)
        return false;

    if (strcmp(method, OGS_SBI_HTTP_METHOD_GET) == 0 &&
        strcmp(path, SEPP_N32F_NF_DISCOVERY_PATH) == 0)
        return true;

    return sepp_n32f_is_ue_context_transfer(method, path);
}

/*
 * Extract the GUAMI (PLMN + AMF-ID) from the 5G-GUTI based {ueContextId}
 * of the UEContextTransfer path :
 *
 *   5g-guti-<MCC><MNC (2 or 3 digits)><AMF-ID (6 hex)><5G-TMSI (8 hex)>
 */
bool sepp_n32f_parse_ue_context_id(const char *path, ogs_guami_t *guami)
{
    const size_t prefix_len = strlen(SEPP_N32F_UE_CONTEXTS_PATH_PREFIX);
    const size_t suffix_len = strlen(SEPP_N32F_UE_CONTEXT_TRANSFER_SUFFIX);
    const char guti_prefix[] = "5g-guti-";
    const size_t guti_prefix_len = sizeof(guti_prefix) - 1;
    const char *id = NULL, *p = NULL;
    char buf[8];
    size_t len, mnc_len;
    uint16_t mcc, mnc;
    ogs_amf_id_t amf_id;

    ogs_assert(guami);
    memset(guami, 0, sizeof(*guami));

    if (!path || sepp_n32f_is_ue_context_transfer(
                OGS_SBI_HTTP_METHOD_GET, path) == false)
        return false;

    id = path + prefix_len;
    len = strlen(path) - prefix_len - suffix_len;

    /* 8 + (5 or 6) + 6 + 8 */
    if (len != guti_prefix_len + 5 + 14 && len != guti_prefix_len + 6 + 14)
        return false;
    if (strncmp(id, guti_prefix, guti_prefix_len) != 0)
        return false;

    mnc_len = len - guti_prefix_len - 14 - 3;
    p = id + guti_prefix_len;
    if (!is_digit_string(p, 3 + mnc_len) || !is_hex_string(p + 3 + mnc_len, 14))
        return false;

    memcpy(buf, p, 3);
    buf[3] = 0;
    mcc = atoi(buf);
    memcpy(buf, p + 3, mnc_len);
    buf[mnc_len] = 0;
    mnc = atoi(buf);
    if (!mcc)
        return false;

    ogs_plmn_id_build(&guami->plmn_id, mcc, mnc, mnc_len);

    memcpy(buf, p + 3 + mnc_len, 6);
    buf[6] = 0;
    ogs_amf_id_from_string(&amf_id, buf);
    memcpy(&guami->amf_id, &amf_id, sizeof(amf_id));

    return true;
}

sepp_node_t *sepp_n32f_find_peer_by_ue_context_id(const char *path)
{
    ogs_guami_t guami;

    if (sepp_n32f_parse_ue_context_id(path, &guami) == false)
        return NULL;

    return sepp_node_find_by_plmn_id(
            ogs_plmn_id_mcc(&guami.plmn_id), ogs_plmn_id_mnc(&guami.plmn_id));
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

void sepp_n32f_handle_ue_context_transfer(
        ogs_pool_id_t stream_id, ogs_sbi_stream_t *stream,
        ogs_sbi_message_t *recvmsg, ogs_sbi_request_t *request)
{
    sepp_node_t *sepp_node = NULL;
    sepp_assoc_t *assoc = NULL;
    ogs_sbi_request_t *forward = NULL;
    ogs_guami_t guami;

    ogs_assert(stream);
    ogs_assert(recvmsg);
    ogs_assert(request);

    if (!request->h.uri || sepp_n32f_is_ue_context_transfer(
                request->h.method, request->h.uri) == false ||
        sepp_n32f_parse_ue_context_id(request->h.uri, &guami) == false) {
        ogs_error("Invalid UEContextTransfer [%s]",
                request->h.uri ? request->h.uri : "NULL");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_BAD_REQUEST, recvmsg,
                "Invalid UEContextTransfer", request->h.uri, NULL));
        return;
    }

    sepp_node = sepp_node_find_by_plmn_id(
            ogs_plmn_id_mcc(&guami.plmn_id), ogs_plmn_id_mnc(&guami.plmn_id));
    if (!sepp_node) {
        ogs_error("No SEPP Peer Node for the ueContextId PLMN");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_NOT_FOUND, recvmsg,
                "No SEPP Peer Node for the ueContextId PLMN", NULL, NULL));
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
            request->h.method, request->h.uri, NULL, request->http.content);
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
    memcpy(&assoc->target_plmn_id, &guami.plmn_id,
            sizeof(assoc->target_plmn_id));

    ogs_debug("[%s] N32-f UEContextTransfer forward [PLMN_ID:0x%x]",
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

/*
 * Peer SEPP : the UEContextTransfer is delivered to the local AMF.
 * The AMF is found by discovering it from the local NRF with the GUAMI
 * of the ueContextId; the request is parked in the association until the
 * discovery result arrives (see n32f_amf_discover_response_handler()).
 */
static void handle_forward_ue_context_transfer(
        ogs_pool_id_t stream_id, ogs_sbi_stream_t *stream,
        sepp_n32f_forward_t *forward)
{
    sepp_assoc_t *assoc = NULL;
    ogs_sbi_request_t *amf_request = NULL, *nrf_request = NULL;
    ogs_sbi_discovery_option_t *discovery_option = NULL;
    ogs_guami_t guami;

    ogs_assert(stream);
    ogs_assert(forward);

    if (sepp_n32f_parse_ue_context_id(forward->path, &guami) == false) {
        ogs_error("Invalid ueContextId [%s]", forward->path);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_BAD_REQUEST, NULL,
                "Invalid ueContextId", forward->path, NULL));
        return;
    }

    /* Only the local PLMN is served by this SEPP's AMF */
    if (ogs_sbi_plmn_id_in_vplmn(&guami.plmn_id) == true) {
        ogs_error("ueContextId PLMN is not served [PLMN_ID:0x%x]",
                ogs_plmn_id_hexdump(&guami.plmn_id));
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_NOT_FOUND, NULL,
                "ueContextId PLMN is not served", NULL, NULL));
        return;
    }

    amf_request = sepp_n32f_build_amf_request(
            forward->method, forward->path, forward->body);
    if (!amf_request) {
        ogs_error("sepp_n32f_build_amf_request() failed");
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL,
                "sepp_n32f_build_amf_request() failed", NULL, NULL));
        return;
    }

    discovery_option = ogs_sbi_discovery_option_new();
    ogs_assert(discovery_option);
    ogs_sbi_discovery_option_add_service_names(
            discovery_option, OpenAPI_service_name_namf_comm);
    ogs_sbi_discovery_option_set_guami(discovery_option, &guami);

    nrf_request = ogs_nnrf_disc_build_discover(
            OpenAPI_nf_type_AMF, OpenAPI_nf_type_SEPP, discovery_option);
    ogs_sbi_discovery_option_free(discovery_option);
    if (!nrf_request) {
        ogs_error("ogs_nnrf_disc_build_discover() failed");
        ogs_sbi_request_free(amf_request);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_INTERNAL_SERVER_ERROR, NULL,
                "ogs_nnrf_disc_build_discover() failed", NULL, NULL));
        return;
    }

    assoc = sepp_assoc_add(stream_id);
    if (!assoc) {
        ogs_error("sepp_assoc_add() failed");
        ogs_sbi_request_free(amf_request);
        ogs_sbi_request_free(nrf_request);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_SERVICE_UNAVAILABLE, NULL,
                "sepp_assoc_add() failed", NULL, NULL));
        return;
    }
    assoc->request = amf_request;

    if (sepp_n32f_send_amf_discover_request(assoc, nrf_request) == false) {
        ogs_error("sepp_n32f_send_amf_discover_request() failed");
        sepp_assoc_remove(assoc);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_SERVICE_UNAVAILABLE, NULL,
                "sepp_n32f_send_amf_discover_request() failed", NULL, NULL));
    }

    ogs_sbi_request_free(nrf_request);
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

    /* Only the NF-discovery and UEContextTransfer relays are supported */
    if (sepp_n32f_is_forward_allowed(forward.method, forward.path) == false) {
        ogs_error("Not supported N32-f forward [%s:%s]",
                forward.method, forward.path);
        ogs_assert(true ==
            ogs_sbi_server_send_error(stream,
                OGS_SBI_HTTP_STATUS_FORBIDDEN, NULL,
                "Not supported N32-f forward", forward.path, NULL));
        sepp_n32f_forward_free(&forward);
        return;
    }

    if (sepp_n32f_is_ue_context_transfer(forward.method, forward.path)) {
        handle_forward_ue_context_transfer(stream_id, stream, &forward);
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
