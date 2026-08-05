#include "wmbus_capture_processor.h"

#include "../protocol/packet/wmbus_packet.h"

#include <furi.h>
#include <stdlib.h>
#include <string.h>

#define TAG "WmBusDecoder"

#define WMBUS_CAPTURE_PROCESSOR_MAX_SINKS 4U
#define WMBUS_CAPTURE_PROCESSOR_QUEUE_DEPTH 6U
#define WMBUS_CAPTURE_PROCESSOR_STACK_SIZE 12288U

typedef enum {
    WmBusProcessorEventFrame = 0,
    WmBusProcessorEventStop,
} WmBusProcessorEventType;

typedef struct {
    WmBusProcessorEventType type;
    WmBusSettings settings;
    WmBusCryptoKeyStore key_store;
    WmBusPhyFrame phy_frame;
} WmBusProcessorEvent;

struct WmBusCaptureProcessor {
    const WmBusPacketSink* sinks[WMBUS_CAPTURE_PROCESSOR_MAX_SINKS];
    size_t sink_count;
    FuriMessageQueue* queue;
    FuriThread* thread;
    bool started;
    uint32_t dropped_frames;
};

static void wmbus_capture_processor_process_frame(
    WmBusCaptureProcessor* processor,
    const WmBusProcessorEvent* event) {
    WmBusPacketRecord record = {0};
    if(!wmbus_packet_process_phy_frame(&event->phy_frame, &event->key_store, &record)) {
        return;
    }

    for(size_t i = 0; i < processor->sink_count; i++) {
        processor->sinks[i]->consume(
            processor->sinks[i]->context, &event->settings, &record);
    }
}

static int32_t wmbus_capture_processor_thread(void* context) {
    WmBusCaptureProcessor* processor = context;
    WmBusProcessorEvent event;

    while(furi_message_queue_get(processor->queue, &event, FuriWaitForever) == FuriStatusOk) {
        if(event.type == WmBusProcessorEventStop) break;
        wmbus_capture_processor_process_frame(processor, &event);
    }

    return 0;
}

WmBusCaptureProcessor* wmbus_capture_processor_alloc(void) {
    WmBusCaptureProcessor* processor = malloc(sizeof(*processor));
    if(!processor) {
        return NULL;
    }

    memset(processor, 0, sizeof(*processor));
    processor->queue = furi_message_queue_alloc(
        WMBUS_CAPTURE_PROCESSOR_QUEUE_DEPTH, sizeof(WmBusProcessorEvent));
    if(!processor->queue) {
        free(processor);
        return NULL;
    }

    processor->thread = furi_thread_alloc_ex(
        "WmBusProcess",
        WMBUS_CAPTURE_PROCESSOR_STACK_SIZE,
        wmbus_capture_processor_thread,
        processor);
    if(!processor->thread) {
        furi_message_queue_free(processor->queue);
        free(processor);
        return NULL;
    }
    furi_thread_set_priority(processor->thread, FuriThreadPriorityLow);
    return processor;
}

void wmbus_capture_processor_free(WmBusCaptureProcessor* processor) {
    if(!processor) return;

    if(processor->started) {
        WmBusProcessorEvent event = {.type = WmBusProcessorEventStop};
        furi_message_queue_put(processor->queue, &event, FuriWaitForever);
        furi_thread_join(processor->thread);
    }
    furi_thread_free(processor->thread);
    furi_message_queue_free(processor->queue);
    free(processor);
}

bool wmbus_capture_processor_add_sink(
    WmBusCaptureProcessor* processor,
    const WmBusPacketSink* sink) {
    if(!processor || processor->started || !sink || !sink->consume ||
       processor->sink_count >= WMBUS_CAPTURE_PROCESSOR_MAX_SINKS) {
        return false;
    }

    processor->sinks[processor->sink_count++] = sink;
    return true;
}

bool wmbus_capture_processor_start(WmBusCaptureProcessor* processor) {
    if(!processor || processor->started || processor->sink_count == 0U) return false;

    processor->started = true;
    furi_thread_start(processor->thread);
    return true;
}

bool wmbus_capture_processor_submit_frame(
    WmBusCaptureProcessor* processor,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store,
    const WmBusPhyFrame* phy_frame) {
    if(!processor || !processor->started || !settings || !phy_frame) {
        return false;
    }

    WmBusProcessorEvent event = {
        .type = WmBusProcessorEventFrame,
        .settings = *settings,
        .phy_frame = *phy_frame,
    };
    if(key_store) event.key_store = *key_store;

    if(furi_message_queue_put(processor->queue, &event, 0U) != FuriStatusOk) {
        processor->dropped_frames++;
        if(processor->dropped_frames == 1U ||
           (processor->dropped_frames & (processor->dropped_frames - 1U)) == 0U) {
            FURI_LOG_W(
                TAG,
                "processor queue full: dropped=%lu mode=%c",
                (unsigned long)processor->dropped_frames,
                phy_frame->mode == WmBusRxModeT ? 'T' : 'C');
        }
        return false;
    }

    return true;
}
