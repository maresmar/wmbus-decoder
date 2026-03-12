#include "wmbus_selftest.h"

#include "../core/wmbus_config.h"
#include "../protocol/wmbus_packet.h"
#include "../protocol/parser/wmbus_parser.h"
#include "../protocol/parser/wmbus_parser_apator162.h"

#include <furi.h>
#include <storage/storage.h>

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define TAG                     "WmBusSelftest"
#define WMBUS_SELFTEST_LINE_MAX 256U

static const uint8_t wmbus_apator_a[] = {
    0x6E, 0x44, 0x01, 0x06, 0x20, 0x20, 0x20, 0x20, 0x05, 0x07, 0x7A, 0x9A, 0x00, 0x60, 0x85, 0x2F,
    0x2F, 0x0F, 0x0A, 0x73, 0x43, 0x93, 0xCC, 0x00, 0x00, 0x43, 0x5B, 0x01, 0x83, 0x00, 0x1A, 0x54,
    0xE0, 0x6F, 0x63, 0x02, 0x91, 0x34, 0x25, 0x10, 0x03, 0x0F, 0x00, 0x00, 0x7B, 0x01, 0x3E, 0x0B,
    0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B,
    0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x3E, 0x0B, 0x00, 0x00, 0x65, 0x00, 0x00, 0x00, 0x3D, 0x00,
    0x00, 0x00, 0x3D, 0x00, 0x00, 0x00, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xA0, 0x91,
    0x0C, 0xB0, 0x03, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xA6, 0x2B,
};

static const uint8_t wmbus_apator_b[] = {
    0x4E, 0x44, 0x01, 0x06, 0x20, 0x20, 0x20, 0x21, 0x05, 0x07, 0x7A, 0x13, 0x00, 0x40, 0x85, 0x2F,
    0x2F, 0x0F, 0x6D, 0x4C, 0x38, 0x93, 0x00, 0x02, 0x00, 0x43, 0x84, 0x02, 0x10, 0x35, 0x1F, 0x04,
    0x00, 0x75, 0x01, 0x2C, 0x0B, 0x04, 0x00, 0x48, 0xD6, 0x03, 0x00, 0x3E, 0x63, 0x03, 0x00, 0xCD,
    0x2C, 0x03, 0x00, 0x1E, 0xF4, 0x02, 0x00, 0x0A, 0xCE, 0x02, 0x00, 0xA0, 0x98, 0xA3, 0x96, 0x03,
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x19, 0x77,
};

static const uint8_t wmbus_apator_c[] = {
    0x3E, 0x44, 0x01, 0x06, 0x14, 0x05, 0x41, 0x03, 0x05, 0x07, 0x7A, 0x19, 0x00, 0x30, 0x85, 0x2F,
    0x2F, 0x0F, 0x86, 0xB4, 0xB8, 0x95, 0x29, 0x02, 0x00, 0x40, 0xC6, 0xC1, 0xB4, 0xF0, 0xF3, 0xF3,
    0x41, 0x55, 0x59, 0x42, 0xFA, 0x70, 0x10, 0x00, 0xF0, 0x01, 0x01, 0x00, 0x00, 0x10, 0xBC, 0x78,
    0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x24, 0x83,
};

// Clear-text Apator regression corpus copied from the vendored wmbusmeters
// sources in this repository:
// - wmbusmeters/src/driver_apator162.cc
// - wmbusmeters/simulations/simulation_apas.txt
typedef struct {
    const char* id;
    uint32_t total_m3_x1000;
    size_t parsed_len;
    uint32_t parsed_fnv1a;
    const char* telegram;
} WmBusSelftestApatorPublicVector;

static const WmBusSelftestApatorPublicVector wmbus_selftest_apator_public_vectors[] = {
    {
        .id = "20202020",
        .total_m3_x1000 = 3843U,
        .parsed_len = 111U,
        .parsed_fnv1a = 0x0DF2CB33U,
        .telegram =
            "|6E4401062020202005077A9A006085|"
            "|2F2F0F0A734393CC0000435B0183001A54E06F630291342510|"
            "|030F00007B013E0B00003E0B00003E0B00003E0B00003E0B00003E0B00003E0B0000650000003D0000003D0000003D00000000000000A0910CB003FFFFFFFFFFFFFFFFFFFFA62B|",
    },
    {
        .id = "21202020",
        .total_m3_x1000 = 270133U,
        .parsed_len = 79U,
        .parsed_fnv1a = 0xE21779D0U,
        .telegram =
            "|4E4401062020202105077A13004085|"
            "|2F2F0F6D4C389300020043840210|"
            "|351F040075012C0B040048D603003E630300CD2C03001EF402000ACE0200A098A39603FFFFFFFFFFFFFFFFFFFFFFFFFF1977|",
    },
    {
        .id = "22202020",
        .total_m3_x1000 = 64508U,
        .parsed_len = 79U,
        .parsed_fnv1a = 0xEFC6CFE2U,
        .telegram =
            "|4E4401062020202205077A4B004085|"
            "|2F2F0FE566B99390000087C0B24B732679FF75350010|"
            "|FCFB00004155594265086A0043B4017301DFF600006AE70000BFD5000051BC0000A0F56C2602FFFF1B1B|",
    },
    {
        .id = "23202020",
        .total_m3_x1000 = 77752U,
        .parsed_len = 79U,
        .parsed_fnv1a = 0xA4B7D3F0U,
        .telegram =
            "|4E4401062020202305077A9D004085|"
            "|2F2F0F81902C9300000010|"
            "|B82F010041555942BD2882004319027301BC2601005C180100CB0A0100DFF60000A0F56C2602FFFFFFFFFFFFFFFFFFFFFFFFFF5B7C|",
    },
    {
        .id = "24202020",
        .total_m3_x1000 = 82426U,
        .parsed_len = 79U,
        .parsed_fnv1a = 0x31AF3493U,
        .telegram =
            "|4E4401062020202405077A6C0040852F2F|"
            "|0F73B3E19410000084E15381E553810101000010|"
            "|FA41010041555942BF4E8A00433B027301AD380100BC2601005C180100CB0A0100A0F56C2602FFFFD0D7|",
    },
    {
        .id = "25202020",
        .total_m3_x1000 = 168577U,
        .parsed_len = 79U,
        .parsed_fnv1a = 0x59DE9773U,
        .telegram =
            "|4E4401062020202505077AEF0040852F2F|"
            "|0F|071122|94|100200|43|6103|84|8B745953486C09100000|10|81920200|75|01F1800200E5640200534A02003431020080150200D9000200|"
            "|A0|DC939703|FFFFA434|",
    },
    {
        .id = "26202020",
        .total_m3_x1000 = 17579U,
        .parsed_len = 111U,
        .parsed_fnv1a = 0x1B33ED9FU,
        .telegram =
            "|6E4401062020202605077AAC0060852F2F|"
            "|0F|0C4442|94|1A0000|43|B502|83|000A549B4159029C290F|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000|"
            "|A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF7823|",
    },
    {
        .id = "26202020",
        .total_m3_x1000 = 17579U,
        .parsed_len = 111U,
        .parsed_fnv1a = 0xC36D9B9EU,
        .telegram =
            "|6E4401062020202605077AAD0060852F2F|"
            "|0F|0E4442|94|1A0000|43|B502|84|4265594C655901010000|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000|"
            "|A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF6C1B|",
    },
    {
        .id = "26202020",
        .total_m3_x1000 = 17579U,
        .parsed_len = 111U,
        .parsed_fnv1a = 0xE2911617U,
        .telegram =
            "|6E4401062020202605077AAE0060852F2F|"
            "|0F|0F4442|94|1A0000|43|B502|81|D87F57D87F5701010000|10|AB440000|7B|012C440000BE3E00008838000072340000493000009B2C00001D2C0000822B00007428000010250000B7200000261C0000|"
            "|A0|A4D9A103|FFFFFFFFFFFFFFFFFFFF5F22|",
    },
    {
        .id = "27202020",
        .total_m3_x1000 = 15992U,
        .parsed_len = 111U,
        .parsed_fnv1a = 0xAB31B2A9U,
        .telegram =
            "|6E4401062020202705077A3D0060852F2F|"
            "|0F|151794|94|0A0200|43|0403|81|D87F57D87F5701010000|10|783E0000|"
            "|7B01223C00009137000098320000392D000010290000F02600004C2400003422000004220000CB21000017200000C51C0000|"
            "|A0|9AD9A103|FFFFFFFFFFFFFFFFFFFF367E|",
    },
    {
        .id = "03410514",
        .total_m3_x1000 = 30908U,
        .parsed_len = 63U,
        .parsed_fnv1a = 0x271F9CBAU,
        .telegram =
            "|3E4401061405410305077A190030852F2F|"
            "|0F|86B4B8|95|290200|40|C6C1|B4|F0F3F3|41|5559|42|FA701000|F0|01010000|10|BC780000|"
            "|FFFFFFFFFFFFFFFFFFFFFF2483|",
    },
    {
        .id = "11441111",
        .total_m3_x1000 = 528923U,
        .parsed_len = 61U,
        .parsed_fnv1a = 0x5A08CB3FU,
        .telegram =
            "|3C4401061111441105077A280030852F2F|"
            "|0F|064CB597180200|43|A0068300055A2D69610156BB0C101B1208007101A60AC5AA6DE6A5F0880E9ADD08393C|",
    },
    {
        .id = "03820304",
        .total_m3_x1000 = 21922U,
        .parsed_len = 63U,
        .parsed_fnv1a = 0x7D63C355U,
        .telegram =
            "|3E4401060403820305077A090030852F2F0F9B5B229700000044C2DED310A25500007201C64A0000853C000094310000A0464B1904FFFFFFFFFFFFFFFF2ED6|",
    },
    {
        .id = "00148686",
        .total_m3_x1000 = 21930U,
        .parsed_len = 79U,
        .parsed_fnv1a = 0xA233FF58U,
        .telegram =
            "4E4401068686140005077A350040852F2F_"
            "0F005B599600000010AA55000041545A42850BD800437D037301C5500000564B00009E4600006A410000A01778EC03FFFFFFFFFFFFFFFFFFFFFFFFFFE393",
    },
    {
        .id = "04040404",
        .total_m3_x1000 = 73519U,
        .parsed_len = 113U,
        .parsed_fnv1a = 0x37C552D5U,
        .telegram =
            "|704401060404040405077A0E0060852F2F_"
            "0F766DFB96010000430600808F67DB8F67DB01010000102F1F01007B01000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000000A05F5C1804FFFFFFFFFFFFFFFFFFFF26BCD649|",
    },
    {
        .id = "04960212",
        .total_m3_x1000 = 14949U,
        .parsed_len = 95U,
        .parsed_fnv1a = 0xD91955C4U,
        .telegram =
            "|5E4401061202960405077A790050852F2F0F78A599983B0200435000805771165771160103000010653A00007919321E0000620000006200000000000000000000000000000000000000000000000000000000000000A0422C6004FFFF2FBF|",
    },
};

static const char* wmbus_selftest_apator_old_style_b6 =
    "5A441486868614000507B6_"
    "0AFFFFF5450106F41BAD717A35004085C90AC6D97E3294827563E70F4CF00655FC796A76B87AD1D4A69D16F5EDD1084318F46559E43D2C60D2B1CE581D0CAC1BBC73A376B9D71F0D71C6C904B04DC30E";

static const size_t wmbus_selftest_apator_old_style_b6_len = 91U;
static const uint32_t wmbus_selftest_apator_old_style_b6_fnv1a = 0x1CD6AC05U;

static const char* wmbus_selftest_apator_encrypted_mode5 =
    "6E4401068888888805077A85006085BC2630713819512EB4CD87FBA554FB43F67CF9654A68EE8E19"
    "4088160DF752E716238292E8AF1AC20986202EE561D743602466915E42F1105D9C6782A54504E4F09"
    "9E65A7656B930C73A30775122D2FDF074B5035CFAA7E0050BF32FAAE03A77";

// Encrypted Apator sample provided by the user on 2026-03-12 and verified
// against the current parser as a zero-key mode-5 telegram with total 345.654 m3.
static const char* wmbus_selftest_apator_encrypted_mode5_gold =
    "6E4401065610990205077A3C006085FB01BBE79622F34A94D066D776406F31554FC96E46E974F5A0"
    "134E47D8E291316394056DFC945A813D7E0B09E2CC4BC58275D33BEEF1D6D834D83C59EF8551CDEFC"
    "0683BAE0DB6F536FB7F9B947A3AE6C2B9BB5B3D214D745D6A7B5F397E6BC9";

// Additional field sample captured from meter 02991035 on 2026-03-12 and
// verified against the current parser as a zero-key mode-5 telegram with
// total 200.257 m3.
static const char* wmbus_selftest_apator_encrypted_mode5_field_02991035 =
    "6E4401063510990205077AFB0060853C383827B03BFE30F3C8A65E56CB7FCD00F8EFB75C43B106D9"
    "B46489032587D00F6EB9967A74CB50BA23288E874F0AC183238E8BA7939C7092F9DEC3ED457DC9EBF"
    "3DB0D97778739645056CCCCB62DB8C8DA8B32F516D8BF0EB6F28AD74FFB5D";

static const char* wmbus_selftest_apator_encrypted_mode5_corrupt =
    "6E4401068888888805077A85006085BD2630713819512EB4CD87FBA554FB43F67CF9654A68EE8E19"
    "4088160DF752E716238292E8AF1AC20986202EE561D743602466915E42F1105D9C6782A54504E4F09"
    "9E65A7656B930C73A30775122D2FDF074B5035CFAA7E0050BF32FAAE03A77";

typedef struct {
    const char* telegram;
    uint32_t total_m3_x1000;
    const char* id;
} WmBusSelftestApatorFieldVector;

static const WmBusTestVector wmbus_vector_c_apator_a_ok = {
    .name = "c_apator_a_ok",
    .data = wmbus_apator_a,
    .len = sizeof(wmbus_apator_a),
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_c_apator_b_ok = {
    .name = "c_apator_b_ok",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_c_apator_c_ok = {
    .name = "c_apator_c_ok",
    .data = wmbus_apator_c,
    .len = sizeof(wmbus_apator_c),
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_c_apator_b_crc_bad = {
    .name = "c_apator_b_crc_bad",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = false,
    .expect_plausible = true,
    .expect_crc_ok = false,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_c_apator_b_bad_c_field = {
    .name = "c_apator_b_bad_c_field",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = false,
    .expect_plausible = false,
    .expect_crc_ok = false,
    .expected_offset = 0,
};

static const WmBusTestVector wmbus_vector_t_apator_a_off1_ok = {
    .name = "t_apator_a_off1_ok",
    .data = wmbus_apator_a,
    .len = sizeof(wmbus_apator_a),
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 1,
};

static const WmBusTestVector wmbus_vector_t_apator_b_off3_ok = {
    .name = "t_apator_b_off3_ok",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 3,
};

static const WmBusTestVector wmbus_vector_t_apator_c_off7_ok = {
    .name = "t_apator_c_off7_ok",
    .data = wmbus_apator_c,
    .len = sizeof(wmbus_apator_c),
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = true,
    .expected_offset = 7,
};

static const WmBusTestVector wmbus_vector_t_apator_b_off5_crc_bad = {
    .name = "t_apator_b_off5_crc_bad",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = true,
    .expect_plausible = true,
    .expect_crc_ok = false,
    .expected_offset = 5,
};

static const WmBusTestVector wmbus_vector_t_apator_b_off2_bad_symbol = {
    .name = "t_apator_b_off2_bad_symbol",
    .data = wmbus_apator_b,
    .len = sizeof(wmbus_apator_b),
    .is_t_raw = true,
    .expect_plausible = false,
    .expect_crc_ok = false,
    .expected_offset = 2,
};

static const WmBusSelftestCase wmbus_selftest_cases[] = {
    {
        .name = "c_apator_a_ok",
        .vector = &wmbus_vector_c_apator_a_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "c_apator_b_ok",
        .vector = &wmbus_vector_c_apator_b_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "c_apator_c_ok",
        .vector = &wmbus_vector_c_apator_c_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "c_apator_b_crc_bad",
        .vector = &wmbus_vector_c_apator_b_crc_bad,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BYTE_LAST,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "c_apator_b_bad_c_field",
        .vector = &wmbus_vector_c_apator_b_bad_c_field,
        .build_format_a = true,
        .seed_corrupt_byte_pos = 1U,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_a_off1_ok",
        .vector = &wmbus_vector_t_apator_a_off1_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_b_off3_ok",
        .vector = &wmbus_vector_t_apator_b_off3_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_c_off7_ok",
        .vector = &wmbus_vector_t_apator_c_off7_ok,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_b_off5_crc_bad",
        .vector = &wmbus_vector_t_apator_b_off5_crc_bad,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BYTE_LAST,
        .raw_corrupt_bit_pos = WMBUS_BIT_NONE,
    },
    {
        .name = "t_apator_b_off2_bad_symbol",
        .vector = &wmbus_vector_t_apator_b_off2_bad_symbol,
        .build_format_a = true,
        .seed_corrupt_byte_pos = WMBUS_BIT_NONE,
        .frame_corrupt_byte_pos = WMBUS_BIT_NONE,
        .raw_corrupt_bit_pos = wmbus_vector_t_apator_b_off2_bad_symbol.expected_offset,
    },
};

static const uint8_t wmbus_3of6_encode_lut[16] = {
    0x16,
    0x0D,
    0x0E,
    0x0B,
    0x1C,
    0x19,
    0x1A,
    0x13,
    0x2C,
    0x25,
    0x26,
    0x23,
    0x34,
    0x31,
    0x32,
    0x29,
};

typedef struct {
    uint8_t seed[WMBUS_SELFTEST_BUF_MAX];
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX];
    uint8_t raw[WMBUS_SELFTEST_BUF_MAX];
    uint8_t shifted[WMBUS_SELFTEST_BUF_MAX];
    uint8_t decoded[WMBUS_SELFTEST_BUF_MAX];
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX];
} WmBusSelftestScratch;

static WmBusSelftestScratch wmbus_selftest_scratch;

static void wmbus_selftest_set_detail(char* detail, size_t detail_len, const char* format, ...) {
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

static bool
    wmbus_selftest_hex_to_bytes(const char* hex, uint8_t* out, size_t out_max, size_t* out_len) {
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

static uint32_t wmbus_selftest_fnv1a32(const uint8_t* data, size_t len) {
    uint32_t hash = 0x811C9DC5U;
    for(size_t i = 0; i < len; i++) {
        hash ^= data[i];
        hash *= 0x01000193U;
    }
    return hash;
}

static bool wmbus_selftest_write_report_line(File* file, const char* format, ...) {
    if(!file || !format) return false;

    char line[WMBUS_SELFTEST_LINE_MAX];

    va_list args;
    va_start(args, format);
    int len = vsnprintf(line, sizeof(line), format, args);
    va_end(args);

    if(len < 0) return false;

    size_t to_write = (size_t)len;
    if(to_write >= sizeof(line)) {
        to_write = sizeof(line) - 1U;
    }

    return storage_file_write(file, line, to_write) == to_write;
}

static void wmbus_selftest_result_reset(WmBusSelftestResult* result) {
    memset(result, 0, sizeof(*result));
    result->best_offset = -1;
    memcpy(result->manufacturer, "???", WMBUS_MFG_STR_LEN);
    memcpy(result->id, "????????", WMBUS_ID_STR_LEN);
}

static void wmbus_selftest_set_identity(
    const uint8_t* frame,
    size_t frame_len,
    WmBusSelftestResult* result) {
    if(frame_len < 8) return;

    uint16_t man = (uint16_t)frame[2] | ((uint16_t)frame[3] << 8);
    wmbus_frame_decode_mfg(man, result->manufacturer);
    wmbus_frame_format_id(&frame[4], result->id, NULL);
    result->has_identity = true;
}

static void wmbus_selftest_corrupt_byte(uint8_t* data, size_t len, size_t byte_pos) {
    if(!data || byte_pos == WMBUS_BIT_NONE) return;
    if(byte_pos == WMBUS_BYTE_LAST) {
        if(len == 0) return;
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

static bool wmbus_selftest_prepare_frame(
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
        wmbus_selftest_apply_frame_corruption(
            *frame, *frame_len, test_case->frame_corrupt_byte_pos);
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

static bool generate_t_3of6_raw_with_offset(
    const uint8_t* decoded,
    size_t decoded_len,
    uint8_t expected_offset,
    uint8_t* out,
    size_t out_max,
    size_t* out_len,
    size_t* out_bit_len) {
    if(!decoded || !out || !out_len || !out_bit_len || expected_offset > 7) return false;

    size_t bit_pos = expected_offset;
    size_t total_bits = expected_offset + decoded_len * 12U;
    size_t raw_len = (total_bits + 7U) / 8U;
    if(raw_len > out_max) return false;

    memset(out, 0, raw_len);
    for(size_t i = 0; i < decoded_len; i++) {
        uint8_t nibbles[2] = {(uint8_t)(decoded[i] >> 4), (uint8_t)(decoded[i] & 0x0F)};
        for(size_t n = 0; n < 2; n++) {
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

static void corrupt_t_raw_bit(uint8_t* raw, size_t raw_bit_len, size_t bit_pos) {
    if(!raw || bit_pos == WMBUS_BIT_NONE) return;
    if(bit_pos >= raw_bit_len) return;
    raw[bit_pos / 8U] ^= (uint8_t)(1U << (7U - (bit_pos % 8U)));
}

static void wmbus_selftest_result_from_record(
    const WmBusPacketRecord* record,
    WmBusSelftestResult* result) {
    furi_check(record);
    wmbus_selftest_result_reset(result);

    result->decoded_ok = record->decoded_ok;
    result->plausible = record->plausible;
    result->length_ok = record->length_ok;
    result->crc_ok = record->crc_ok;
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

static bool wmbus_selftest_process_capture_record(
    WmBusRxMode mode,
    const uint8_t* data,
    size_t data_len,
    const WmBusKeyring* keyring,
    WmBusPacketRecord* record) {
    WmBusCaptureFrame capture = {0};

    if(!data || !record || data_len > sizeof(capture.data)) {
        return false;
    }

    memset(record, 0, sizeof(*record));
    memcpy(capture.data, data, data_len);
    capture.len = data_len;
    capture.raw_len = data_len;
    capture.rssi = -60;
    capture.mode = mode;

    return wmbus_packet_process_capture(&capture, keyring, record);
}

static bool wmbus_selftest_run_capture(
    WmBusRxMode mode,
    const uint8_t* data,
    size_t data_len,
    const WmBusKeyring* keyring,
    WmBusSelftestResult* result) {
    WmBusPacketRecord record = {0};

    if(!data || !result) {
        return false;
    }

    if(!wmbus_selftest_process_capture_record(mode, data, data_len, keyring, &record)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    wmbus_selftest_result_from_record(&record, result);
    return true;
}

static bool wmbus_selftest_offset_match(
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result) {
    return (result->best_offset >= 0) &&
           ((uint8_t)result->best_offset == test_case->vector->expected_offset);
}

static const char* wmbus_selftest_format_l_field(const WmBusSelftestResult* result, char out[8]) {
    if(!result->has_l_field) {
        memcpy(out, "??", 3);
    } else {
        snprintf(out, 8, "%02X", result->l_field);
    }
    return out;
}

static const char*
    wmbus_selftest_format_computed_len(const WmBusSelftestResult* result, char out[16]) {
    if(!result->has_computed_len) {
        memcpy(out, "??", 3);
    } else {
        snprintf(out, 16, "%u", (unsigned int)result->computed_len);
    }
    return out;
}

static bool
    wmbus_selftest_run_c_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result) {
    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    if(!wmbus_selftest_prepare_frame(test_case, &frame, &frame_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    if(!wmbus_selftest_run_capture(WmBusRxModeC, frame, frame_len, NULL, result)) {
        return false;
    }

    return (result->plausible == test_case->vector->expect_plausible) &&
           (result->crc_ok == test_case->vector->expect_crc_ok);
}

static bool
    wmbus_selftest_run_t_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result) {
    const uint8_t* frame = NULL;
    size_t frame_len = 0;
    if(!wmbus_selftest_prepare_frame(test_case, &frame, &frame_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    size_t raw_len = 0;
    size_t raw_bit_len = 0;
    if(!generate_t_3of6_raw_with_offset(
           frame,
           frame_len,
           test_case->vector->expected_offset,
           wmbus_selftest_scratch.raw,
           sizeof(wmbus_selftest_scratch.raw),
           &raw_len,
           &raw_bit_len)) {
        wmbus_selftest_result_reset(result);
        return false;
    }

    corrupt_t_raw_bit(wmbus_selftest_scratch.raw, raw_bit_len, test_case->raw_corrupt_bit_pos);
    if(!wmbus_selftest_run_capture(
           WmBusRxModeT, wmbus_selftest_scratch.raw, raw_len, NULL, result)) {
        return false;
    }

    bool offset_match = wmbus_selftest_offset_match(test_case, result);
    return (result->plausible == test_case->vector->expect_plausible) &&
           (result->crc_ok == test_case->vector->expect_crc_ok) &&
           (test_case->vector->expect_plausible ? offset_match : !offset_match);
}

size_t wmbus_selftest_get_case_count(void) {
    return COUNT_OF(wmbus_selftest_cases);
}

const WmBusSelftestCase* wmbus_selftest_get_case(size_t index) {
    if(index >= wmbus_selftest_get_case_count()) return NULL;
    return &wmbus_selftest_cases[index];
}

bool wmbus_selftest_run_case(const WmBusSelftestCase* test_case, WmBusSelftestResult* result) {
    WmBusSelftestResult local_result = {0};

    if(!test_case || !test_case->vector) return false;
    if(!result) result = &local_result;

    if(test_case->vector->is_t_raw) {
        return wmbus_selftest_run_t_case(test_case, result);
    } else {
        return wmbus_selftest_run_c_case(test_case, result);
    }
}

void wmbus_selftest_log_case_result(
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result,
    bool pass) {
    furi_check(test_case);
    furi_check(test_case->vector);
    furi_check(result);

    char l_field[8] = {0};
    char computed_len[16] = {0};
    wmbus_selftest_format_l_field(result, l_field);
    wmbus_selftest_format_computed_len(result, computed_len);

    if(test_case->vector->is_t_raw) {
        bool offset_match = wmbus_selftest_offset_match(test_case, result);
        if(pass) {
            FURI_LOG_I(
                TAG,
                "%s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s best_offset=%d expected_offset=%u expected_found=%s",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id,
                result->best_offset,
                test_case->vector->expected_offset,
                offset_match ? "YES" : "NO");
        } else {
            FURI_LOG_W(
                TAG,
                "%s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s best_offset=%d expected_offset=%u expected_found=%s",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id,
                result->best_offset,
                test_case->vector->expected_offset,
                offset_match ? "YES" : "NO");
        }
    } else {
        if(pass) {
            FURI_LOG_I(
                TAG,
                "%s mode=C plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id);
        } else {
            FURI_LOG_W(
                TAG,
                "%s mode=C plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s",
                test_case->name,
                result->plausible ? "YES" : "NO",
                l_field,
                computed_len,
                result->crc_ok ? "YES" : "NO",
                result->manufacturer,
                result->id);
        }
    }
}

static void wmbus_selftest_report_case_result(
    File* file,
    const WmBusSelftestCase* test_case,
    const WmBusSelftestResult* result,
    bool pass) {
    if(!file || !test_case || !test_case->vector || !result) return;

    char l_field[8] = {0};
    char computed_len[16] = {0};
    wmbus_selftest_format_l_field(result, l_field);
    wmbus_selftest_format_computed_len(result, computed_len);

    if(test_case->vector->is_t_raw) {
        bool offset_match = wmbus_selftest_offset_match(test_case, result);
        wmbus_selftest_write_report_line(
            file,
            "%s %s mode=T plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s best_offset=%d expected_offset=%u expected_found=%s\n",
            pass ? "PASS" : "FAIL",
            test_case->name,
            result->plausible ? "YES" : "NO",
            l_field,
            computed_len,
            result->crc_ok ? "YES" : "NO",
            result->manufacturer,
            result->id,
            result->best_offset,
            test_case->vector->expected_offset,
            offset_match ? "YES" : "NO");
    } else {
        wmbus_selftest_write_report_line(
            file,
            "%s %s mode=C plausible=%s L=%s len=%s CRC=%s mfg=%s id=%s\n",
            pass ? "PASS" : "FAIL",
            test_case->name,
            result->plausible ? "YES" : "NO",
            l_field,
            computed_len,
            result->crc_ok ? "YES" : "NO",
            result->manufacturer,
            result->id);
    }
}

typedef bool (*WmBusSelftestCheckFn)(char* detail, size_t detail_len);

typedef struct {
    const char* name;
    WmBusSelftestCheckFn run;
} WmBusSelftestCheck;

static void wmbus_selftest_log_check_result(const char* name, const char* detail, bool pass) {
    if(pass) {
        FURI_LOG_I(TAG, "%s %s", name, detail);
    } else {
        FURI_LOG_W(TAG, "%s %s", name, detail);
    }
}

static void wmbus_selftest_report_check_result(
    File* file,
    const char* name,
    const char* detail,
    bool pass) {
    if(!file || !name || !detail) return;

    wmbus_selftest_write_report_line(file, "%s %s %s\n", pass ? "PASS" : "FAIL", name, detail);
}

static bool wmbus_selftest_check_3of6_valid_single_byte(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x58, 0xD0};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(!wmbus_parser_decode_3of6(raw, sizeof(raw), out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "decode failed");
        return false;
    }
    if(out_len != 1U || out[0] != 0x01U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected out_len=%u out0=%02X", (unsigned int)out_len, out[0]);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "decoded=01 len=1");
    return true;
}

static bool wmbus_selftest_check_3of6_valid_single_byte_offset_1(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x2C, 0x68, 0x00};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(!wmbus_parser_decode_3of6_bits(raw, 17U, 1U, out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "decode failed at offset=1");
        return false;
    }
    if(out_len != 1U || out[0] != 0x01U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected out_len=%u out0=%02X", (unsigned int)out_len, out[0]);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "decoded=01 len=1 offset=1");
    return true;
}

static bool wmbus_selftest_check_3of6_reject_dangling_nibble(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x58};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(wmbus_parser_decode_3of6(raw, sizeof(raw), out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "dangling nibble accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_3of6_reject_invalid_symbol(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x00};
    uint8_t out[4] = {0};
    size_t out_len = 0;

    if(wmbus_parser_decode_3of6(raw, sizeof(raw), out, sizeof(out), &out_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "invalid symbol accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_parser_plausibility(char* detail, size_t detail_len) {
    const uint8_t valid[] = {10, 0x44, 0x01, 0x06, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t bad_c[] = {10, 0x45, 0x01, 0x06, 0, 0, 0, 0, 0, 0, 0};
    const uint8_t bad_mfg[] = {10, 0x44, 0x00, 0x00, 0, 0, 0, 0, 0, 0, 0};

    if(!wmbus_parser_is_plausible(valid, sizeof(valid))) {
        wmbus_selftest_set_detail(detail, detail_len, "valid frame rejected");
        return false;
    }
    if(wmbus_parser_is_plausible(bad_c, sizeof(bad_c))) {
        wmbus_selftest_set_detail(detail, detail_len, "bad C-field accepted");
        return false;
    }
    if(wmbus_parser_is_plausible(bad_mfg, sizeof(bad_mfg))) {
        wmbus_selftest_set_detail(detail, detail_len, "bad manufacturer accepted");
        return false;
    }
    if(wmbus_parser_is_plausible(valid, 10U)) {
        wmbus_selftest_set_detail(detail, detail_len, "too-short frame accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "valid=YES bad_c=NO bad_mfg=NO short=NO");
    return true;
}

static bool wmbus_selftest_check_apator162_register_sizes(char* detail, size_t detail_len) {
    int size_10 = wmbus_parser_apator162_register_size(0x10);
    int size_a1 = wmbus_parser_apator162_register_size(0xA1);
    int size_b2 = wmbus_parser_apator162_register_size(0xB2);
    int size_fe = wmbus_parser_apator162_register_size(0xFE);

    if(size_10 != 4 || size_a1 != 4 || size_b2 != 16 || size_fe != -1) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "sizes 10=%d A1=%d B2=%d FE=%d",
            size_10,
            size_a1,
            size_b2,
            size_fe);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "10=4 A1=4 B2=16 FE=-1");
    return true;
}

static bool wmbus_selftest_check_capture_reconstruct_c_frame(char* detail, size_t detail_len) {
    const uint8_t payload[] = {0x44, 0x01, 0x06, 0x20, 0x20};
    uint8_t frame[16] = {0};
    size_t frame_len = 0;

    if(!wmbus_capture_reconstruct_c_frame(
           payload, sizeof(payload), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "reconstruct failed");
        return false;
    }
    if(frame_len != sizeof(payload) + 1U || frame[0] != sizeof(payload) || frame[1] != 0x44U ||
       frame[4] != 0x20U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected frame_len=%u L=%02X C=%02X id0=%02X",
            (unsigned int)frame_len,
            frame[0],
            frame[1],
            frame[4]);
        return false;
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "frame_len=%u L=%02X C=%02X", 6U, frame[0], frame[1]);
    return true;
}

static bool wmbus_selftest_check_capture_reconstruct_c_frame_reject_oversize(
    char* detail,
    size_t detail_len) {
    uint8_t payload[256] = {0};
    uint8_t frame[300] = {0};
    size_t frame_len = 0;

    if(wmbus_capture_reconstruct_c_frame(
           payload, sizeof(payload), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "oversize payload accepted");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool
    wmbus_selftest_check_capture_t_expected_raw_len_estimate(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x5A, 0x60};
    size_t expected_raw_len = 0;

    if(!wmbus_capture_estimate_t_expected_raw_len(raw, sizeof(raw), 256U, &expected_raw_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_raw_len != 23U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected expected_raw_len=%u", (unsigned int)expected_raw_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "expected_raw_len=23");
    return true;
}

static bool
    wmbus_selftest_check_capture_t_expected_raw_len_clamp(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x5A, 0x60};
    size_t expected_raw_len = 0;

    if(!wmbus_capture_estimate_t_expected_raw_len(raw, sizeof(raw), 20U, &expected_raw_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_raw_len != 20U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected expected_raw_len=%u", (unsigned int)expected_raw_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "expected_raw_len=20");
    return true;
}

static bool wmbus_selftest_check_capture_t_expected_raw_len_reject_invalid(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[] = {0x00, 0x00};
    size_t expected_raw_len = 0;

    if(wmbus_capture_estimate_t_expected_raw_len(raw, sizeof(raw), 256U, &expected_raw_len)) {
        wmbus_selftest_set_detail(
            detail, detail_len, "invalid raw accepted len=%u", (unsigned int)expected_raw_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "reject=YES");
    return true;
}

static bool wmbus_selftest_check_capture_c_frame_offset_waits_for_disambiguation(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x3E};
    size_t frame_offset = wmbus_capture_c_frame_offset(raw, sizeof(raw));

    if(frame_offset != SIZE_MAX) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected frame_offset=%u", (unsigned int)frame_offset);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "offset=WAIT");
    return true;
}

static bool
    wmbus_selftest_check_capture_c_frame_offset_with_signal_byte(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x3E, 0x44, 0x01, 0x06};
    size_t frame_offset = wmbus_capture_c_frame_offset(raw, sizeof(raw));

    if(frame_offset != 1U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected frame_offset=%u", (unsigned int)frame_offset);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "frame_offset=1");
    return true;
}

static bool
    wmbus_selftest_check_capture_c_frame_offset_l_field_54(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x44, 0x01, 0x06, 0x20};
    size_t frame_offset = wmbus_capture_c_frame_offset(raw, sizeof(raw));

    if(frame_offset != 0U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected frame_offset=%u", (unsigned int)frame_offset);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "frame_offset=0");
    return true;
}

static bool wmbus_selftest_check_capture_c_expected_len_estimate(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x3E, 0x44, 0x01, 0x06};
    size_t expected_len = 0;

    if(!wmbus_capture_estimate_c_expected_len(raw, sizeof(raw), 256U, &expected_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed");
        return false;
    }
    if(expected_len != 73U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected expected_len=%u", (unsigned int)expected_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "expected_len=73");
    return true;
}

static bool wmbus_selftest_check_capture_c_expected_len_estimate_with_signal_byte(
    char* detail,
    size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x3E, 0x44, 0x01, 0x06};
    size_t expected_len = 0;

    if(!wmbus_capture_estimate_c_expected_len(raw, sizeof(raw), 256U, &expected_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "signal-byte estimate failed");
        return false;
    }
    if(expected_len != 74U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected expected_len=%u", (unsigned int)expected_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "expected_len=74");
    return true;
}

static bool
    wmbus_selftest_check_capture_c_expected_len_real_l_field_54(char* detail, size_t detail_len) {
    const uint8_t raw[] = {0x54, 0x44, 0x01, 0x06, 0x20};
    size_t expected_len = 0;

    if(!wmbus_capture_estimate_c_expected_len(raw, sizeof(raw), 256U, &expected_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "estimate failed for L=54");
        return false;
    }
    if(expected_len != 97U) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected expected_len=%u", (unsigned int)expected_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "expected_len=97");
    return true;
}

static bool
    wmbus_selftest_check_frame_normalize_format_a_wire_frame(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_a(
           wmbus_apator_a, sizeof(wmbus_apator_a), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_frame_normalize(
           WmBusRxModeT, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-A failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok ||
       result.format != WmBusFrameFormatA || result.normalized_len != sizeof(wmbus_apator_a) ||
       memcmp(wmbus_apator_a, normalized, sizeof(wmbus_apator_a)) != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u",
            (unsigned int)result.format,
            result.length_ok ? 1U : 0U,
            result.crc_ok ? 1U : 0U,
            (unsigned int)result.normalized_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "format=A normalized_len=%u", 111U);
    return true;
}

static bool wmbus_selftest_check_frame_normalize_c_mode_format_a_wire_frame(
    char* detail,
    size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_a(
           wmbus_apator_b, sizeof(wmbus_apator_b), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-A failed");
        return false;
    }
    if(!wmbus_frame_normalize(
           WmBusRxModeC, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-A failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok ||
       result.format != WmBusFrameFormatA || result.normalized_len != sizeof(wmbus_apator_b) ||
       memcmp(wmbus_apator_b, normalized, sizeof(wmbus_apator_b)) != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u",
            (unsigned int)result.format,
            result.length_ok ? 1U : 0U,
            result.crc_ok ? 1U : 0U,
            (unsigned int)result.normalized_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "mode=C format=A normalized_len=%u", 79U);
    return true;
}

static bool
    wmbus_selftest_check_frame_normalize_format_b_wire_frame(char* detail, size_t detail_len) {
    uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t frame_len = 0;
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    WmBusFrameNormalizeResult result = {0};

    if(!wmbus_frame_build_format_b(
           wmbus_apator_c, sizeof(wmbus_apator_c), frame, sizeof(frame), &frame_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "build format-B failed");
        return false;
    }
    if(!wmbus_frame_normalize(
           WmBusRxModeC, frame, frame_len, normalized, sizeof(normalized), &result)) {
        wmbus_selftest_set_detail(detail, detail_len, "normalize format-B failed");
        return false;
    }
    if(!result.length_ok || !result.crc_known || !result.crc_ok ||
       result.format != WmBusFrameFormatB || result.normalized_len != sizeof(wmbus_apator_c) ||
       memcmp(wmbus_apator_c, normalized, sizeof(wmbus_apator_c)) != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected format=%u len_ok=%u crc_ok=%u normalized_len=%u",
            (unsigned int)result.format,
            result.length_ok ? 1U : 0U,
            result.crc_ok ? 1U : 0U,
            (unsigned int)result.normalized_len);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "format=B normalized_len=%u", 63U);
    return true;
}

static bool wmbus_selftest_check_parser_apator162_public_vectors(char* detail, size_t detail_len) {
    for(size_t i = 0; i < COUNT_OF(wmbus_selftest_apator_public_vectors); i++) {
        const WmBusSelftestApatorPublicVector* vector = &wmbus_selftest_apator_public_vectors[i];
        uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t normalized_len = 0;
        uint8_t frame[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t frame_len = 0;
        uint8_t roundtrip[WMBUS_SELFTEST_BUF_MAX] = {0};
        WmBusFrameNormalizeResult result = {0};
        WmBusPacketRecord record = {0};
        char id[WMBUS_ID_STR_LEN] = {0};

        if(!wmbus_selftest_hex_to_bytes(
               vector->telegram, normalized, sizeof(normalized), &normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s hex parse failed", vector->id);
            return false;
        }
        if(normalized_len != vector->parsed_len) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s unexpected len=%u",
                vector->id,
                (unsigned int)normalized_len);
            return false;
        }
        if(wmbus_selftest_fnv1a32(normalized, normalized_len) != vector->parsed_fnv1a) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s fingerprint mismatch", vector->id);
            return false;
        }
        if(normalized[0] != (uint8_t)(normalized_len - 1U) ||
           !wmbus_parser_is_plausible(normalized, normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s plausibility failed", vector->id);
            return false;
        }
        if(!wmbus_frame_build_format_a(
               normalized, normalized_len, frame, sizeof(frame), &frame_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s format-A build failed", vector->id);
            return false;
        }
        if(!wmbus_frame_normalize(
               WmBusRxModeT, frame, frame_len, roundtrip, sizeof(roundtrip), &result)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s normalize failed", vector->id);
            return false;
        }
        if(!result.length_ok || !result.crc_known || !result.crc_ok ||
           result.format != WmBusFrameFormatA || result.normalized_len != normalized_len ||
           memcmp(normalized, roundtrip, normalized_len) != 0) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s roundtrip failed", vector->id);
            return false;
        }
        wmbus_frame_format_id(&roundtrip[4], id, NULL);
        if(strncmp(vector->id, id, WMBUS_ID_STR_LEN) != 0) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector expected id=%s got=%s", vector->id, id);
            return false;
        }
        if(!wmbus_selftest_process_capture_record(
               WmBusRxModeC, roundtrip, result.normalized_len, NULL, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s packet process failed", vector->id);
            return false;
        }
        if(strcmp(record.data.parser_name, "Apator162") != 0 || !record.data.has_total_m3 ||
           record.data.total_m3_x1000 != vector->total_m3_x1000 ||
           strcmp(record.data.id_str, vector->id) != 0) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s total=%lu expected=%lu id=%s",
                vector->id,
                record.data.parser_name,
                (unsigned long)record.data.total_m3_x1000,
                (unsigned long)vector->total_m3_x1000,
                record.data.id_str);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "vectors=%u",
        (unsigned int)COUNT_OF(wmbus_selftest_apator_public_vectors));
    return true;
}

static bool wmbus_selftest_check_parser_apator162_old_style_ci_b6_rejected(
    char* detail,
    size_t detail_len) {
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t normalized_len = 0;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_bytes(
           wmbus_selftest_apator_old_style_b6, normalized, sizeof(normalized), &normalized_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "hex parse failed");
        return false;
    }
    if(normalized_len != wmbus_selftest_apator_old_style_b6_len) {
        wmbus_selftest_set_detail(
            detail, detail_len, "unexpected len=%u", (unsigned int)normalized_len);
        return false;
    }
    if(wmbus_selftest_fnv1a32(normalized, normalized_len) !=
       wmbus_selftest_apator_old_style_b6_fnv1a) {
        wmbus_selftest_set_detail(detail, detail_len, "fingerprint mismatch");
        return false;
    }
    if(normalized_len <= 10U || normalized[10] != 0xB6U) {
        wmbus_selftest_set_detail(detail, detail_len, "expected CI=B6");
        return false;
    }
    if(!wmbus_selftest_process_capture_record(
           WmBusRxModeC, normalized, normalized_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    if(strcmp(record.data.parser_name, "Apator162") == 0 || record.data.has_total_m3) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "old-style CI=B6 accepted parser=%s total=%lu",
            record.data.parser_name,
            (unsigned long)record.data.total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "ci=B6 rejected=YES");
    return true;
}

static bool
    wmbus_selftest_check_parser_apator162_mode5_zero_key_vectors(char* detail, size_t detail_len) {
    const struct {
        const char* telegram;
        uint32_t total_m3_x1000;
        const char* id;
    } vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5, 4848U, "88888888"},
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t normalized_len = 0;
        WmBusPacketRecord record = {0};
        uint16_t cfg = 0;

        if(!wmbus_selftest_hex_to_bytes(
               vectors[i].telegram, normalized, sizeof(normalized), &normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s hex parse failed", vectors[i].id);
            return false;
        }
        if(normalized_len < 17U || normalized[10] != 0x7AU) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s unexpected frame", vectors[i].id);
            return false;
        }
        cfg = (uint16_t)normalized[13] | ((uint16_t)normalized[14] << 8);
        if(!wmbus_parser_short_tpl_security_likely_encrypted(cfg) ||
           (normalized[15] == 0x2FU && normalized[16] == 0x2FU)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s unexpected cipher state", vectors[i].id);
            return false;
        }
        if(!wmbus_selftest_process_capture_record(
               WmBusRxModeC, normalized, normalized_len, NULL, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s packet process failed", vectors[i].id);
            return false;
        }
        if(strcmp(record.data.parser_name, "Apator162") != 0 || !record.data.has_total_m3 ||
           record.data.total_m3_x1000 != vectors[i].total_m3_x1000 || !record.data.decrypted ||
           record.data.key_index != 0U ||
           strcmp(record.data.id_str, vectors[i].id) != 0) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s total=%lu dec=%s idx=%u id=%s",
                vectors[i].id,
                record.data.parser_name,
                (unsigned long)record.data.total_m3_x1000,
                record.data.decrypted ? "YES" : "NO",
                (unsigned int)record.data.key_index,
                record.data.id_str);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "mode5 zero-key vectors=%u", (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_zero_key_fallback_vector(
    char* detail,
    size_t detail_len) {
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t normalized_len = 0;
    WmBusCaptureFrame capture = {0};
    WmBusKeyring keyring = {0};
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_bytes(
           wmbus_selftest_apator_encrypted_mode5,
           normalized,
           sizeof(normalized),
           &normalized_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "hex parse failed");
        return false;
    }

    memcpy(capture.data, normalized, normalized_len);
    capture.len = normalized_len;
    capture.raw_len = normalized_len;
    capture.rssi = -60;
    capture.mode = WmBusRxModeC;

    if(!wmbus_packet_process_capture(&capture, &keyring, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    if(strcmp(record.data.parser_name, "Apator162") != 0 || !record.data.has_total_m3 ||
       record.data.total_m3_x1000 != 4848U || !record.data.decrypted ||
       record.data.key_index != 0U || strcmp(record.data.id_str, "88888888") != 0) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "parser=%s total=%lu dec=%s idx=%u id=%s",
            record.data.parser_name,
            (unsigned long)record.data.total_m3_x1000,
            record.data.decrypted ? "YES" : "NO",
            (unsigned int)record.data.key_index,
            record.data.id_str);
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "parser=%s total=%lu fallback=zero id=%s",
        record.data.parser_name,
        (unsigned long)record.data.total_m3_x1000,
        record.data.id_str);
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_parser_zero_key_fallback(
    char* detail,
    size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t normalized_len = 0;
        WmBusCaptureFrame capture = {0};
        WmBusPacketRecord record = {0};
        const WmBusSelftestApatorFieldVector* vector = &vectors[i];

        if(!wmbus_selftest_hex_to_bytes(
               vector->telegram, normalized, sizeof(normalized), &normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s hex parse failed", vector->id);
            return false;
        }

        memcpy(capture.data, normalized, normalized_len);
        capture.len = normalized_len;
        capture.raw_len = normalized_len;
        capture.rssi = -60;
        capture.mode = WmBusRxModeC;

        if(!wmbus_packet_process_capture(&capture, NULL, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s packet process failed", vector->id);
            return false;
        }

        if(strcmp(record.data.parser_name, "Apator162") != 0 || !record.data.has_total_m3 ||
           record.data.total_m3_x1000 != vector->total_m3_x1000 || !record.data.decrypted ||
           record.data.key_index != 0U ||
           strcmp(record.data.id_str, vector->id) != 0) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s total=%lu dec=%s idx=%u id=%s",
                vector->id,
                record.data.parser_name,
                (unsigned long)record.data.total_m3_x1000,
                record.data.decrypted ? "YES" : "NO",
                (unsigned int)record.data.key_index,
                record.data.id_str);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "parser_fallback=zero vectors=%u", (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_configured_zero_key_slot(
    char* detail,
    size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };
    WmBusKeyring keyring = {0};

    memset(&keyring, 0, sizeof(keyring));
    keyring.count = 1U;

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t normalized_len = 0;
        WmBusCaptureFrame capture = {0};
        WmBusPacketRecord record = {0};
        const WmBusSelftestApatorFieldVector* vector = &vectors[i];

        if(!wmbus_selftest_hex_to_bytes(
               vector->telegram, normalized, sizeof(normalized), &normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s hex parse failed", vector->id);
            return false;
        }

        memcpy(capture.data, normalized, normalized_len);
        capture.len = normalized_len;
        capture.raw_len = normalized_len;
        capture.rssi = -60;
        capture.mode = WmBusRxModeC;

        if(!wmbus_packet_process_capture(&capture, &keyring, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s packet process failed", vector->id);
            return false;
        }

        if(strcmp(record.data.parser_name, "Apator162") != 0 || !record.data.has_total_m3 ||
           record.data.total_m3_x1000 != vector->total_m3_x1000 || !record.data.decrypted ||
           record.data.key_index != 1U ||
           strcmp(record.data.id_str, vector->id) != 0) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s total=%lu dec=%s idx=%u id=%s",
                vector->id,
                record.data.parser_name,
                (unsigned long)record.data.total_m3_x1000,
                record.data.decrypted ? "YES" : "NO",
                (unsigned int)record.data.key_index,
                record.data.id_str);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "configured_zero_key vectors=%u key_index=1",
        (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool wmbus_selftest_check_packet_process_mode5_wrong_key_falls_back_to_zero(
    char* detail,
    size_t detail_len) {
    const WmBusSelftestApatorFieldVector vectors[] = {
        {wmbus_selftest_apator_encrypted_mode5_gold, 345654U, "02991056"},
        {wmbus_selftest_apator_encrypted_mode5_field_02991035, 200257U, "02991035"},
    };
    WmBusKeyring keyring = {0};

    memset(&keyring, 0, sizeof(keyring));
    memset(keyring.entries[0].key, 0xA5, sizeof(keyring.entries[0].key));
    keyring.count = 1U;

    for(size_t i = 0; i < COUNT_OF(vectors); i++) {
        uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
        size_t normalized_len = 0;
        WmBusCaptureFrame capture = {0};
        WmBusPacketRecord record = {0};
        const WmBusSelftestApatorFieldVector* vector = &vectors[i];

        if(!wmbus_selftest_hex_to_bytes(
               vector->telegram, normalized, sizeof(normalized), &normalized_len)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s hex parse failed", vector->id);
            return false;
        }

        memcpy(capture.data, normalized, normalized_len);
        capture.len = normalized_len;
        capture.raw_len = normalized_len;
        capture.rssi = -60;
        capture.mode = WmBusRxModeC;

        if(!wmbus_packet_process_capture(&capture, &keyring, &record)) {
            wmbus_selftest_set_detail(
                detail, detail_len, "vector %s packet process failed", vector->id);
            return false;
        }

        if(strcmp(record.data.parser_name, "Apator162") != 0 || !record.data.has_total_m3 ||
           record.data.total_m3_x1000 != vector->total_m3_x1000 || !record.data.decrypted ||
           record.data.key_index != 0U ||
           strcmp(record.data.id_str, vector->id) != 0) {
            wmbus_selftest_set_detail(
                detail,
                detail_len,
                "vector %s parser=%s total=%lu dec=%s idx=%u id=%s",
                vector->id,
                record.data.parser_name,
                (unsigned long)record.data.total_m3_x1000,
                record.data.decrypted ? "YES" : "NO",
                (unsigned int)record.data.key_index,
                record.data.id_str);
            return false;
        }
    }

    wmbus_selftest_set_detail(
        detail, detail_len, "wrong_key=MISS zero=HIT vectors=%u", (unsigned int)COUNT_OF(vectors));
    return true;
}

static bool
    wmbus_selftest_check_parser_apator162_mode5_corrupt_rejected(char* detail, size_t detail_len) {
    uint8_t normalized[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t normalized_len = 0;
    WmBusPacketRecord record = {0};

    if(!wmbus_selftest_hex_to_bytes(
           wmbus_selftest_apator_encrypted_mode5_corrupt,
           normalized,
           sizeof(normalized),
           &normalized_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "hex parse failed");
        return false;
    }
    if(!wmbus_selftest_process_capture_record(
           WmBusRxModeC, normalized, normalized_len, NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }
    if(strcmp(record.data.parser_name, "Apator162") == 0 || record.data.has_total_m3 ||
       record.data.decrypted) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "corrupt ciphertext accepted parser=%s total=%lu dec=%s",
            record.data.parser_name,
            (unsigned long)record.data.total_m3_x1000,
            record.data.decrypted ? "YES" : "NO");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "mode5 corrupt rejected=YES");
    return true;
}
static bool wmbus_selftest_check_parser_apator162_payload_validate_without_total(
    char* detail,
    size_t detail_len) {
    static const char* payload_hex = "2F2F0F00000000000000436E0A415242FF";
    uint8_t payload[WMBUS_SELFTEST_BUF_MAX] = {0};
    size_t payload_len = 0U;
    uint32_t total_m3_x1000 = 0U;

    if(!wmbus_selftest_hex_to_bytes(payload_hex, payload, sizeof(payload), &payload_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "hex parse failed");
        return false;
    }

    if(!wmbus_parser_validate_apator162_payload(payload, payload_len)) {
        wmbus_selftest_set_detail(detail, detail_len, "payload validate failed");
        return false;
    }

    if(wmbus_parser_parse_apator162_payload_total(payload, payload_len, &total_m3_x1000)) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "payload unexpectedly parsed total=%lu",
            (unsigned long)total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "payload validate=YES total=NO");
    return true;
}

static bool wmbus_selftest_check_parser_apator162_invalid_payload_not_claimed(
    char* detail,
    size_t detail_len) {
    uint8_t frame[sizeof(wmbus_apator_b)] = {0};
    WmBusPacketRecord record = {0};

    memcpy(frame, wmbus_apator_b, sizeof(frame));
    frame[25] = 0xFEU;

    if(!wmbus_selftest_process_capture_record(WmBusRxModeC, frame, sizeof(frame), NULL, &record)) {
        wmbus_selftest_set_detail(detail, detail_len, "packet process failed");
        return false;
    }

    if(strcmp(record.data.parser_name, "Apator162") == 0 || record.data.has_total_m3) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "invalid payload accepted parser=%s total=%lu",
            record.data.parser_name,
            (unsigned long)record.data.total_m3_x1000);
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "invalid payload rejected=YES");
    return true;
}

static bool wmbus_selftest_check_short_tpl_security_modes(char* detail, size_t detail_len) {
    uint8_t clear_mode = wmbus_parser_short_tpl_security_mode(0x0000U);
    uint8_t aes_cbc_iv_mode = wmbus_parser_short_tpl_security_mode(0x8560U);
    uint8_t mfct_mode = wmbus_parser_short_tpl_security_mode(0x0100U);
    uint8_t aes_ctr_cmac_mode = wmbus_parser_short_tpl_security_mode(0x0800U);

    if(clear_mode != 0U || aes_cbc_iv_mode != 5U || mfct_mode != 1U || aes_ctr_cmac_mode != 8U) {
        wmbus_selftest_set_detail(
            detail,
            detail_len,
            "unexpected modes clear=%u aes=%u mfct=%u ctr=%u",
            (unsigned int)clear_mode,
            (unsigned int)aes_cbc_iv_mode,
            (unsigned int)mfct_mode,
            (unsigned int)aes_ctr_cmac_mode);
        return false;
    }

    if(wmbus_parser_short_tpl_security_likely_encrypted(0x0000U) ||
       !wmbus_parser_short_tpl_security_likely_encrypted(0x8560U) ||
       wmbus_parser_short_tpl_security_likely_encrypted(0x0100U) ||
       !wmbus_parser_short_tpl_security_likely_encrypted(0x0800U)) {
        wmbus_selftest_set_detail(detail, detail_len, "security encryption heuristic mismatch");
        return false;
    }

    wmbus_selftest_set_detail(
        detail,
        detail_len,
        "clear=%u aes=%u ctr=%u",
        (unsigned int)clear_mode,
        (unsigned int)aes_cbc_iv_mode,
        (unsigned int)aes_ctr_cmac_mode);
    return true;
}

static bool wmbus_selftest_check_capture_state_reset(char* detail, size_t detail_len) {
    WmBusCaptureStateT state_t = {
        .raw_len = 9U,
        .in_packet = true,
        .expected_raw_len = 42U,
        .last_byte_tick = 1234U,
    };
    WmBusCaptureStateC state_c = {
        .raw_len = 12U,
        .in_packet = true,
        .expected_len = 73U,
        .last_byte_tick = 5678U,
        .dropped_invalid = 5U,
        .dropped_oversize = 2U,
    };

    wmbus_capture_state_t_reset(&state_t);
    wmbus_capture_state_c_reset(&state_c);

    if(state_t.raw_len != 0U || state_t.in_packet || state_t.expected_raw_len != 0U ||
       state_t.last_byte_tick != 0U) {
        wmbus_selftest_set_detail(detail, detail_len, "T state reset failed");
        return false;
    }
    if(state_c.raw_len != 0U || state_c.in_packet || state_c.expected_len != 0U ||
       state_c.last_byte_tick != 0U || state_c.dropped_invalid != 0U ||
       state_c.dropped_oversize != 0U) {
        wmbus_selftest_set_detail(detail, detail_len, "C state reset failed");
        return false;
    }

    wmbus_selftest_set_detail(detail, detail_len, "state_reset=YES");
    return true;
}

static const WmBusSelftestCheck wmbus_selftest_checks_t[] = {
    {"check_3of6_valid_single_byte", wmbus_selftest_check_3of6_valid_single_byte},
    {"check_3of6_valid_single_byte_offset_1",
     wmbus_selftest_check_3of6_valid_single_byte_offset_1},
    {"check_3of6_reject_dangling_nibble", wmbus_selftest_check_3of6_reject_dangling_nibble},
    {"check_3of6_reject_invalid_symbol", wmbus_selftest_check_3of6_reject_invalid_symbol},
    {
        "check_capture_t_expected_raw_len_estimate",
        wmbus_selftest_check_capture_t_expected_raw_len_estimate,
    },
    {
        "check_capture_t_expected_raw_len_clamp",
        wmbus_selftest_check_capture_t_expected_raw_len_clamp,
    },
    {
        "check_capture_t_expected_raw_len_reject_invalid",
        wmbus_selftest_check_capture_t_expected_raw_len_reject_invalid,
    },
    {
        "check_frame_normalize_format_a_wire_frame",
        wmbus_selftest_check_frame_normalize_format_a_wire_frame,
    },
};

static const WmBusSelftestCheck wmbus_selftest_checks_c[] = {
    {"check_capture_reconstruct_c_frame", wmbus_selftest_check_capture_reconstruct_c_frame},
    {
        "check_capture_reconstruct_c_frame_reject_oversize",
        wmbus_selftest_check_capture_reconstruct_c_frame_reject_oversize,
    },
    {
        "check_capture_c_frame_offset_waits_for_disambiguation",
        wmbus_selftest_check_capture_c_frame_offset_waits_for_disambiguation,
    },
    {
        "check_capture_c_frame_offset_with_signal_byte",
        wmbus_selftest_check_capture_c_frame_offset_with_signal_byte,
    },
    {
        "check_capture_c_frame_offset_l_field_54",
        wmbus_selftest_check_capture_c_frame_offset_l_field_54,
    },
    {"check_capture_c_expected_len_estimate",
     wmbus_selftest_check_capture_c_expected_len_estimate},
    {
        "check_capture_c_expected_len_estimate_with_signal_byte",
        wmbus_selftest_check_capture_c_expected_len_estimate_with_signal_byte,
    },
    {
        "check_capture_c_expected_len_real_l_field_54",
        wmbus_selftest_check_capture_c_expected_len_real_l_field_54,
    },
    {
        "check_frame_normalize_c_mode_format_a_wire_frame",
        wmbus_selftest_check_frame_normalize_c_mode_format_a_wire_frame,
    },
    {
        "check_frame_normalize_format_b_wire_frame",
        wmbus_selftest_check_frame_normalize_format_b_wire_frame,
    },
};

static const WmBusSelftestCheck wmbus_selftest_checks_common[] = {
    {"check_parser_plausibility", wmbus_selftest_check_parser_plausibility},
    {"check_apator162_register_sizes", wmbus_selftest_check_apator162_register_sizes},
    {
        "check_parser_apator162_public_vectors",
        wmbus_selftest_check_parser_apator162_public_vectors,
    },
    {
        "check_parser_apator162_old_style_ci_b6_rejected",
        wmbus_selftest_check_parser_apator162_old_style_ci_b6_rejected,
    },
    {
        "check_parser_apator162_mode5_zero_key_vectors",
        wmbus_selftest_check_parser_apator162_mode5_zero_key_vectors,
    },
    {
        "check_parser_apator162_payload_validate_without_total",
        wmbus_selftest_check_parser_apator162_payload_validate_without_total,
    },
    {
        "check_parser_apator162_invalid_payload_not_claimed",
        wmbus_selftest_check_parser_apator162_invalid_payload_not_claimed,
    },
    {
        "check_packet_process_mode5_zero_key_fallback_vector",
        wmbus_selftest_check_packet_process_mode5_zero_key_fallback_vector,
    },
    {
        "check_packet_process_mode5_parser_zero_key_fallback",
        wmbus_selftest_check_packet_process_mode5_parser_zero_key_fallback,
    },
    {
        "check_packet_process_mode5_configured_zero_key_slot",
        wmbus_selftest_check_packet_process_mode5_configured_zero_key_slot,
    },
    {
        "check_packet_process_mode5_wrong_key_falls_back_to_zero",
        wmbus_selftest_check_packet_process_mode5_wrong_key_falls_back_to_zero,
    },
    {
        "check_parser_apator162_mode5_corrupt_rejected",
        wmbus_selftest_check_parser_apator162_mode5_corrupt_rejected,
    },
    {"check_short_tpl_security_modes", wmbus_selftest_check_short_tpl_security_modes},
    {"check_capture_state_reset", wmbus_selftest_check_capture_state_reset},
};

static void wmbus_selftest_note_summary(WmBusSelftestSummary* summary, bool pass) {
    summary->total++;
    if(pass) {
        summary->passed++;
    } else {
        summary->failed++;
    }
}

static void wmbus_selftest_run_checks(
    WmBusSelftestSummary* summary,
    bool log_results,
    File* report,
    const WmBusSelftestCheck* checks,
    size_t check_count) {
    for(size_t i = 0; i < check_count; i++) {
        char detail[WMBUS_SELFTEST_LINE_MAX] = {0};
        bool pass = checks[i].run(detail, sizeof(detail));

        wmbus_selftest_note_summary(summary, pass);
        if(log_results) {
            wmbus_selftest_log_check_result(checks[i].name, detail, pass);
        }
        if(report) {
            wmbus_selftest_report_check_result(report, checks[i].name, detail, pass);
        }
    }
}

static void
    wmbus_selftest_run_internal(WmBusSelftestSummary* summary, bool log_results, File* report) {
    for(size_t i = 0; i < wmbus_selftest_get_case_count(); i++) {
        const WmBusSelftestCase* test_case = wmbus_selftest_get_case(i);
        WmBusSelftestResult result = {0};
        bool pass = wmbus_selftest_run_case(test_case, &result);

        wmbus_selftest_note_summary(summary, pass);
        if(log_results) {
            wmbus_selftest_log_case_result(test_case, &result, pass);
        }
        if(report) {
            wmbus_selftest_report_case_result(report, test_case, &result, pass);
        }
    }

    wmbus_selftest_run_checks(
        summary, log_results, report, wmbus_selftest_checks_t, COUNT_OF(wmbus_selftest_checks_t));
    wmbus_selftest_run_checks(
        summary, log_results, report, wmbus_selftest_checks_c, COUNT_OF(wmbus_selftest_checks_c));
    wmbus_selftest_run_checks(
        summary,
        log_results,
        report,
        wmbus_selftest_checks_common,
        COUNT_OF(wmbus_selftest_checks_common));
}

void wmbus_selftest_run_all(WmBusSelftestSummary* summary, bool log_results) {
    WmBusSelftestSummary local_summary = {0};
    if(!summary) summary = &local_summary;

    memset(summary, 0, sizeof(*summary));
    wmbus_selftest_run_internal(summary, log_results, NULL);
}

void wmbus_run_selftests(void) {
#if WMBUS_SELFTESTS
    WmBusSelftestSummary summary = {0};
    Storage* storage = furi_record_open(RECORD_STORAGE);
    wmbus_storage_ensure_app_folder(storage);
    File* report = storage_file_alloc(storage);
    bool report_open =
        storage_file_open(report, WMBUS_SELFTEST_REPORT_PATH, FSAM_WRITE, FSOM_CREATE_ALWAYS);

    FURI_LOG_I(TAG, "selftests begin");
    if(report_open) {
        wmbus_selftest_write_report_line(report, "selftests begin\n");
    } else {
        FURI_LOG_W(TAG, "failed to open selftest report: %s", WMBUS_SELFTEST_REPORT_PATH);
    }

    wmbus_selftest_run_internal(&summary, true, report_open ? report : NULL);

    if(summary.failed == 0) {
        FURI_LOG_I(
            TAG, "selftests done total=%lu passed=%lu failed=0", summary.total, summary.passed);
        if(report_open) {
            wmbus_selftest_write_report_line(
                report,
                "selftests done total=%lu passed=%lu failed=0\n",
                summary.total,
                summary.passed);
        }
    } else {
        FURI_LOG_W(
            TAG,
            "selftests done total=%lu passed=%lu failed=%lu",
            summary.total,
            summary.passed,
            summary.failed);
        if(report_open) {
            wmbus_selftest_write_report_line(
                report,
                "selftests done total=%lu passed=%lu failed=%lu\n",
                summary.total,
                summary.passed,
                summary.failed);
        }
    }

    if(report_open) {
        storage_file_close(report);
    }
    storage_file_free(report);
    furi_record_close(RECORD_STORAGE);
#endif
}
