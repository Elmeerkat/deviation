/*
 This project is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.

 Deviation is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with Deviation.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifdef MODULAR
  //Allows the linker to properly relocate
  #define SUMD_Cmds PROTO_Cmds
  #pragma long_calls
#endif

#include "common.h"
#include "interface.h"
#include "mixer.h"
#include "config/model.h"
#include "config/tx.h"
//#include "target.h"

#ifdef MODULAR
  #pragma long_calls_off
  extern unsigned _data_loadaddr;
  const unsigned long protocol_type = (unsigned long)&_data_loadaddr;
#endif
#define SUMD_CHANNELS NUM_OUT_CHANNELS
static u8 packet[2 * SUMD_CHANNELS + 5];
u8 num_channels;

ctassert(LAST_PROTO_OPT <= NUM_PROTO_OPTS, too_many_protocol_opts);

volatile u8 state;

static u16 crc_val;

void CRC16_reset()
{
    crc_val = 0;
}

void CRC16(u8 value)
{
    u8 ii;
    crc_val = crc_val ^ (s16) value << 8;
    for (ii = 0; ii < 8; ii++)
    {
        if (crc_val & 0x8000)
        {
            crc_val = (crc_val << 1) ^ 0x1021;
        } else
        {
            crc_val = (crc_val << 1);
        }
    }
}

static void build_data_pkt()
{
    int ii;
    CRC16_reset();
    packet[0] = 0xA8;
    packet[1] = 0x01;
    packet[2] = num_channels;
    CRC16(packet[0]);
    CRC16(packet[1]);
    CRC16(packet[2]);
    for (ii = 0; ii < num_channels; ii++) {
        s32 value = (s32)Channels[ii] * 4800 / CHAN_MAX_VALUE
                    + 12000;
        packet[ii * 2 + 3] = value >> 8;
        packet[ii * 2 + 4] = value & 0xFF;
        CRC16(packet[ii * 2 + 3]);
        CRC16(packet[ii * 2 + 4]);
    }
    packet[num_channels * 2 + 3] = crc_val >> 8;
    packet[num_channels * 2 + 4] = crc_val & 0xFF;
}

static u16 sumd_cb()
{
    build_data_pkt();
    UART_Send(packet, 2 * SUMD_CHANNELS + 5);
#ifdef EMULATOR
    return 3000;
#else
    return 10000;
#endif
}

static void initialize()
{
    CLOCK_StopTimer();
    if (PPMin_Mode())
    {
        return;
    }
    UART_SetDataRate(115200);
#if HAS_EXTENDED_AUDIO
#if HAS_AUDIO_UART5
    if (Transmitter.audio_uart5) return;
#endif
    Transmitter.audio_player = AUDIO_DISABLED; // disable voice commands on serial port
#endif
    num_channels = Model.num_channels;
    state = 0;
    CLOCK_StartTimer(1000, sumd_cb);
}

const void * SUMD_Cmds(enum ProtoCmds cmd)
{
    switch(cmd) {
        case PROTOCMD_INIT:  initialize(); return 0;
        case PROTOCMD_DEINIT: UART_SetDataRate(0); return 0;
        case PROTOCMD_CHECK_AUTOBIND: return (void *)1L;
        case PROTOCMD_BIND:  initialize(); return 0;
        case PROTOCMD_NUMCHAN: return (void *)12L;
        case PROTOCMD_DEFAULT_NUMCHAN: return (void *)8L;
        case PROTOCMD_GETOPTIONS: return (void*)0L;
        case PROTOCMD_TELEMETRYSTATE: return (void *)(long)PROTO_TELEM_UNSUPPORTED;
        default: break;
    }
    return 0;
}