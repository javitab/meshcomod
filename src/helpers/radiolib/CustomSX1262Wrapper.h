#pragma once

#include "CustomSX1262.h"
#include "RadioLibWrappers.h"
#include "SX126xReset.h"

#ifndef USE_SX1262
#define USE_SX1262
#endif

class CustomSX1262Wrapper : public RadioLibWrapper {
public:
  CustomSX1262Wrapper(CustomSX1262& radio, mesh::MainBoard& board) : RadioLibWrapper(radio, board) { }

  void setParams(float freq, float bw, uint8_t sf, uint8_t cr) override {
    int16_t result = setParamsChecked(freq, bw, sf, cr);
    if (result != RADIOLIB_ERR_NONE) {
      MESH_DEBUG_PRINTLN("SX1262: setParams failed (%d)", result);
    }
  }

  int16_t setParamsChecked(float freq, float bw, uint8_t sf, uint8_t cr) {
    int16_t result = ((CustomSX1262 *)_radio)->setFrequency(freq);
    if (result != RADIOLIB_ERR_NONE) return result;
    result = ((CustomSX1262 *)_radio)->setSpreadingFactor(sf);
    if (result != RADIOLIB_ERR_NONE) return result;
    result = ((CustomSX1262 *)_radio)->setBandwidth(bw);
    if (result != RADIOLIB_ERR_NONE) return result;
    result = ((CustomSX1262 *)_radio)->setCodingRate(cr);
    if (result != RADIOLIB_ERR_NONE) return result;
    result = ((CustomSX1262 *)_radio)->setPreambleLength(preambleLengthForSF(sf));
    if (result != RADIOLIB_ERR_NONE) return result;
    _preamble_sf = sf;
    PacketMillis pm = calcMaxPacketMillis(sf, bw, cr, preambleLengthForSF(sf));
    ((CustomSX1262 *)_radio)->setPreambleMillis(pm.preambleMillis);
    ((CustomSX1262 *)_radio)->setMaxPayloadMillis(pm.payloadMillis);
    return RADIOLIB_ERR_NONE;
  }

  bool isReceivingPacket() override { 
    return ((CustomSX1262 *)_radio)->isReceiving();
  }
  float getCurrentRSSI() override {
    return ((CustomSX1262 *)_radio)->getRSSI(false);
  }
  float getLastRSSI() const override { return ((CustomSX1262 *)_radio)->getRSSI(); }
  float getLastSNR() const override { return ((CustomSX1262 *)_radio)->getSNR(); }

  float packetScore(float snr, int packet_len) override {
    int sf = ((CustomSX1262 *)_radio)->spreadingFactor;
    return packetScoreInt(snr, sf, packet_len);
  }
  uint8_t getSpreadingFactor() const override { return ((CustomSX1262 *)_radio)->spreadingFactor; }
  virtual void powerOff() override {
    ((CustomSX1262 *)_radio)->sleep(false);
  }

  void doResetAGC() override { sx126xResetAGC((SX126x *)_radio); }

  // See RadioLibWrapper::pollRxIfNoIrq — RX-done straight from the IRQ register.
  bool pollRxDone() override {
    return (((CustomSX1262 *)_radio)->getIrqFlags() & RADIOLIB_SX126X_IRQ_RX_DONE) != 0;
  }

  bool setRxBoostedGainMode(bool en) override {
    return ((CustomSX1262 *)_radio)->setRxBoostedGainMode(en) == RADIOLIB_ERR_NONE;
  }
  bool getRxBoostedGainMode() const override {
    return ((CustomSX1262 *)_radio)->getRxBoostedGainMode();
  }
};
