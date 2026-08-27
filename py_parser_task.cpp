#include "py_uart.h"
#include "py_log.h"
#include "config.h"
#include "py_parser.h"

extern PyUart py_uart;
extern QueueHandle_t frameQueue;

/*
 * Parser Task
 * -----------
 * Reads frames from the UART frame queue and dispatches them
 * to the corresponding parser (PWR / BAT / STAT).
 *
 * All parsers use RAM‑optimized global tables:
 *   - PWR  → pwrTable
 *   - BAT  → batTable
 *   - STAT → statTable
 *
 * The raw frame content is always taken from py_uart.getLastRawFrame().
 */

void parserTask(void* param)
{
    UartFrame f;

    while (true)
    {
        // Wait for next UART frame
        if (xQueueReceive(frameQueue, &f, portMAX_DELAY) != pdPASS)
            continue;

        // Always fetch raw frame directly from UART
        String raw = py_uart.getLastRawFrame();

        Log(LOG_DEBUG,
            "parserTask: FRAME received type=" + String(f.type) +
            " len=" + String(raw.length()));

        if (!py_uart.isFrameValid()) {
            Log(LOG_WARN, "parserTask: frame invalid → skip");
            vTaskDelay(5);
            continue;
        }

        // ---------------------------------------------------------
        // PWR FRAME
        // ---------------------------------------------------------
        if (f.type == FRAME_PWR)
        {
            Log(LOG_DEBUG, "parserTask: PWR frame");

            ParseResult r = parsePwrFrame(raw);

            if (r == PARSE_OK)
            {
                // Signal that new PWR data is available
                parserHasData = true;

                // Prepare module publishing
                pwrFrameReady    = true;
                pwrCurrentModule = 0;
                pwrTotalModules  = pwrTable.rows;
            }

            Log(LOG_DEBUG, "parserTask: parsePwrFrame -> " + String(r));
        }

        // ---------------------------------------------------------
        // BAT FRAME
        // ---------------------------------------------------------
        else if (f.type == FRAME_BAT)
        {
            Log(LOG_DEBUG, "parserTask: BAT frame");

            ParseResult r = parseBatFrame(f.module, raw);

            if (r == PARSE_OK)
            {
                batParserHasData     = true;
                batParserModuleIndex = f.module;
            }

            Log(LOG_DEBUG, "parserTask: parseBatFrame -> " + String(r));
        }

        // ---------------------------------------------------------
        // STAT FRAME
        // ---------------------------------------------------------
        else if (f.type == FRAME_STAT)
        {
            Log(LOG_DEBUG, "parserTask: STAT frame");

            ParseResult r = parseStatFrame(f.module, raw);

            if (r == PARSE_OK)
            {
                statParserHasData     = true;
                statParserModuleIndex = f.module;
            }

            Log(LOG_DEBUG, "parserTask: parseStatFrame -> " + String(r));
        }

        vTaskDelay(5);
    }
}
