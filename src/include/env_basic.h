#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <iostream>
#include <atomic>

#include "perftest_parameters.h"
#include "perftest_resources.h"
#include "multicast_resources.h"
#include "perftest_communication.h"
#include "link_buf.h"

/************** Basic Environment ***************/
struct env_basic_param {
  char  *ib_devname;
  char  *servername;
  char  *server_ip;
  int32_t     server_port;

  int32_t     ib_port;
  int32_t     link_type;

  int32_t     connection_type;
  int32_t     transport_type;

  MachineType machine;
  int32_t     cache_line_size;
};

class EnvironmentProc {
public:
  EnvironmentProc() { init_env_params(); }
  ~EnvironmentProc() {};

  int16_t parse_params(int16_t p_num, char *pars[]);
  int16_t check_env();

  env_basic_param *get_params() { return &env_params; }
  bool is_server() {
    return env_params.machine == SERVER;
  }

private:
  env_basic_param   env_params;
  ibv_context       *env_context;

  void init_env_params();
};

#define byte unsigned char
#define RDMA_MESSAGE_MAGIC  (uint32_t)491237815
#define RDMA_MESSAGE_HEAD_LEN  4 + 4
static inline void m_write_1(byte *b, uint64_t n) {
  b[0] = static_cast<byte>(n);
}
static inline void m_write_2(byte *b, uint64_t n) {
  b[0] = static_cast<byte>(n >> 8);
  b[1] = static_cast<byte>(n);
}
static inline void m_write_4(byte *b, uint64_t n) {
  b[0] = static_cast<byte>(n >> 24);
  b[1] = static_cast<byte>(n >> 16);
  b[2] = static_cast<byte>(n >> 8);
  b[3] = static_cast<byte>(n);
}
static inline void m_write_8(byte *b, uint64_t n) {
  m_write_4(b, (uint64_t)(n >> 32));
  m_write_4(b + 4, (uint64_t)(n));
}
static inline uint8_t m_read_1(const byte *b) {
  return (static_cast<uint8_t>(b[0]));
}
static inline uint16_t m_read_2(const byte *b) {
  return ((static_cast<uint16_t>(b[0]) << 8) |
          (static_cast<uint16_t>(b[1])));
}
static inline uint32_t m_read_4(const byte *b) {
  return ((static_cast<uint32_t>(b[0]) << 24) |
          (static_cast<uint32_t>(b[1]) << 16) |
          (static_cast<uint32_t>(b[2]) << 8) |
          (static_cast<uint32_t>(b[3])));
}
static inline uint64_t m_read_8(const byte *b) {
  uint64_t u64;
  u64 = m_read_4(b);
  u64 <<= 32;
  u64 |= m_read_4(b + 4);
  return u64;
}

