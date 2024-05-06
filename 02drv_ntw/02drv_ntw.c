#include <string.h>
#include <stdbool.h>
#include "board.h"
#include "periodictimer.h"
#include "ntw.h"
#include <nrf.h>
#include "nrf52833.h"

//=========================== defines =========================================

#define DATA_PERIOD_S 10

//=========================== typedef =========================================

//=========================== variables =======================================

static uint8_t pdu[] = {
    0x00, // header
       2, // length
    0x0F, 0x00
};

void init_radio(){
// configured HF clock
    NRF_CLOCK->TASKS_HFCLKSTART = 1;
    while (NRF_CLOCK->EVENTS_HFCLKSTARTED == 0) {}

    // configure radio
    NRF_RADIO->MODE          = (  RADIO_MODE_MODE_Ble_LR125Kbit << RADIO_MODE_MODE_Pos);
    NRF_RADIO->TXPOWER       = (  RADIO_TXPOWER_TXPOWER_Pos8dBm << RADIO_TXPOWER_TXPOWER_Pos);
    NRF_RADIO->PCNF0         = (                              8 << RADIO_PCNF0_LFLEN_Pos)          |
                               (                              1 << RADIO_PCNF0_S0LEN_Pos)          |
                               (                              0 << RADIO_PCNF0_S1LEN_Pos)          |
                               (                              2 << RADIO_PCNF0_CILEN_Pos)          |
                               (     RADIO_PCNF0_PLEN_LongRange << RADIO_PCNF0_PLEN_Pos)           |
                               (                              3 << RADIO_PCNF0_TERMLEN_Pos);
    NRF_RADIO->PCNF1         = (                    sizeof(pdu) << RADIO_PCNF1_MAXLEN_Pos)         |
                               (                              0 << RADIO_PCNF1_STATLEN_Pos)        |
                               (                              3 << RADIO_PCNF1_BALEN_Pos)          |
                               (      RADIO_PCNF1_ENDIAN_Little << RADIO_PCNF1_ENDIAN_Pos)         |
                               (   RADIO_PCNF1_WHITEEN_Disabled << RADIO_PCNF1_WHITEEN_Pos);
    NRF_RADIO->BASE0         = 0xAAAAAAAAUL;
    NRF_RADIO->TXADDRESS     = 0UL;
    NRF_RADIO->RXADDRESSES   = (RADIO_RXADDRESSES_ADDR0_Enabled << RADIO_RXADDRESSES_ADDR0_Pos);
    NRF_RADIO->TIFS          = 1000U;
    NRF_RADIO->CRCCNF        = (         RADIO_CRCCNF_LEN_Three << RADIO_CRCCNF_LEN_Pos)           |
                               (     RADIO_CRCCNF_SKIPADDR_Skip << RADIO_CRCCNF_SKIPADDR_Pos);
    NRF_RADIO->CRCINIT       = 0xFFFFUL;
    NRF_RADIO->CRCPOLY       = 0x00065b; // CRC poly: x^16 + x^12^x^5 + 1
    NRF_RADIO->FREQUENCY     = 10;
    NRF_RADIO->PACKETPTR     = (uint32_t)pdu;

    NRF_RADIO->INTENCLR = 0xffffffff;
    NRF_RADIO->SHORTS = (RADIO_SHORTS_READY_START_Enabled << RADIO_SHORTS_READY_START_Pos) |
                        (RADIO_SHORTS_END_DISABLE_Enabled << RADIO_SHORTS_END_DISABLE_Pos);
    NRF_RADIO->INTENSET = (RADIO_INTENSET_DISABLED_Enabled << RADIO_INTENSET_DISABLED_Pos);
    NVIC_EnableIRQ(RADIO_IRQn);

}

typedef struct {
    uint8_t        txCounter;
} app_vars_t;

app_vars_t app_vars;

typedef struct {
    uint32_t       numReceive;
    uint32_t       numTransmit;
    uint32_t       numTransmit_success;
    uint32_t       numTransmit_fail;
} app_dbg_t;

app_dbg_t app_dbg;

//=========================== prototypes ======================================

void _periodtimer_cb(void);
void _ntw_receive_cb(uint8_t* buf, uint8_t bufLen);

//=========================== main ============================================

int main(void) {

    init_radio();

    // initialize variables
    memset(&app_vars,0x00,sizeof(app_vars));
    memset(&app_dbg, 0x00,sizeof(app_dbg));
    
    // bsp
    board_init();

    // ntw
    ntw_init(NULL, NULL, _ntw_receive_cb);

    // initialize the periodic timer
    periodictimer_init(
        DATA_PERIOD_S,       // period_s
        _periodtimer_cb      // periodtimer_cb
    );

    // main loop
    while(1) {

        // wait for event 
        board_sleep();
    }
}

//=========================== private =========================================

void _periodtimer_cb(void) {
    uint8_t txBuf[4];
    bool    success;

    // increment
    app_vars.txCounter++;

    // fill txBuf
    txBuf[0] = app_vars.txCounter;
    txBuf[1] = 0x01;
    txBuf[2] = 0x02;
    txBuf[3] = 0x03;

    // send txBuf
    success = ntw_transmit(txBuf,sizeof(txBuf));

    // debug
    app_dbg.numTransmit++;
    if (success==true) {
        app_dbg.numTransmit_success++;
    } else {
        app_dbg.numTransmit_fail++;
    }
}

void send_radio(){

    NRF_RADIO->TASKS_TXEN = (RADIO_TASKS_TXEN_TASKS_TXEN_Trigger << RADIO_TASKS_TXEN_TASKS_TXEN_Pos);
    while (NRF_RADIO->EVENTS_DISABLED != 0) {}
}

void _ntw_receive_cb(uint8_t* buf, uint8_t bufLen) {
    
    // debug
    app_dbg.numReceive++;
    pdu[2] = buf[0];
    pdu[3] = buf[1];
    send_radio();
}

void RADIO_IRQHandler(void) {
    if (NRF_RADIO->EVENTS_DISABLED) {
        NRF_RADIO->EVENTS_DISABLED = 0;
    }
}