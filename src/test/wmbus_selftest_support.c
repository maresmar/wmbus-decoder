#include "wmbus_selftest_i.h"

#include "../protocol/format/wmbus_record_formatter.h"
#include "../protocol/model/wmbus_application_record.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const uint8_t wmbus_apator_a[] = {
    0x6E, 0x44, 0x01, 0x06, 0x20, 0x20, 0x20, 0x20, 0x05, 0x07, 0x7A, 0x9A, 0x00, 0x60, 0x85, 0x2F,
    0x2F, 0x0F, 0x0A, 0x73, 0x43, 0x93, 0xCC, 0x00, 0x00, 0x43, 0x5B, 0x01, 0x83, 0x00, 0x1A, 0x54,
    0xE0, 0x6F, 0x63, 0x02, 0x91, 0x34, 0x25, 0x10, 0x03, 0x0F, 0x00, 0x00, 0x7B, 0x01, 0x3E, 0x0B,
    0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B,
    0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x65, 0x00, 0x00, 0x00, 0x3D, 0x00,
    0x00, 0x00, 0x3D, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x91,
    0x0C, 0xB0, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA6, 0x2B,
};

const uint8_t wmbus_apator_b[] = {
    0x4E, 0x44, 0x01, 0x06, 0x20, 0x20, 0x20, 0x21, 0x05, 0x07, 0x7A, 0x13, 0x00, 0x40, 0x85, 0x2F,
    0x2F, 0x0F, 0x6D, 0x4C, 0x38, 0x93, 0x00, 0x02, 0x00, 0x43, 0x84, 0x02, 0x10, 0x35, 0x1F, 0x04,
    0x00, 0x75, 0x01, 0x2C, 0x0B, 0x04, 0x00, 0x48, 0xD6, 0x03, 0x00, 0x3E, 0x63, 0x03, 0x00, 0xCD,
    0x2C, 0x03, 0x00, 0x1E, 0xF4, 0x02, 0x00, 0x0A, 0xCE, 0x02, 0x00, 0xA0, 0x98, 0xA3, 0x96, 0x03,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x19, 0x77,
};

const uint8_t wmbus_apator_c[] = {
    0x3E, 0x44, 0x01, 0x06, 0x14, 0x05, 0x41, 0x03, 0x05, 0x07, 0x7A, 0x19, 0x00, 0x30, 0x85, 0x2F,
    0x2F, 0x0F, 0x86, 0xB4, 0xB8, 0x95, 0x29, 0x02, 0x00, 0x40, 0xC6, 0xC1, 0xB4, 0xF0, 0xF3, 0xF3,
    0x41, 0x55, 0x59, 0x42, 0xFA, 0x70, 0x10, 0x00, 0xF0, 0x01, 0x01, 0x00, 0x00, 0x10, 0xBC, 0x78,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24, 0x83,
};

const WmBusSelftestApatorPublicVector wmbus_selftest_apator_public_vectors[] = {
    {.id = "20202020", .total_m3_x1000 = 3843U, .parsed_len = 111U, .parsed_fnv1a = 0x0DF2CB33U, .telegram = "|6E4401062020202005077A9A006085||2F2F0F0A734393CC0000435B0183001A54E06F630291342510||030F00007B013E0B00003E0B00003E0B00003E0B00003E0B00003E0B00003E0B0000650000003D0000003D0000003D00000000000000A0910CB003FFFFFFFFFFFFFFFFFFFFA62B|"},
    {.id = "21202020", .total_m3_x1000 = 270133U, .parsed_len = 79U, .parsed_fnv1a = 0xE21779D0U, .telegram = "|4E4401062020202105077A13004085||2F2F0F6D4C389300020043840210||351F040075012C0B040048D603003E630300CD2C03001EF402000ACE0200A098A39603FFFFFFFFFFFFFFFFFFFFFFFFFF1977|"},
    {.id = "22202020", .total_m3_x1000 = 64508U, .parsed_len = 79U, .parsed_fnv1a = 0xEFC6CFE2U, .telegram = "|4E4401062020202205077A4B004085||2F2F0FE566B99390000087C0B24B732679FF75350010||FCFB00004155594265086A0043B4017301DFF600006AE70000BFD5000051BC0000A0F56C2602FFFF1B1B|"},
    {.id = "23202020", .total_m3_x1000 = 77752U, .parsed_len = 79U, .parsed_fnv1a = 0xA4B7D3F0U, .telegram = "|4E4401062020202305077A9D004085||2F2F0F81902C9300000010||B82F010041555942BD2882004319027301BC2601005C180100CB0A0100DFF60000A0F56C2602FFFFFFFFFFFFFFFFFFFFFFFFFF5B7C|"},
    {.id = "24202020", .total_m3_x1000 = 82426U, .parsed_len = 79U, .parsed_fnv1a = 0x31AF3493U, .telegram = "|4E4401062020202405077A6C0040852F2F||0F73B3E19410000084E15381E553810101000010||FA41010041555942BF4E8A00433B027301AD380100BC2601005C180100CB0A0100A0F56C2602FFFFD0D7|"},
    {.id = "25202020", .total_m3_x1000 = 168577U, .parsed_len = 79U, .parsed_fnv1a = 0x59DE9773U, .telegram = "|4E4401062020202505077AEF0040852F2F||0F|071122|94|100200|43|6103|84|8B745953486C09100000|10|81920200|75|01F1800200E5640200534A02003431020080150200D9000200||A0|DC939703|FFFFA434|"},
    {.id = "26202020", .total_m3_x1000 = 17579U, .parsed_len = 111U, .parsed_fnv1a = 0x1B33ED9FU, .telegram = "|6E4401062020202605077AAC0060852F2F||0F|0C4442|94|1A0000|43|B502|83|000A549B4159029C290F|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000||A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF7823|"},
    {.id = "26202020", .total_m3_x1000 = 17579U, .parsed_len = 111U, .parsed_fnv1a = 0xC36D9B9EU, .telegram = "|6E4401062020202605077AAD0060852F2F||0F|0E4442|94|1A0000|43|B502|84|4265594C655901010000|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000||A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF6C1B|"},
    {.id = "26202020", .total_m3_x1000 = 17579U, .parsed_len = 111U, .parsed_fnv1a = 0xE2911617U, .telegram = "|6E4401062020202605077AAE0060852F2F||0F|0F4442|94|1A0000|43|B502|81|D87F57D87F5701010000|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000||A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF5F22|"},
    {.id = "27202020", .total_m3_x1000 = 15992U, .parsed_len = 111U, .parsed_fnv1a = 0xAB31B2A9U, .telegram = "|6E4401062020202705077A3D0060852F2F||0F|151794|94|0A0200|43|0403|81|D87F57D87F5701010000|10|783E0000||7B01223C00009137000098320000392D000010290000F02600004C2400003422000004220000CB21000017200000C51C0000||A0|9AD9A103|FFFFFFFFFFFFFFFFFFFF367E|"},
    {.id = "03410514", .total_m3_x1000 = 30908U, .parsed_len = 63U, .parsed_fnv1a = 0x271F9CBAU, .telegram = "|3E4401061405410305077A190030852F2F||0F|86B4B8|95|290200|40|C6C1|B4|F0F3F3|41|5559|42|FA701000|F0|01010000|10|BC780000||FFFFFFFFFFFFFFFFFFFFFF2483|"},
    {.id = "03820304", .total_m3_x1000 = 21922U, .parsed_len = 63U, .parsed_fnv1a = 0x7D63C355U, .telegram = "|3E4401060403820305077A090030852F2F0F9B5B229700000044C2DED310A25500007201C64A0000853C000094310000A0464B1904FFFFFFFFFFFFFFFF2ED6|"},
    {.id = "00148686", .total_m3_x1000 = 21930U, .parsed_len = 79U, .parsed_fnv1a = 0xA233FF58U, .telegram = "4E4401068686140005077A350040852F2F_0F005B599600000010AA55000041545A42850BD800437D037301C5500000564B00009E4600006A410000A01778EC03FFFFFFFFFFFFFFFFFFFFFFFFFFE393"},
    {.id = "04960212", .total_m3_x1000 = 14949U, .parsed_len = 95U, .parsed_fnv1a = 0xD91955C4U, .telegram = "|5E4401061202960405077A790050852F2F0F78A599983B0200435000805771165771160103000010653A00007919321E0000620000006200000000000000000000000000000000000000000000000000000000000000A0422C6004FFFF2FBF|"},
};

const char* wmbus_selftest_apator_old_style_b6 =
    "5A441486868614000507B6_"
    "0AFFFFF5450106F41BAD717A35004085C90AC6D97E3294827563E70F4CF00655FC796A76B87AD1D4A69D16F5EDD1084318F46559E43D2C60D2B1CE581D0CAC1BBC73A376B9D71F0D71C6C904B04DC30E";

const size_t wmbus_selftest_apator_old_style_b6_len = 91U;
const uint32_t wmbus_selftest_apator_old_style_b6_fnv1a = 0x1CD6AC05U;

const char* wmbus_selftest_apator_encrypted_mode5 =
    "6E4401068888888805077A85006085BC2630713819512EB4CD87FBA554FB43F67CF9654A68EE8E19"
    "4088160DF752E716238292E8AF1AC20986202EE561D743602466915E42F1105D9C6782A54504E4F09"
    "9E65A7656B930C73A30775122D2FDF074B5035CFAA7E0050BF32FAAE03A77";

const char* wmbus_selftest_apator_encrypted_mode5_gold =
    "6E4401065610990205077A3C006085FB01BBE79622F34A94D066D776406F31554FC96E46E974F5A0"
    "134E47D8E291316394056DFC945A813D7E0B09E2CC4BC58275D33BEEF1D6D834D83C59EF8551CDEFC"
    "0683BAE0DB6F536FB7F9B947A3AE6C2B9BB5B3D214D745D6A7B5F397E6BC9";

const char* wmbus_selftest_apator_encrypted_mode5_field_02991035 =
    "6E4401063510990205077AFB0060853C383827B03BFE30F3C8A65E56CB7FCD00F8EFB75C43B106D9"
    "B46489032587D00F6EB9967A74CB50BA23288E874F0AC183238E8BA7939C7092F9DEC3ED457DC9EBF"
    "3DB0D97778739645056CCCCB62DB8C8DA8B32F516D8BF0EB6F28AD74FFB5D";

const char* wmbus_selftest_apator_encrypted_mode5_corrupt =
    "6E4401068888888805077A85006085BD2630713819512EB4CD87FBA554FB43F67CF9654A68EE8E19"
    "4088160DF752E716238292E8AF1AC20986202EE561D743602466915E42F1105D9C6782A54504E4F09"
    "9E65A7656B930C73A30775122D2FDF074B5035CFAA7E0050BF32FAAE03A77";

const uint8_t wmbus_3of6_encode_lut[16] = {
    0x16, 0x0D, 0x0E, 0x0B, 0x1C, 0x19, 0x1A, 0x13, 0x2C, 0x25, 0x26, 0x23, 0x34, 0x31, 0x32, 0x29,
};

typedef struct {
    uint8_t seed[WMBUS_SELFTEST_BUF_MAX];
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX];
    uint8_t raw[WMBUS_SELFTEST_BUF_MAX];
} WmBusSelftestScratch;

static WmBusSelftestScratch wmbus_selftest_scratch;

bool wmbus_selftest_find_total_volume(const WmBusPacketRecord* record, uint32_t* total_m3_x1000) {
    return wmbus_application_find_total_volume(
        record->application.records, record->application.record_count, total_m3_x1000);
}

void wmbus_selftest_describe_first_record(
    const WmBusPacketRecord* packet,
    char* out,
    size_t out_size) {
    if(!out || out_size == 0U) return;
    out[0] = '\0';
    if(!packet) {
        snprintf(out, out_size, "packet=NULL");
        return;
    }
    if(packet->application.record_count == 0U) {
        snprintf(out, out_size, "records=0");
        return;
    }

    const WmBusApplicationRecord* rec = &packet->application.records[0];
    snprintf(
        out,
        out_size,
        "records=%u q=%u vt=%u scale=%d value=%llu storage=%u data_len=%u",
        (unsigned int)packet->application.record_count,
        (unsigned int)rec->quantity,
        (unsigned int)rec->value_type,
        (int)rec->scale10,
        (unsigned long long)rec->value_unsigned,
        (unsigned int)rec->storage_no,
        (unsigned int)rec->data_len);
}

void wmbus_selftest_set_detail(char* detail, size_t detail_len, const char* format, ...) {
    if(!detail || detail_len == 0U) return;
    va_list args;
    va_start(args, format);
    vsnprintf(detail, detail_len, format, args);
    va_end(args);
}

static int wmbus_selftest_hex_nibble(char c) {
    if(c >= '0' && c <= '9') return c - '0';
    if(c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if(c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

bool wmbus_selftest_hex_to_bytes(const char* hex, uint8_t* out, size_t out_max, size_t* out_len) {
    if(!hex || !out || !out_len) return false;

    int high = -1;
    size_t write = 0U;
    while(*hex) {
        int nibble = wmbus_selftest_hex_nibble(*hex++);
        if(nibble < 0) continue;
        if(high < 0) {
            high = nibble;
            continue;
        }
        if(write >= out_max) return false;
        out[write++] = (uint8_t)((high << 4) | nibble);
        high = -1;
    }

    if(high >= 0) return false;
    *out_len = write;
    return true;
}

bool wmbus_selftest_hex_to_format_b_frame(
    const char* hex,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t normalized_len = 0U;

    if(!hex || !out || !out_len) return false;
    if(!wmbus_selftest_hex_to_bytes(hex, normalized, sizeof(normalized), &normalized_len)) {
        return false;
    }

    return wmbus_frame_build_format_b(normalized, normalized_len, out, out_max, out_len);
}

uint32_t wmbus_selftest_fnv1a32(const uint8_t* data, size_t len) {
    uint32_t hash = 0x811C9DC5U;
    for(size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x01000193U;
    }
    return hash;
}

bool wmbus_selftest_write_report_line(File* file, const char* format, ...) {
    if(!file || !format) return false;

    char line[WMBUS_SELFTEST_LINE_MAX];
    va_list args;
    va_start(args, format);
    int len = vsnprintf(line, sizeof(line), format, args);
    va_end(args);
    if(len < 0) return false;

    size_t to_write = (size_t)len;
    if(to_write >= sizeof(line)) to_write = sizeof(line) - 1U;
    return storage_file_write(file, line, to_write) == to_write;
}

void wmbus_selftest_result_reset(WmBusSelftestResult* result) {
    memset(result, 0, sizeof(*result));
    result->best_offset = -1;
    memcpy(result->manufacturer, "???", WMBUS_MFG_STR_LEN);
    memcpy(result->id, "????????", WMBUS_ID_STR_LEN);
}

static void wmbus_selftest_set_identity(
    const uint8_t* frame,
    size_t frame_len,
    WmBusSelftestResult* result) {
    if(frame_len < 8U) return;

    uint16_t man = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    wmbus_frame_decode_mfg(man, result->manufacturer);
    wmbus_frame_format_id(&frame[4], result->id, NULL);
    result->has_identity = true;
}

static void wmbus_selftest_corrupt_byte(uint8_t* data, size_t len, size_t byte_pos) {
    if(!data || byte_pos == WMBUS_BIT_NONE) return;
    if(byte_pos == WMBUS_BYTE_LAST) {
        if(len == 0U) return;
        byte_pos = len - 1U;
    }
    if(byte_pos >= len) return;
    data[byte_pos] ^= 0x01U;
}

static void wmbus_selftest_apply_frame_corruption(
    const uint8_t* frame,
    size_t frame_len,
    size_t frame_corrupt_byte_pos) {
    if(frame == wmbus_selftest_scratch.frame) {
        wmbus_selftest_corrupt_byte(
            wmbus_selftest_scratch.frame, frame_len, frame_corrupt_byte_pos);
    } else if(frame == wmbus_selftest_scratch.seed) {
        wmbus_selftest_corrupt_byte(
            wmbus_selftest_scratch.seed, frame_len, frame_corrupt_byte_pos);
    }
}

bool wmbus_selftest_prepare_frame(
    const WmBusSelftestCase* test_case,
    const uint8_t** frame,
    size_t* frame_len) {
    furi_check(test_case);
    furi_check(test_case->vector);
    furi_check(frame);
    furi_check(frame_len);

    if(test_case->vector->len > sizeof(wmbus_selftest_scratch.seed)) return false;

    memcpy(wmbus_selftest_scratch.seed, test_case->vector->data, test_case->vector->len);
    wmbus_selftest_corrupt_byte(
        wmbus_selftest_scratch.seed, test_case->vector->len, test_case->seed_corrupt_byte_pos);

    if(!test_case->build_format_a) {
        *frame = wmbus_selftest_scratch.seed;
        *frame_len = test_case->vector->len;
        wmbus_selftest_apply_frame_corruption(*frame, *frame_len, test_case->frame_corrupt_byte_pos);
        return true;
    }

    if(!wmbus_frame_build_format_a(
           wmbus_selftest_scratch.seed,
           test_case->vector->len,
           wmbus_selftest_scratch.frame,
           sizeof(wmbus_selftest_scratch.frame),
           frame_len)) {
        return false;
    }

    *frame = wmbus_selftest_scratch.frame;
    wmbus_selftest_apply_frame_corruption(*frame, *frame_len, test_case->frame_corrupt_byte_pos);
    return true;
}

bool wmbus_selftest_generate_t_3of6_raw_with_offset(
    const uint8_t* decoded,
    size_t decoded_len,
    uint8_t expected_offset,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    size_t* out_bit_len) {
    if(!decoded || !out || !out_len || !out_bit_len || expected_offset > 7U) return false;

    size_t bit_pos = expected_offset;
    size_t total_bits = expected_offset + decoded_len * 12U;
    size_t raw_len = (total_bits + 7U) / 8U;
    if(raw_len > out_max) return false;

    memset(out, 0, raw_len);
    for(size_t i = 0; i < decoded_len; i++) {
        uint8_t nibbles[2] = {(uint8_t)(decoded[i] >> 4), (uint8_t)(decoded[i] & 0x0F)};
        for(size_t n = 0; n < 2U; n++) {
            uint8_t symbol = wmbus_3of6_encode_lut[nibbles[n]];
            for(uint8_t bit = 0; bit < 6U; bit++) {
                size_t pos = bit_pos + bit;
                if((symbol & (1U << (5U - bit))) == 0U) continue;
                out[pos / 8U] |= (uint8_t)(1U << (7U - (pos % 8U)));
            }
            bit_pos += 6U;
        }
    }

    *out_len = raw_len;
    *out_bit_len = total_bits;
    return true;
}

void wmbus_selftest_corrupt_t_raw_bit(uint8_t* raw, size_t raw_bit_len, size_t bit_pos) {
    if(!raw || bit_pos == WMBUS_BIT_NONE || bit_pos >= raw_bit_len) return;
    raw[bit_pos / 8U] ^= (uint8_t)(1U << (7U - (bit_pos % 8U)));
}

void wmbus_selftest_result_from_record(
    const WmBusPacketRecord* record,
    WmBusSelftestResult* result) {
    furi_check(record);
    wmbus_selftest_result_reset(result);

    result->decoded_ok = record->quality != WmBusPacketQualityAnyCapture;
    result->plausible = wmbus_packet_quality_meets(record->quality, WmBusPacketQualityHeaderOk);
    result->length_ok =
        wmbus_packet_quality_meets(record->quality, WmBusPacketQualityFrameComplete);
    result->crc_ok = wmbus_packet_quality_meets(record->quality, WmBusPacketQualityCrcOk);
    result->best_offset = record->best_offset;

    if(record->packet_len > 0U) {
        result->l_field = record->packet_bytes[0];
        result->has_l_field = true;
        result->computed_len = record->packet_len;
        result->has_computed_len = true;
    }

    if(record->packet_len >= 8U) {
        wmbus_selftest_set_identity(record->packet_bytes, record->packet_len, result);
    }
}

bool wmbus_selftest_process_capture_record(
    WmBusRxMode mode,
    const uint8_t* data,
    size_t data_len,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketRecord* record) {
    WmBusCaptureFrame capture = {0};

    if(!data || !record || data_len > sizeof(capture.data)) return false;

    memset(record, 0, sizeof(*record));
    memcpy(capture.data, data, data_len);
    capture.len = data_len;
    capture.rssi = -60;
    capture.mode = mode;

    return wmbus_packet_process_capture(&capture, key_store, record);
}

bool wmbus_selftest_run_capture(
    WmBusRxMode mode,
    const uint8_t* data,
    size_t data_len,
    const WmBusCryptoKeyStore* key_store,
    WmBusSelftestResult* result) {
    WmBusPacketRecord record = {0};

    if(!data || !result) return false;
    if(!wmbus_selftest_process_capture_record(mode, data, data_len, key_store, &record)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    wmbus_selftest_result_from_record(&record, result);
    return true;
}

const char* wmbus_selftest_format_l_field(const WmBusSelftestResult* result, char out[8]) {
    if(!result->has_l_field) {
        memcpy(out, "??", 3);
    } else {
        snprintf(out, 8, "%02X", result->l_field);
    }
    return out;
}

const char* wmbus_selftest_format_computed_len(const WmBusSelftestResult* result, char out[16]) {
    if(!result->has_computed_len) {
        memcpy(out, "??", 3);
    } else {
        snprintf(out, 16, "%u", (unsigned int)result->computed_len);
    }
    return out;
}
