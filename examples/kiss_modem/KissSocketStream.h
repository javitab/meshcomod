#pragma once

#include <Arduino.h>

// Borrows a connected socket. The server owns closing it and resetting the modem.
class KissSocketStream : public Stream {
  int _fd = -1;
  int _error = 0;
  bool _closed = false;
  uint8_t _input[512];
  size_t _pos = 0;
  size_t _len = 0;
  size_t _read_budget = 0;
  uint32_t _last_write_ms = 0;

  void recordError();

public:
  void attach(int fd);
  void beginPoll() { _read_budget = sizeof(_input); }
  bool closed() const { return _closed; }
  int error() const { return _error; }
  uint32_t lastWriteMillis() const { return _last_write_ms; }

  int available() override;
  int read() override;
  int peek() override;
  int availableForWrite() override;
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* data, size_t len) override;
  void flush() override {}
};
