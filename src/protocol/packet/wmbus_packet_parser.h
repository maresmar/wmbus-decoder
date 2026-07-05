#pragma once

#include "wmbus_packet.h"

bool wmbus_packet_parse_application(WmBusPacketRecord* record);
void wmbus_packet_finalize_parser(WmBusPacketRecord* record);
