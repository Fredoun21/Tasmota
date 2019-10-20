/*
  xnrg_11_ddsu666.ino - Chint DDSU666-Modbus energy meter support for Sonoff-Tasmota

  Copyright (C) 2019  Pablo Zerón and Theo Arends

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_ENERGY_SENSOR
#ifdef USE_SFH620A
/*********************************************************************************************\
 * Chint SFH620A Serial TeleInfo counter energy
\*********************************************************************************************/

#define XNRG_13             13

// can be user defined in my_user_config.h
#ifndef SFH620A_SPEED
  #define SFH620A_SPEED      1200    // default teleinfo speed
#endif
// can be user defined in my_user_config.h
#ifndef SFH620A_RX
  #define SFH620A_RX       12       // default DDSU66 serial software GPIO
#endif

#include <TasmotaSerial.h>
TasmotaSerial *Sfh620aSerial;

const uint16_t Ddsu666_start_addresses[] {
  0x2000,   // DDSU666_VOLTAGE             [V]
  0x2002,   // DDSU666_CURRENT             [A]
  0x2004,   // DDSU666_POWER               [KW]
  0x2006,   // DDSU666_REACTIVE_POWER      [KVAR]
  0x200A,   // DDSU666_POWER_FACTOR
  0x200E,   // DDSU666_FREQUENCY           [Hz]
  0X4000,   // DDSU666_IMPORT_ACTIVE       [kWh]
  0X400A,   // DDSU666_EXPORT_ACTIVE       [kWh]
};

struct DDSU666 {
  float import_active = NAN;
  uint8_t read_state = 0;
  uint8_t send_retry = 0;
} Ddsu666;

/*********************************************************************************************/

void SFH620Avery250ms(void)
{
  bool data_ready = Ddsu666Modbus->ReceiveReady();

  if (data_ready) {
    uint8_t buffer[14];  // At least 5 + (2 * 2) = 9

    uint32_t error = Ddsu666Modbus->ReceiveBuffer(buffer, 2);
    AddLogBuffer(LOG_LEVEL_DEBUG_MORE, buffer, Ddsu666Modbus->ReceiveCount());

    if (error) {
      AddLog_P2(LOG_LEVEL_DEBUG, PSTR("SDM: Ddsu666 error %d"), error);
    } else {
      Energy.data_valid[0] = 0;
     
      //  0  1  2  3  4  5  6  7  8
      // SA FC BC Fh Fl Sh Sl Cl Ch
      // 01 04 04 43 66 33 34 1B 38 = 230.2 Volt
      float value;
      ((uint8_t*)&value)[3] = buffer[3];   // Get float values
      ((uint8_t*)&value)[2] = buffer[4];
      ((uint8_t*)&value)[1] = buffer[5];
      ((uint8_t*)&value)[0] = buffer[6];

      switch(Ddsu666.read_state) {
        case 0:
          Energy.voltage[0] = value;          // 230.2 V
          break;

        case 1:
          Energy.current[0]  = value;         // 1.260 A
          break;

        case 2:
          Energy.active_power[0] = value * 1000;     // -196.3 W
          break;

        case 3:
          Energy.reactive_power[0] = value * 1000;   // 92.2
          break;

        case 4:
          Energy.power_factor[0] = value;     // 0.91
          break;

        case 5:
          Energy.frequency[0] = value;        // 50.0 Hz
          break;

        case 6:
          Ddsu666.import_active = value;    // 478.492 kWh
          break;

        case 7:
          Energy.export_active = value;    // 6.216 kWh
          break;
      }

      Ddsu666.read_state++;

      if (Ddsu666.read_state == 8) {
        Ddsu666.read_state = 0;
        EnergyUpdateTotal(Ddsu666.import_active, true);  // 484.708 kWh
      }
    }
  } // end data ready

  if (0 == Ddsu666.send_retry || data_ready) {
    Ddsu666.send_retry = 5;
    Ddsu666Modbus->Send(DDSU666_ADDR, 0x04, Ddsu666_start_addresses[Ddsu666.read_state], 2);
  } else {
    Ddsu666.send_retry--;
  }
}

void SFH620ASnsInit(void)
{
  Ddsu666Modbus = new TasmotaModbus(pin[GPIO_DDSU666_RX], pin[GPIO_DDSU666_TX]);
  uint8_t result = Ddsu666Modbus->Begin(DDSU666_SPEED);
  if (result) {
    if (2 == result) { ClaimSerial(); }
  } else {
    energy_flg = ENERGY_NONE;
  }
}

void SFH620ADrvInit(void)
{
  if ((pin[GPIO_DDSU666_RX] < 99) && (pin[GPIO_DDSU666_TX] < 99)) {
    energy_flg = XNRG_11;
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xnrg13(uint8_t function)
{
  bool result = false;

  switch (function) {
    case FUNC_EVERY_250_MSECOND:
      if (uptime > 4) { SFH620AEvery250ms(); }
      break;
    case FUNC_INIT:
      SFH620ASnsInit();
      break;
    case FUNC_PRE_INIT:
      SFH620ADrvInit();
      break;
  }
  return result;
}

#endif  // USE_SFH620A
#endif  // USE_ENERGY_SENSOR
