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

#include "n32f-build.h"

static void set_content(ogs_sbi_http_message_t *http, const char *body)
{
    ogs_assert(http);

    if (!body)
        return;

    http->content = ogs_strdup(body);
    ogs_assert(http->content);
    http->content_length = strlen(http->content);
    ogs_sbi_header_set(http->headers,
            OGS_SBI_CONTENT_TYPE, OGS_SBI_CONTENT_JSON_TYPE);
}

ogs_sbi_request_t *sepp_n32f_build_forward_request(
        const char *method, const char *path, ogs_hash_t *params,
        const char *body)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    ogs_hash_index_t *hi = NULL;
    cJSON *item = NULL, *jparams = NULL;
    char *content = NULL;

    ogs_assert(method);
    ogs_assert(path);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)OGS_SBI_HTTP_METHOD_POST;
    message.h.service.name = (char *)OGS_SBI_SERVICE_NAME_N32F_FORWARD;
    message.h.api.version = (char *)OGS_SBI_API_V1;
    message.h.resource.component[0] =
        (char *)OGS_SBI_RESOURCE_NAME_FORWARD;

    item = cJSON_CreateObject();
    ogs_assert(item);
    cJSON_AddStringToObject(item, "method", method);
    cJSON_AddStringToObject(item, "path", path);

    jparams = cJSON_AddObjectToObject(item, "params");
    ogs_assert(jparams);
    if (params) {
        for (hi = ogs_hash_first(params); hi; hi = ogs_hash_next(hi)) {
            const char *key = ogs_hash_this_key(hi);
            const char *val = ogs_hash_this_val(hi);
            if (key && val)
                cJSON_AddStringToObject(jparams, key, val);
        }
    }

    if (body)
        cJSON_AddStringToObject(item, "body", body);

    content = cJSON_PrintUnformatted(item);
    cJSON_Delete(item);
    if (!content) {
        ogs_error("cJSON_PrintUnformatted() failed");
        return NULL;
    }

    request = ogs_sbi_build_request(&message);
    if (!request) {
        ogs_error("ogs_sbi_build_request() failed");
        ogs_free(content);
        return NULL;
    }

    set_content(&request->http, content);
    ogs_free(content);

    return request;
}

ogs_sbi_response_t *sepp_n32f_build_forward_response(
        int status, const char *body)
{
    ogs_sbi_response_t *response = NULL;
    cJSON *item = NULL;
    char *content = NULL;

    item = cJSON_CreateObject();
    ogs_assert(item);
    cJSON_AddNumberToObject(item, "status", status);
    if (body)
        cJSON_AddStringToObject(item, "body", body);

    content = cJSON_PrintUnformatted(item);
    cJSON_Delete(item);
    if (!content) {
        ogs_error("cJSON_PrintUnformatted() failed");
        return NULL;
    }

    response = ogs_sbi_response_new();
    if (!response) {
        ogs_error("ogs_sbi_response_new() failed");
        ogs_free(content);
        return NULL;
    }

    response->status = OGS_SBI_HTTP_STATUS_OK;
    set_content(&response->http, content);
    ogs_free(content);

    return response;
}

ogs_sbi_request_t *sepp_n32f_build_nrf_request(
        const char *method, const char *path, ogs_hash_t *params,
        const char *body)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;
    ogs_hash_index_t *hi = NULL;

    ogs_assert(method);
    ogs_assert(path);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)method;
    message.h.uri = (char *)path;

    request = ogs_sbi_build_request(&message);
    if (!request) {
        ogs_error("ogs_sbi_build_request() failed");
        return NULL;
    }

    if (params) {
        for (hi = ogs_hash_first(params); hi; hi = ogs_hash_next(hi)) {
            const char *key = ogs_hash_this_key(hi);
            const char *val = ogs_hash_this_val(hi);
            /* Only the NF-discovery query parameters are relayed to the NRF */
            if (key && val && (
                !strcmp(key, "target-nf-type") ||
                !strcmp(key, "requester-nf-type") ||
                !strcmp(key, "service-names") ||
                !strcmp(key, "guami")))
                ogs_sbi_header_set(request->http.params, key, val);
        }
    }

    /* The relayed discovery is a GET; the body is ignored */
    (void)body;

    return request;
}

ogs_sbi_request_t *sepp_n32f_build_amf_request(
        const char *method, const char *path, const char *body)
{
    ogs_sbi_message_t message;
    ogs_sbi_request_t *request = NULL;

    ogs_assert(method);
    ogs_assert(path);

    memset(&message, 0, sizeof(message));
    message.h.method = (char *)method;
    message.h.uri = (char *)path;

    request = ogs_sbi_build_request(&message);
    if (!request) {
        ogs_error("ogs_sbi_build_request() failed");
        return NULL;
    }

    /* The UEContextTransfer request body is relayed as it is */
    set_content(&request->http, body);

    return request;
}

ogs_sbi_response_t *sepp_n32f_build_response(int status, const char *body)
{
    ogs_sbi_response_t *response = NULL;

    response = ogs_sbi_response_new();
    if (!response) {
        ogs_error("ogs_sbi_response_new() failed");
        return NULL;
    }

    response->status = status;
    set_content(&response->http, body);

    return response;
}
