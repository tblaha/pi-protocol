
#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include "pi-protocol.h"
#include "pi-messages.h"

// --- sender --- //
#if (PI_MODE & PI_TX)
void piSendMsg(void * msg_raw, void (*serialWriter)(uint8_t byte)) {
    // cast to uint8_t pointer
    uint8_t * msg = (uint8_t *) msg_raw;

    // start byte first. Next field (id) will be handled with the usual escape
    serialWriter(PI_STX);

    // this is not that nice, because it assume that id and len are 1 byte
    bool id_sent = false;
    uint8_t id = *(msg++);
    uint8_t len = *(msg++);

    while (len-- > 0) {
        uint8_t byte;
        if (!id_sent) {
            byte = id;
            len++;
            id_sent = true;
        } else {
            byte = *(msg++);
        }
        switch(byte) {
            case PI_STX:
                serialWriter(PI_ESC);
                serialWriter(PI_STX_ESC);
                break;
            case PI_ESC:
                serialWriter(PI_ESC);
                serialWriter(PI_ESC_ESC);
                break;
            default:
                serialWriter(byte);
        }
    }
}
#endif // #if (PI_MODE & PI_TX)

// --- parser ---
#if (PI_MODE & PI_RX)
__attribute__((unused)) void piParse(uint8_t byte) {
    static bool piEscHit = false;
    static pi_parse_state_t piState = PI_IDLE;
    static uint8_t msgId = PI_MSG_NONE_ID;
    static uint8_t byteCount = 0;
    static pi_parse_msg_result_t msgParseResult = PI_PARSE_MSG_NO_ERROR;

#ifdef PI_STATS
    piStats[PI_PARSE_INVOKE]++;
#endif

    if (byte == PI_STX) {
#ifdef PI_STATS
        piStats[PI_STX_COUNT]++;
        if (msgParseResult == PI_PARSE_MSG_SUCCESS)
            piStats[PI_SUCCESS]++;
#endif
        piEscHit = false;
        piState = PI_STX_FOUND;
        return;
    }

    if (piState == PI_IDLE)
        return;

    if (piEscHit) {
        piEscHit = false;
        switch(byte) {
            case PI_STX_ESC:
                byte = PI_STX;
                break;
            case PI_ESC_ESC:
                byte = PI_ESC;
                break;
            default:
                // failure, next byte MUST have been PI_STX_ESC or PI_ESC_ESC
#ifdef PI_DEBUG
                printf("Escaping error: next byte was neither PI_STX_ESC nor PI_ESC_ESC, but rather %02hhX\n", byte);
#endif
#ifdef PI_STATS
                piStats[PI_ESC_ERROR]++;
#endif
                piState = PI_IDLE;
                return;
        }
    } else {
        if (byte == PI_ESC) {
            piEscHit = true;
            return;
        }
    }


    switch(piState) {
        case PI_IDLE: // can never happen, so fall through to satisfy GCC -Wall
            break;
        case PI_STX_FOUND:
            // parse id
            msgId = byte;
            piState = PI_ID_FOUND;
            byteCount = 0;
            break;
        case PI_ID_FOUND:
            // payload time
            msgParseResult = piParseIntoMsg(msgId, byte, byteCount++);
            /* No success handling here, because we require an STX
               next, otherwise it's not actually success, but EXCEED_MSG_LEN
               */
            if (msgParseResult > PI_PARSE_MSG_SUCCESS) {
#ifdef PI_DEBUG
                printf("\n msgParseResult > PI_PARSE_MSG_SUCCESS at id 0x%02hhX, byte 0x%02hhX: %d\n", msgId, byte, msgParseResult);
                msgId = PI_MSG_NONE_ID;
                piState = PI_IDLE;
#endif
#ifdef PI_STATS
                piStats[msgParseResult]++;
#endif
            }
            break;
    }
}

#ifdef PI_STATS
unsigned int piStats[NUM_PI_STATS_RESULT] = {0,0,0,0, 0,0,0,0, 0};

__attribute__((unused)) void piPrintStats(int (*printer)(const char * s, ...)) {
    static int i = 0;
    printer("\n+------------ piPrintStats invokation %d -------------+\n", i++);
    printer("|%-30s|%-10s|\n", "Result", "Occurence");
    printer("|------------------------------|----------|\n");
    for (int j=0; j < NUM_PI_STATS_RESULT; j++)
        printer("|%30s|%10d|\n", piStatsNames[j], piStats[j]);
    printer("|------------------------------|----------|\n");
}
#endif// #ifdef PI_STATS

#endif// #if (PI_MODE & PI_RX)


#ifdef __cplusplus
}
#endif