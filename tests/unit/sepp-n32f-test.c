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

#include "sepp/n32f-build.h"
#include "sepp/n32f-handler.h"
#include "core/abts.h"

#define GUAMI_PARAM \
    "{\"plmnId\":{\"mcc\":\"202\",\"mnc\":\"02\"},\"amfId\":\"020040\"}"
#define NRF_BODY "{\"nfInstances\":[{\"nfInstanceId\":\"1\"}]}"

/* The forwarded method/path/params/body survive the N32-f wrapper */
static void n32f_forward_request_test(abts_case *tc, void *data)
{
    ogs_hash_t *params = ogs_hash_make();
    ogs_sbi_request_t *wrapped = NULL, *nrf = NULL;
    sepp_n32f_forward_t forward;
    char *v = NULL;
    ogs_time_t duration = ogs_local_conf()->time.message.duration;

    /* Required by ogs_sbi_build_request() */
    ogs_local_conf()->time.message.duration = ogs_time_from_sec(10);

    ogs_assert(params);
    ogs_sbi_header_set(params, "target-nf-type", "AMF");
    ogs_sbi_header_set(params, "service-names", "namf-comm");
    ogs_sbi_header_set(params, "guami", GUAMI_PARAM);

    wrapped = sepp_n32f_build_forward_request(
            OGS_SBI_HTTP_METHOD_GET, SEPP_N32F_NF_DISCOVERY_PATH, params,
            NULL);
    ABTS_PTR_NOTNULL(tc, wrapped);
    ABTS_STR_EQUAL(tc, OGS_SBI_HTTP_METHOD_POST, wrapped->h.method);
    ABTS_STR_EQUAL(tc, OGS_SBI_SERVICE_NAME_N32F_FORWARD,
            wrapped->h.service.name);
    ABTS_STR_EQUAL(tc, OGS_SBI_API_V1, wrapped->h.api.version);
    ABTS_STR_EQUAL(tc, OGS_SBI_RESOURCE_NAME_FORWARD,
            wrapped->h.resource.component[0]);

    ABTS_TRUE(tc, sepp_n32f_parse_forward_request(&forward, wrapped));
    ABTS_STR_EQUAL(tc, OGS_SBI_HTTP_METHOD_GET, forward.method);
    ABTS_STR_EQUAL(tc, SEPP_N32F_NF_DISCOVERY_PATH, forward.path);
    ABTS_INT_EQUAL(tc, 3, ogs_hash_count(forward.params));
    v = ogs_sbi_header_get(forward.params, "guami");
    ABTS_PTR_NOTNULL(tc, v);
    if (v) ABTS_STR_EQUAL(tc, GUAMI_PARAM, v);
    ABTS_PTR_EQUAL(tc, NULL, forward.body);

    /* The peer SEPP rebuilds the original request for its local NRF */
    nrf = sepp_n32f_build_nrf_request(
            forward.method, forward.path, forward.params, forward.body);
    ABTS_PTR_NOTNULL(tc, nrf);
    ABTS_STR_EQUAL(tc, OGS_SBI_HTTP_METHOD_GET, nrf->h.method);
    ABTS_STR_EQUAL(tc, SEPP_N32F_NF_DISCOVERY_PATH, nrf->h.uri);
    ABTS_INT_EQUAL(tc, 3, ogs_hash_count(nrf->http.params));
    v = ogs_sbi_header_get(nrf->http.params, "service-names");
    ABTS_PTR_NOTNULL(tc, v);
    if (v) ABTS_STR_EQUAL(tc, "namf-comm", v);

    sepp_n32f_forward_free(&forward);
    ogs_sbi_request_free(nrf);
    ogs_sbi_request_free(wrapped);
    ogs_sbi_http_hash_free(params);

    ogs_local_conf()->time.message.duration = duration;
}

/* The NRF response is relayed unchanged through the N32-f response */
static void n32f_forward_response_test(abts_case *tc, void *data)
{
    ogs_sbi_response_t *wrapped = NULL, *relayed = NULL;
    int status = 0;
    char *body = NULL;

    wrapped = sepp_n32f_build_forward_response(
            OGS_SBI_HTTP_STATUS_OK, NRF_BODY);
    ABTS_PTR_NOTNULL(tc, wrapped);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, wrapped->status);

    ABTS_TRUE(tc, sepp_n32f_parse_forward_response(wrapped, &status, &body));
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, status);
    ABTS_PTR_NOTNULL(tc, body);
    if (body) ABTS_STR_EQUAL(tc, NRF_BODY, body);

    relayed = sepp_n32f_build_response(status, body);
    ABTS_PTR_NOTNULL(tc, relayed);
    ABTS_INT_EQUAL(tc, OGS_SBI_HTTP_STATUS_OK, relayed->status);
    ABTS_STR_EQUAL(tc, NRF_BODY, relayed->http.content);

    ogs_free(body);
    ogs_sbi_response_free(relayed);
    ogs_sbi_response_free(wrapped);
}

static void n32f_invalid_content_test(abts_case *tc, void *data)
{
    ogs_sbi_request_t *request = ogs_sbi_request_new();
    sepp_n32f_forward_t forward;

    ogs_assert(request);
    ABTS_TRUE(tc, sepp_n32f_parse_forward_request(&forward, request) == false);

    request->http.content = ogs_strdup("{\"method\":\"GET\"}");
    ABTS_TRUE(tc, sepp_n32f_parse_forward_request(&forward, request) == false);

    ogs_sbi_request_free(request);
}

/* The peer SEPP is selected by the PLMN embedded in the GUAMI */
static void n32f_peer_routing_test(abts_case *tc, void *data)
{
    sepp_node_t peer;
    ogs_sbi_discovery_option_t *option = ogs_sbi_discovery_option_new();
    ogs_guami_t guami;

    ogs_assert(option);
    memset(&peer, 0, sizeof(peer));
    peer.receiver = (char *)"sepp.5gc.mnc002.mcc202.3gppnetwork.org";
    ogs_plmn_id_build(&peer.plmn_id[0], 202, 2, 2);
    peer.num_of_plmn_id = 1;
    ogs_list_add(&sepp_self()->peer_list, &peer);

    /* No GUAMI : not routable */
    ABTS_PTR_EQUAL(tc, NULL, sepp_n32f_find_peer_by_guami(NULL));
    ABTS_PTR_EQUAL(tc, NULL, sepp_n32f_find_peer_by_guami(option));

    /* GUAMI of an unknown PLMN : not routable */
    memset(&guami, 0, sizeof(guami));
    ogs_plmn_id_build(&guami.plmn_id, 315, 10, 2);
    ogs_sbi_discovery_option_set_guami(option, &guami);
    ABTS_PTR_EQUAL(tc, NULL, sepp_n32f_find_peer_by_guami(option));

    /* GUAMI of the peer PLMN */
    ogs_plmn_id_build(&guami.plmn_id, 202, 2, 2);
    ogs_sbi_discovery_option_free(option);
    option = ogs_sbi_discovery_option_new();
    ogs_assert(option);
    ogs_sbi_discovery_option_set_guami(option, &guami);
    ABTS_PTR_EQUAL(tc, &peer, sepp_n32f_find_peer_by_guami(option));

    ogs_list_remove(&sepp_self()->peer_list, &peer);
    ogs_sbi_discovery_option_free(option);
}

abts_suite *test_sepp_n32f(abts_suite *suite)
{
    suite = ADD_SUITE(suite)

    ogs_log_install_domain(&__sepp_log_domain, "sepp", OGS_LOG_ERROR);

    abts_run_test(suite, n32f_forward_request_test, NULL);
    abts_run_test(suite, n32f_forward_response_test, NULL);
    abts_run_test(suite, n32f_invalid_content_test, NULL);
    abts_run_test(suite, n32f_peer_routing_test, NULL);

    return suite;
}
