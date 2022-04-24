#pragma once

#include "env_basic.h"

#define RDMA_SERVER_IP "172.18.158.94"
#define RDMA_TIMEOUT_IN_MS 1000


struct MetaMessage{
  enum MetaType: uint16_t {
    META_RECV_MR = 0,
    META_RECV_MESSAGE
  } type;
  union {
    struct ibv_mr mr;
    uint64_t messages;
  } data;
};

struct RdmaMessage {
  enum Type : uint16_t {
    MSG_MR = 0,
    MSG_CMD,
    MSG_DONE
  } type;
  // byte number
  RdmaMessage(uint32_t l, uint32_t c_l) : length(l) {
    // TODO mzy: avoid doing malloc every time
    rdma_pck = (byte *)malloc(length + 12);
    data = rdma_pck + 8;
    m_write_4(rdma_pck, RDMA_MESSAGE_MAGIC);
    m_write_4(rdma_pck + 4, length);
    m_write_4(rdma_pck + RDMA_MESSAGE_HEAD_LEN + length, RDMA_MESSAGE_MAGIC);
    pck_len = length + 12;
  }
  ~RdmaMessage() {
    free(rdma_pck);
  }
  byte *data;
  byte *rdma_pck;
  uint32_t length;
  uint32_t pck_len;
};

struct conn_parameter {
  env_basic_param *env;
  bool  connected;
  int   num_of_qps;
  int   wr_cq_number;
};

struct conn_context {
	rdma_cm_id			*cm_id_control;
	rdma_cm_id			*cm_id;
  rdma_event_channel		*cm_channel;
  /* Context */
  ibv_context		*context;
	ibv_comp_channel			*comp_channel;
	ibv_pd				*pd;
  ibv_cq				*cq;
  ibv_qp				*qp;
	// ibv_cq				*send_cq;
	// ibv_cq				*recv_cq;
  /* Meta Message memory */
  ibv_mr *meta_recv_mr;
  ibv_mr *meta_send_mr;
  MetaMessage *meta_recv;
  MetaMessage *meta_send;
  /* cq polling thread */
  pthread_t cq_poller_thread;
};

struct conn_callback {
  // TODO mzy: callbacks

};

struct rdma_mem_info {
  struct ibv_mr *dst_mr;
  struct ibv_mr *src_mr;
  uint32_t length;
  uint32_t offset;

  bool use_imm_data;
  uint32_t imm_data;
  uint64_t local_address;
};

class ConnectionProc {
public:
  ConnectionProc() {};
  virtual ~ConnectionProc() {};

  virtual int16_t run() = 0;
  virtual void stop() = 0;

  /* deal with the completion event */
  void on_completion(ibv_wc *wc);

protected:
  /* variables */
  conn_parameter  conn_params;
  conn_context    conn_ctx;
  conn_callback   conn_calls;

  /* functions */
  virtual int16_t on_connection(rdma_cm_id *id) = 0;
  virtual int16_t on_disconnect(rdma_cm_id *id) = 0;
  virtual int16_t on_event(rdma_cm_event *event) = 0;

  // /* deal with received meta message */
  // void on_meta_message();

  void register_message_memory();
  int16_t build_connection(rdma_cm_id *id);
  void destroy_connection();

  /* recv "meta_recv" message from the remote */
  int16_t post_meta_recv_wr();
  /* send "meta_send" message to the remote */
  int16_t post_meta_send_wr();


  // /* get imm_data of RDMA based on the imm_code and user_data, imm_code no more than
  // 8 bits, user_data no more than 24 bits */
  // uint32_t get_rdma_imm_data(uint32_t imm_code, uint32_t user_data);


  /* Directly rdma read/write, Note: the memory must be registered by reletive mr */
  int16_t rdma_write(rdma_mem_info &info);
  int16_t rdma_read(rdma_mem_info &info);

  typedef enum DmaType {
    RDMA_REMOTE_WRITE = 1,  // RDMA remote write
    RDMA_REMOTE_READ = 2  // RDMA remote read
  } DmaType_t;
  int16_t rdma_op(DmaType_t dma_type, rdma_mem_info &info);
};

class RDMAServer : public ConnectionProc {
public:
  RDMAServer(EnvironmentProc *env);
  virtual ~RDMAServer() {};

  virtual int16_t run();
  virtual void stop();

protected:
  virtual int16_t on_connection(rdma_cm_id *id);
  virtual int16_t on_disconnect(rdma_cm_id *id);
  virtual int16_t on_event(rdma_cm_event *event);

  int16_t on_connect_request(rdma_cm_id *id);
};

class RDMAClient : public ConnectionProc {
public:
  RDMAClient(EnvironmentProc *env);
  virtual ~RDMAClient() {};

  virtual int16_t run();
  virtual void stop();

protected:
  virtual int16_t on_connection(rdma_cm_id *id);
  virtual int16_t on_disconnect(rdma_cm_id *id);
  virtual int16_t on_event(rdma_cm_event *event);

  int16_t on_addr_resolved(rdma_cm_id *id);
  int16_t on_route_resolved(rdma_cm_id *id);
};