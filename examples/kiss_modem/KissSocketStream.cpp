#if defined(ESP32) || defined(KISS_SOCKET_NATIVE_TEST)
#include "KissSocketStream.h"

#ifdef ESP32
#include <lwip/sockets.h>
#else
#include <sys/socket.h>
#include <sys/select.h>
#endif
#include <errno.h>
#include <fcntl.h>

void KissSocketStream::attach(int fd) {
  _fd = fd;
  _error = 0;
  _closed = false;
  _pos = _len = _read_budget = 0;
  _last_write_ms = millis();
  if (_fd >= 0) {
    int flags = fcntl(_fd, F_GETFL, 0);
    if (flags < 0 || fcntl(_fd, F_SETFL, flags | O_NONBLOCK) < 0) {
      _error = errno;
      _closed = true;
    }
  }
}

void KissSocketStream::recordError() {
  if (errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) {
    _error = errno;
    _closed = true;
  }
}

int KissSocketStream::available() {
  if (_fd < 0 || _closed || !_read_budget) return 0;
  if (_pos == _len) {
    size_t cap = _read_budget < sizeof(_input) ? _read_budget : sizeof(_input);
    int n = recv(_fd, _input, cap, MSG_DONTWAIT);
    if (n < 0) {
      recordError();
      return 0;
    }
    if (n == 0) {
      _closed = true;
      return 0;
    }
    _pos = 0;
    _len = static_cast<size_t>(n);
  }
  size_t left = _len - _pos;
  return static_cast<int>(left < _read_budget ? left : _read_budget);
}

int KissSocketStream::read() {
  if (!available()) return -1;
  --_read_budget;
  return _input[_pos++];
}

int KissSocketStream::peek() {
  return available() ? _input[_pos] : -1;
}

int KissSocketStream::availableForWrite() {
  if (_fd < 0 || _closed) return 0;
  fd_set write_fds;
  FD_ZERO(&write_fds);
  FD_SET(_fd, &write_fds);
  timeval timeout = {0, 0};
  int ready = select(_fd + 1, nullptr, &write_fds, nullptr, &timeout);
  if (ready < 0) recordError();
  return ready > 0 ? 512 : 0;
}

size_t KissSocketStream::write(const uint8_t* data, size_t len) {
  if (_fd < 0 || _closed || !len) return 0;
  int flags = MSG_DONTWAIT;
#ifdef MSG_NOSIGNAL
  flags |= MSG_NOSIGNAL;
#endif
  int n = send(_fd, data, len, flags);
  if (n < 0) {
    recordError();
    return 0;
  }
  if (n > 0) _last_write_ms = millis();
  return static_cast<size_t>(n);
}
#endif
