/**
 * @file    test_cfdp_tlv.c
 * @brief   Unit tests for the LV and TLV parameter codecs
 *
 * Exercises src/cfdp_tlv.c against CCSDS 727.0-B-5 Section 5.1.8, Section 5.1.9
 * and Section 5.4 with round-trip and known-vector checks.
 * See also: docs/727x0b5e1.pdf
 *
 * OpenSpaceCode — https://github.com/OpenSpaceCode
 */

#include "cfdp.h"
#include "cunit.h"
#include "test_runners.h"

static int test_lv_roundtrip(void)
{
    uint8_t buf[16];
    ASSERT_EQ_INT(4, cfdp_lv_serialize("abc", 3, buf, sizeof(buf)));
    ASSERT_EQ_INT(3, buf[0]);
    ASSERT_EQ_MEM("abc", &buf[1], 3);

    const char *value = NULL;
    uint8_t value_len = 0;
    ASSERT_EQ_INT(4, cfdp_lv_deserialize(buf, sizeof(buf), &value, &value_len));
    ASSERT_EQ_INT(3, value_len);
    ASSERT_EQ_MEM("abc", value, 3);

    /* An empty value is a bare length octet and decodes to a NULL pointer. */
    ASSERT_EQ_INT(1, cfdp_lv_serialize(NULL, 0, buf, sizeof(buf)));
    ASSERT_EQ_INT(1, cfdp_lv_deserialize(buf, sizeof(buf), &value, &value_len));
    ASSERT_TRUE(!value);
    ASSERT_EQ_INT(0, value_len);
    return 0;
}

static int test_lv_invalid_args(void)
{
    uint8_t buf[4] = {3, 'a', 'b', 'c'};
    const char *value = NULL;
    uint8_t value_len = 0;

    ASSERT_EQ_INT(0, cfdp_lv_serialize("abc", 3, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_lv_serialize(NULL, 3, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_lv_serialize("abc", 3, buf, 3));

    ASSERT_EQ_INT(0, cfdp_lv_deserialize(NULL, sizeof(buf), &value, &value_len));
    ASSERT_EQ_INT(0, cfdp_lv_deserialize(buf, sizeof(buf), NULL, &value_len));
    ASSERT_EQ_INT(0, cfdp_lv_deserialize(buf, sizeof(buf), &value, NULL));
    ASSERT_EQ_INT(0, cfdp_lv_deserialize(buf, 0, &value, &value_len));
    ASSERT_EQ_INT(0, cfdp_lv_deserialize(buf, 3, &value, &value_len));
    return 0;
}

static int test_tlv_roundtrip(void)
{
    const uint8_t message[] = {'c', 'f', 'd', 'p'};
    cfdp_tlv_t tlv = {0};
    tlv.type = (uint8_t)CFDP_TLV_MESSAGE_TO_USER;
    tlv.length = sizeof(message);
    tlv.value = message;

    uint8_t buf[16];
    ASSERT_EQ_INT(2 + sizeof(message), cfdp_tlv_serialize(&tlv, buf, sizeof(buf)));
    ASSERT_EQ_INT(0x02, buf[0]);
    ASSERT_EQ_INT(4, buf[1]);

    cfdp_tlv_t out = {0};
    ASSERT_EQ_INT(2 + sizeof(message), cfdp_tlv_deserialize(buf, sizeof(buf), &out));
    ASSERT_EQ_INT(CFDP_TLV_MESSAGE_TO_USER, out.type);
    ASSERT_EQ_INT(sizeof(message), out.length);
    ASSERT_EQ_MEM(message, out.value, sizeof(message));

    /* A zero-length TLV is legal and carries no value pointer. */
    cfdp_tlv_t empty = {0};
    empty.type = (uint8_t)CFDP_TLV_FLOW_LABEL;
    ASSERT_EQ_INT(2, cfdp_tlv_serialize(&empty, buf, sizeof(buf)));
    ASSERT_EQ_INT(2, cfdp_tlv_deserialize(buf, sizeof(buf), &out));
    ASSERT_TRUE(!out.value);
    return 0;
}

static int test_tlv_invalid_args(void)
{
    const uint8_t value[] = {0xAA};
    cfdp_tlv_t tlv = {0};
    tlv.type = (uint8_t)CFDP_TLV_FLOW_LABEL;
    tlv.length = 1;
    tlv.value = value;

    uint8_t buf[4] = {0x05, 0x01, 0xAA, 0x00};
    cfdp_tlv_t out = {0};

    ASSERT_EQ_INT(0, cfdp_tlv_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_tlv_serialize(&tlv, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_tlv_serialize(&tlv, buf, 2));

    cfdp_tlv_t no_value = tlv;
    no_value.value = NULL;
    ASSERT_EQ_INT(0, cfdp_tlv_serialize(&no_value, buf, sizeof(buf)));

    ASSERT_EQ_INT(0, cfdp_tlv_deserialize(NULL, sizeof(buf), &out));
    ASSERT_EQ_INT(0, cfdp_tlv_deserialize(buf, sizeof(buf), NULL));
    ASSERT_EQ_INT(0, cfdp_tlv_deserialize(buf, 1, &out));
    ASSERT_EQ_INT(0, cfdp_tlv_deserialize(buf, 2, &out));
    return 0;
}

static int test_entity_id_tlv_roundtrip(void)
{
    uint8_t buf[16];
    ASSERT_EQ_INT(3, cfdp_entity_id_tlv_serialize(0x2A, 1, buf, sizeof(buf)));
    ASSERT_EQ_INT(CFDP_TLV_ENTITY_ID, buf[0]);
    ASSERT_EQ_INT(1, buf[1]);
    ASSERT_EQ_INT(0x2A, buf[2]);

    uint64_t entity_id = 0;
    uint8_t id_len = 0;
    ASSERT_EQ_INT(3, cfdp_entity_id_tlv_deserialize(buf, sizeof(buf), &entity_id, &id_len));
    ASSERT_TRUE(entity_id == 0x2AULL);
    ASSERT_EQ_INT(1, id_len);

    ASSERT_EQ_INT(10, cfdp_entity_id_tlv_serialize(0x0102030405060708ULL, 8, buf, sizeof(buf)));
    ASSERT_EQ_INT(10, cfdp_entity_id_tlv_deserialize(buf, sizeof(buf), &entity_id, &id_len));
    ASSERT_TRUE(entity_id == 0x0102030405060708ULL);
    ASSERT_EQ_INT(8, id_len);
    return 0;
}

static int test_entity_id_tlv_invalid_args(void)
{
    uint8_t buf[16] = {0};
    uint64_t entity_id = 0;
    uint8_t id_len = 0;

    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_serialize(1, 1, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_serialize(1, 0, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_serialize(1, 9, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_serialize(1, 4, buf, 5));

    ASSERT_EQ_INT(3, cfdp_entity_id_tlv_serialize(7, 1, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_deserialize(buf, sizeof(buf), NULL, &id_len));
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_deserialize(buf, sizeof(buf), &entity_id, NULL));
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_deserialize(buf, 1, &entity_id, &id_len));

    /* Right shape, wrong type; then the right type with an illegal ID length. */
    buf[0] = (uint8_t)CFDP_TLV_FLOW_LABEL;
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_deserialize(buf, sizeof(buf), &entity_id, &id_len));
    buf[0] = (uint8_t)CFDP_TLV_ENTITY_ID;
    buf[1] = 0;
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_deserialize(buf, sizeof(buf), &entity_id, &id_len));
    buf[1] = 9;
    ASSERT_EQ_INT(0, cfdp_entity_id_tlv_deserialize(buf, sizeof(buf), &entity_id, &id_len));
    return 0;
}

static int test_fault_handler_tlv_roundtrip(void)
{
    uint8_t buf[8];
    size_t n = cfdp_fault_handler_tlv_serialize(CFDP_COND_FILESTORE_REJECTION,
                                                CFDP_HANDLER_IGNORE_ERROR,
                                                buf,
                                                sizeof(buf));
    ASSERT_EQ_INT(3, n);
    ASSERT_EQ_INT(CFDP_TLV_FAULT_HANDLER_OVERRIDE, buf[0]);
    ASSERT_EQ_INT(1, buf[1]);
    ASSERT_EQ_INT(0x43, buf[2]);

    cfdp_condition_code_t condition = CFDP_COND_NO_ERROR;
    cfdp_fault_handler_code_t handler = CFDP_HANDLER_RESERVED;
    ASSERT_EQ_INT(3, cfdp_fault_handler_tlv_deserialize(buf, n, &condition, &handler));
    ASSERT_EQ_INT(CFDP_COND_FILESTORE_REJECTION, condition);
    ASSERT_EQ_INT(CFDP_HANDLER_IGNORE_ERROR, handler);
    return 0;
}

static int test_fault_handler_tlv_invalid_args(void)
{
    uint8_t buf[8] = {0};
    cfdp_condition_code_t condition = CFDP_COND_NO_ERROR;
    cfdp_fault_handler_code_t handler = CFDP_HANDLER_RESERVED;

    ASSERT_EQ_INT(0,
                  cfdp_fault_handler_tlv_serialize(CFDP_COND_NO_ERROR,
                                                   CFDP_HANDLER_IGNORE_ERROR,
                                                   NULL,
                                                   sizeof(buf)));
    ASSERT_EQ_INT(
        0,
        cfdp_fault_handler_tlv_serialize(CFDP_COND_NO_ERROR, CFDP_HANDLER_IGNORE_ERROR, buf, 2));

    ASSERT_EQ_INT(3,
                  cfdp_fault_handler_tlv_serialize(CFDP_COND_NAK_LIMIT_REACHED,
                                                   CFDP_HANDLER_ABANDON_TRANSACTION,
                                                   buf,
                                                   sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_fault_handler_tlv_deserialize(buf, sizeof(buf), NULL, &handler));
    ASSERT_EQ_INT(0, cfdp_fault_handler_tlv_deserialize(buf, sizeof(buf), &condition, NULL));
    ASSERT_EQ_INT(0, cfdp_fault_handler_tlv_deserialize(buf, 1, &condition, &handler));

    buf[0] = (uint8_t)CFDP_TLV_FLOW_LABEL;
    ASSERT_EQ_INT(0, cfdp_fault_handler_tlv_deserialize(buf, sizeof(buf), &condition, &handler));
    buf[0] = (uint8_t)CFDP_TLV_FAULT_HANDLER_OVERRIDE;
    buf[1] = 2;
    ASSERT_EQ_INT(0, cfdp_fault_handler_tlv_deserialize(buf, sizeof(buf), &condition, &handler));
    return 0;
}

static int test_filestore_action_second_filename(void)
{
    ASSERT_TRUE(cfdp_filestore_action_has_second_filename(CFDP_FS_ACTION_RENAME_FILE));
    ASSERT_TRUE(cfdp_filestore_action_has_second_filename(CFDP_FS_ACTION_APPEND_FILE));
    ASSERT_TRUE(cfdp_filestore_action_has_second_filename(CFDP_FS_ACTION_REPLACE_FILE));

    ASSERT_TRUE(!cfdp_filestore_action_has_second_filename(CFDP_FS_ACTION_CREATE_FILE));
    ASSERT_TRUE(!cfdp_filestore_action_has_second_filename(CFDP_FS_ACTION_DELETE_FILE));
    ASSERT_TRUE(!cfdp_filestore_action_has_second_filename(CFDP_FS_ACTION_DENY_DIRECTORY));
    return 0;
}

static int test_filestore_request_roundtrip(void)
{
    cfdp_filestore_request_t req = {0};
    req.action_code = CFDP_FS_ACTION_DELETE_FILE;
    req.first_filename = "old.dat";
    req.first_filename_len = 7;

    uint8_t buf[32];
    size_t n = cfdp_filestore_request_tlv_serialize(&req, buf, sizeof(buf));
    ASSERT_EQ_INT(2 + 1 + 1 + 7, n);
    ASSERT_EQ_INT(CFDP_TLV_FILESTORE_REQUEST, buf[0]);
    ASSERT_EQ_INT(9, buf[1]);
    ASSERT_EQ_INT(0x10, buf[2]);

    cfdp_filestore_request_t out = {0};
    ASSERT_EQ_INT(n, cfdp_filestore_request_tlv_deserialize(buf, n, &out));
    ASSERT_EQ_INT(CFDP_FS_ACTION_DELETE_FILE, out.action_code);
    ASSERT_EQ_INT(7, out.first_filename_len);
    ASSERT_EQ_MEM("old.dat", out.first_filename, 7);
    ASSERT_TRUE(!out.second_filename);
    ASSERT_EQ_INT(0, out.second_filename_len);
    return 0;
}

static int test_filestore_request_second_filename(void)
{
    cfdp_filestore_request_t req = {0};
    req.action_code = CFDP_FS_ACTION_RENAME_FILE;
    req.first_filename = "a.dat";
    req.first_filename_len = 5;
    req.second_filename = "b.dat";
    req.second_filename_len = 5;

    uint8_t buf[32];
    size_t n = cfdp_filestore_request_tlv_serialize(&req, buf, sizeof(buf));
    ASSERT_EQ_INT(2 + 1 + 6 + 6, n);

    cfdp_filestore_request_t out = {0};
    ASSERT_EQ_INT(n, cfdp_filestore_request_tlv_deserialize(buf, n, &out));
    ASSERT_EQ_INT(CFDP_FS_ACTION_RENAME_FILE, out.action_code);
    ASSERT_EQ_MEM("a.dat", out.first_filename, 5);
    ASSERT_EQ_MEM("b.dat", out.second_filename, 5);
    return 0;
}

static int test_filestore_request_invalid_args(void)
{
    cfdp_filestore_request_t req = {0};
    req.action_code = CFDP_FS_ACTION_RENAME_FILE;
    req.first_filename = "a.dat";
    req.first_filename_len = 5;
    req.second_filename = "b.dat";
    req.second_filename_len = 5;

    uint8_t buf[32];
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_serialize(&req, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_serialize(&req, buf, 2));
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_serialize(&req, buf, 9));
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_serialize(&req, buf, 14));

    size_t n = cfdp_filestore_request_tlv_serialize(&req, buf, sizeof(buf));
    cfdp_filestore_request_t out = {0};
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_deserialize(buf, n, NULL));
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_deserialize(buf, 1, &out));

    buf[0] = (uint8_t)CFDP_TLV_FLOW_LABEL;
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_deserialize(buf, n, &out));

    /* Right type, but a value that cannot hold even the action octet. */
    const uint8_t empty_value[] = {CFDP_TLV_FILESTORE_REQUEST, 0x00};
    ASSERT_EQ_INT(0,
                  cfdp_filestore_request_tlv_deserialize(empty_value, sizeof(empty_value), &out));
    return 0;
}

static int test_filestore_request_malformed_value(void)
{
    cfdp_filestore_request_t out = {0};

    /* The name LV claims 5 octets but the TLV value holds 2. */
    const uint8_t truncated[] = {CFDP_TLV_FILESTORE_REQUEST, 0x03, 0x10, 0x05, 'a'};
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_deserialize(truncated, sizeof(truncated), &out));

    /* A well-formed name LV followed by an octet the format does not allow. */
    const uint8_t trailing[] = {CFDP_TLV_FILESTORE_REQUEST, 0x04, 0x10, 0x01, 'a', 0xFF};
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_deserialize(trailing, sizeof(trailing), &out));

    /* Rename carries two names, but only the first is present. */
    const uint8_t missing_second[] = {CFDP_TLV_FILESTORE_REQUEST, 0x03, 0x20, 0x01, 'a'};
    ASSERT_EQ_INT(
        0,
        cfdp_filestore_request_tlv_deserialize(missing_second, sizeof(missing_second), &out));
    return 0;
}

static int test_filestore_response_roundtrip(void)
{
    cfdp_filestore_response_t resp = {0};
    resp.action_code = CFDP_FS_ACTION_CREATE_FILE;
    resp.status_code = CFDP_FS_STATUS_NOT_PERFORMED;
    resp.first_filename = "new.dat";
    resp.first_filename_len = 7;
    resp.message = "no space";
    resp.message_len = 8;

    uint8_t buf[64];
    size_t n = cfdp_filestore_response_tlv_serialize(&resp, buf, sizeof(buf));
    ASSERT_EQ_INT(2 + 1 + 8 + 9, n);
    ASSERT_EQ_INT(CFDP_TLV_FILESTORE_RESPONSE, buf[0]);
    ASSERT_EQ_INT(0x0F, buf[2]);

    cfdp_filestore_response_t out = {0};
    ASSERT_EQ_INT(n, cfdp_filestore_response_tlv_deserialize(buf, n, &out));
    ASSERT_EQ_INT(CFDP_FS_ACTION_CREATE_FILE, out.action_code);
    ASSERT_EQ_INT(CFDP_FS_STATUS_NOT_PERFORMED, out.status_code);
    ASSERT_EQ_MEM("new.dat", out.first_filename, 7);
    ASSERT_EQ_MEM("no space", out.message, 8);
    ASSERT_TRUE(!out.second_filename);
    return 0;
}

static int test_filestore_response_second_filename(void)
{
    cfdp_filestore_response_t resp = {0};
    resp.action_code = CFDP_FS_ACTION_REPLACE_FILE;
    resp.status_code = CFDP_FS_STATUS_ERROR_2;
    resp.first_filename = "a.dat";
    resp.first_filename_len = 5;
    resp.second_filename = "b.dat";
    resp.second_filename_len = 5;

    uint8_t buf[64];
    size_t n = cfdp_filestore_response_tlv_serialize(&resp, buf, sizeof(buf));
    ASSERT_EQ_INT(2 + 1 + 6 + 6 + 1, n);

    cfdp_filestore_response_t out = {0};
    ASSERT_EQ_INT(n, cfdp_filestore_response_tlv_deserialize(buf, n, &out));
    ASSERT_EQ_INT(CFDP_FS_STATUS_ERROR_2, out.status_code);
    ASSERT_EQ_MEM("a.dat", out.first_filename, 5);
    ASSERT_EQ_MEM("b.dat", out.second_filename, 5);
    ASSERT_TRUE(!out.message);
    ASSERT_EQ_INT(0, out.message_len);
    return 0;
}

static int test_filestore_response_invalid_args(void)
{
    cfdp_filestore_response_t resp = {0};
    resp.action_code = CFDP_FS_ACTION_DELETE_FILE;
    resp.first_filename = "a.dat";
    resp.first_filename_len = 5;
    resp.message = "gone";
    resp.message_len = 4;

    uint8_t buf[64];
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_serialize(NULL, buf, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_serialize(&resp, NULL, sizeof(buf)));
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_serialize(&resp, buf, 2));
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_serialize(&resp, buf, 8));
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_serialize(&resp, buf, 12));

    size_t n = cfdp_filestore_response_tlv_serialize(&resp, buf, sizeof(buf));
    cfdp_filestore_response_t out = {0};
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_deserialize(buf, n, NULL));
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_deserialize(buf, 1, &out));

    buf[0] = (uint8_t)CFDP_TLV_FILESTORE_REQUEST;
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_deserialize(buf, n, &out));

    const uint8_t empty_value[] = {CFDP_TLV_FILESTORE_RESPONSE, 0x00};
    ASSERT_EQ_INT(0,
                  cfdp_filestore_response_tlv_deserialize(empty_value, sizeof(empty_value), &out));
    return 0;
}

static int test_filestore_response_malformed_value(void)
{
    cfdp_filestore_response_t out = {0};

    /* The name LV claims 5 octets but the TLV value holds 2. */
    const uint8_t bad_name[] = {CFDP_TLV_FILESTORE_RESPONSE, 0x03, 0x10, 0x05, 'a'};
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_deserialize(bad_name, sizeof(bad_name), &out));

    /* Name LV present, message LV missing. */
    const uint8_t no_message[] = {CFDP_TLV_FILESTORE_RESPONSE, 0x03, 0x10, 0x01, 'a'};
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_deserialize(no_message, sizeof(no_message), &out));

    /* Name and message LVs, then an octet the format does not allow. */
    const uint8_t trailing[] = {CFDP_TLV_FILESTORE_RESPONSE, 0x05, 0x10, 0x01, 'a', 0x00, 0xFF};
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_deserialize(trailing, sizeof(trailing), &out));
    return 0;
}

static int test_filestore_tlv_value_too_long(void)
{
    static char name[255];
    memset(name, 'x', sizeof(name));

    /* Two 255-octet names plus the action octet overflow the 8-bit TLV length. */
    cfdp_filestore_request_t req = {0};
    req.action_code = CFDP_FS_ACTION_RENAME_FILE;
    req.first_filename = name;
    req.first_filename_len = (uint8_t)sizeof(name);
    req.second_filename = name;
    req.second_filename_len = (uint8_t)sizeof(name);

    static uint8_t buf[600];
    ASSERT_EQ_INT(0, cfdp_filestore_request_tlv_serialize(&req, buf, sizeof(buf)));

    cfdp_filestore_response_t resp = {0};
    resp.action_code = CFDP_FS_ACTION_RENAME_FILE;
    resp.first_filename = name;
    resp.first_filename_len = (uint8_t)sizeof(name);
    resp.second_filename = name;
    resp.second_filename_len = (uint8_t)sizeof(name);
    ASSERT_EQ_INT(0, cfdp_filestore_response_tlv_serialize(&resp, buf, sizeof(buf)));
    return 0;
}

test_result_t test_cfdp_tlv_run_all(void)
{
    RUN_TEST(test_lv_roundtrip);
    RUN_TEST(test_lv_invalid_args);
    RUN_TEST(test_tlv_roundtrip);
    RUN_TEST(test_tlv_invalid_args);
    RUN_TEST(test_entity_id_tlv_roundtrip);
    RUN_TEST(test_entity_id_tlv_invalid_args);
    RUN_TEST(test_fault_handler_tlv_roundtrip);
    RUN_TEST(test_fault_handler_tlv_invalid_args);
    RUN_TEST(test_filestore_action_second_filename);
    RUN_TEST(test_filestore_request_roundtrip);
    RUN_TEST(test_filestore_request_second_filename);
    RUN_TEST(test_filestore_request_invalid_args);
    RUN_TEST(test_filestore_request_malformed_value);
    RUN_TEST(test_filestore_response_roundtrip);
    RUN_TEST(test_filestore_response_second_filename);
    RUN_TEST(test_filestore_response_invalid_args);
    RUN_TEST(test_filestore_response_malformed_value);
    RUN_TEST(test_filestore_tlv_value_too_long);

    /* cunit.h keeps its tally in file-local statics, so these counters cover
     * only the tests run above. */
    test_result_t result = {cunit_total_tests - cunit_overall_failures, cunit_total_tests};
    return result;
}
