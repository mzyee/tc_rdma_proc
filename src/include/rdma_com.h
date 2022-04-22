#pragma once

#include "env_basic.h"

#define RDMA_SERVER_IP "172.18.158.94"
#define RDMA_TIMEOUT_IN_MS 1000

class RDMA_Server : public Connection_Proc {
public:
  RDMA_Server(Environment_Proc *env);
  virtual ~RDMA_Server() {};

  virtual int16_t run();
  virtual void stop();

protected:
  virtual int16_t on_connection(rdma_cm_id *id);
  virtual int16_t on_disconnect(rdma_cm_id *id);
  virtual int16_t on_event(rdma_cm_event *event);

  int16_t on_connect_request(rdma_cm_id *id);
};

class RDMA_Client : public Connection_Proc {
public:
  RDMA_Client(Environment_Proc *env);
  virtual ~RDMA_Client() {};

  virtual int16_t run();
  virtual void stop();

protected:
  virtual int16_t on_connection(rdma_cm_id *id);
  virtual int16_t on_disconnect(rdma_cm_id *id);
  virtual int16_t on_event(rdma_cm_event *event);

  int16_t on_addr_resolved(rdma_cm_id *id);
  int16_t on_route_resolved(rdma_cm_id *id);
};