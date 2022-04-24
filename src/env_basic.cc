#include <assert.h>
#include <getopt.h>

#include "env_basic.h"
#include "rdma_com.h"

static const char *portStates[] = {"Nop", "Down", "Init", "Armed", "", "Active Defer"};

void EnvironmentProc::init_env_params() {
  env_params.server_port = DEF_PORT;
  env_params.ib_port = DEF_IB_PORT;
  env_params.link_type = LINK_UNSPEC;

  env_params.connection_type = RC;
  env_params.machine = SERVER;

  env_params.cache_line_size = DEF_CACHE_LINE_SIZE;
}

int16_t EnvironmentProc::parse_params(int16_t argc, char *argv[]) {

  static const struct option long_options[] = {
    { name : "ib-dev",  has_arg : 1, flag : NULL, val : 'd' },
    { name : "ib-port",  has_arg : 1, flag : NULL, val : 'i' },
    { name : "dest_ip",  has_arg : 1, flag : NULL, val : 'J' },
    { name : "source_ip",  has_arg : 1, flag : NULL, val : 'j' },
    { name : "dest_port",  has_arg : 1, flag : NULL, val : 'K' },
    { name : "source_port",  has_arg : 1, flag : NULL, val : 'k' },
    { name : "server",  has_arg : 0, flag : NULL, val : 'Z' },
    { name : "client",  has_arg : 0, flag : NULL, val : 'P' },
    {0}
  };

  int ch;
  char *not_int_ptr = NULL;
  while (1) {
    ch = getopt_long(argc, argv, "p:d:i:c:J:j:K:k:ZP", long_options, NULL);

    if (ch == -1) {
      break;
    }

    switch (ch) {
      case 'd': { GET_STRING(env_params.ib_devname, strdupa(optarg)); } break;
      case 'i': {
        CHECK_VALUE(env_params.ib_port, uint8_t, "IB Port", not_int_ptr);
        if (env_params.ib_port < MIN_IB_PORT) {
          fprintf(stderr, "IB Port can't be less than %d\n", MIN_IB_PORT);
          return FAILURE;
        }
      } break;
      case 'J': { env_params.server_ip = strdup(optarg); } break;
      case 'j': {} break;
      case 'K': {CHECK_VALUE(env_params.server_port, int, "Server Port", not_int_ptr);} break;
      case 'k': {} break;
      case 'Z': { env_params.machine = SERVER; } break;
      case 'P': { env_params.machine = CLIENT; }  break;
    default:
      return FAILURE;
      break;
    }
  }

  return SUCCESS;
}

int16_t EnvironmentProc::check_env() {
  ibv_device *ib_dev = ctx_find_dev(&env_params.ib_devname);
  if (!ib_dev) {
    std::cerr << "Unable to find the Infiniband/RoCE device!" << std::endl;
    return FAILURE;
  }
  env_context = ibv_open_device(ib_dev);
  if (!env_context) {
    std::cerr << "Couldn't get context for the device!" << std::endl;
    return FAILURE;
  }

  ctx_device current_dev = ib_dev_name(env_context);

  env_params.transport_type = env_context->device->transport_type;

  ibv_port_attr port_attr;
  int8_t curr_link = env_params.link_type;

  if (ibv_query_port(env_context, env_params.ib_port, &port_attr)) {
    fprintf(stderr, " Unable to query port %d attributes\n", env_params.ib_port);
    return FAILURE;
  }

  if (curr_link == LINK_UNSPEC)
    env_params.link_type = port_attr.link_layer;

  if (port_attr.state != IBV_PORT_ACTIVE) {
    fprintf(stderr, " Port number %d state is %s\n",
            env_params.ib_port, portStates[port_attr.state]);
    return FAILURE;
  }

  if (strcmp("Unknown", link_layer_str(env_params.link_type)) == 0) {
    fprintf(stderr, "Link layer on port %d is Unknown\n", env_params.ib_port);
    return FAILURE;
  }

  return SUCCESS;
}

/* We use a cq polling thread to maximum RDMA message throughput.
NOTE: don't exec long sync task on_completion, instead, to dispatch
  them if needed. */
static void *poll_cq(void *ctx) {
  conn_context *conn_ctx = static_cast<conn_context *>(ctx);
  ibv_wc  wc;
  ibv_cq  *ev_cq;
  void    *ev_ctx;
  int     num_comp;

  while (1) {
    ibv_get_cq_event(conn_ctx->comp_channel, &ev_cq, &ev_ctx);
    ibv_ack_cq_events(ev_cq, 1);
    if (ibv_req_notify_cq(ev_cq, 0)) {
      // TODO mzy: failover
      fprintf(stderr, "poll_cq error ibv_req_notify_cq.");
      assert(false);
    }

    while ((num_comp = ibv_poll_cq(ev_cq, 1, &wc)) > 0){
        ConnectionProc *conn = (ConnectionProc *)(uintptr_t)wc.wr_id;
        conn->on_completion(&wc);
    }
    if (num_comp < 0){
      // TODO mzy: failover
      fprintf(stderr, "poll_cq error ibv_poll_cq.");
      assert(false);
    }
  }

  return NULL;
}

void ConnectionProc::on_completion(ibv_wc *wc) {
  // TODO mzy: local completion callback
  if (wc->status != IBV_WC_SUCCESS){
    fprintf(stderr, " error wc status %d\n", wc->status);
    return;
  }

  switch (wc->opcode) {
  case IBV_WC_RECV: {
    fprintf(stdout, "wc opcode IBV_WC_RECV\n");

  } break;
  case IBV_WC_RECV_RDMA_WITH_IMM: {
    fprintf(stdout, "wc opcode IBV_WC_RECV_RDMA_WITH_IMM\n");

  } break;
  case IBV_WC_RDMA_READ: {
    fprintf(stdout, "wc opcode IBV_WC_RDMA_READ\n");

  } break;
  default:
    fprintf(stdout, "unprocessed wc opcode %d\n", wc->opcode);
    break;
  }
}

void ConnectionProc::register_message_memory() {
  /* Meta Message */
  conn_ctx.meta_send = (MetaMessage *)malloc(sizeof(MetaMessage));
  conn_ctx.meta_recv = (MetaMessage *)malloc(sizeof(MetaMessage));
  conn_ctx.meta_send_mr = ibv_reg_mr(conn_ctx.pd, conn_ctx.meta_send, sizeof(MetaMessage), 0);
  conn_ctx.meta_recv_mr = ibv_reg_mr(conn_ctx.pd, conn_ctx.meta_recv, sizeof(MetaMessage), IBV_ACCESS_LOCAL_WRITE);
}

int16_t ConnectionProc::post_meta_recv_wr() {
	ibv_recv_wr wr;
	ibv_recv_wr *bad_wr;
	ibv_sge list;

	list.addr   = (uint64_t)conn_ctx.meta_recv;
	list.length = sizeof(MetaMessage);
	list.lkey   = conn_ctx.meta_recv_mr->lkey;

	wr.next = NULL;
	wr.wr_id = (uint64_t)this;
	wr.sg_list = &list;
	wr.num_sge = 1;

	return ibv_post_recv(conn_ctx.qp, &wr, &bad_wr);
}

int16_t ConnectionProc::post_meta_send_wr() {
  ibv_send_wr wr;
  ibv_send_wr *bad_wr;
  ibv_sge list;

  memset(&wr, 0, sizeof(wr));

	list.addr   = (uint64_t)conn_ctx.meta_send;
	list.length = sizeof(MetaMessage);
	list.lkey   = conn_ctx.meta_send_mr->lkey;

  wr.wr_id = (uint64_t)this;
  wr.opcode = IBV_WR_SEND;
  wr.sg_list = &list;
  wr.num_sge = 1;
  wr.send_flags = IBV_SEND_SIGNALED; // No Call Back
  /* it is necessary to post a WQE that opens the completion
  signal. Because only after CQE is generated, the program will
  clear the SQE of the sending queue SQ after reading the CQE
  */

  assert (conn_params.connected);

  return ibv_post_send(conn_ctx.qp, &wr, &bad_wr);
}

void ConnectionProc::destroy_connection() {

  rdma_destroy_qp(conn_ctx.cm_id);

  ibv_dereg_mr(conn_ctx.meta_send_mr);
  ibv_dereg_mr(conn_ctx.meta_recv_mr);

  free(conn_ctx.meta_send);
  free(conn_ctx.meta_recv);


  rdma_destroy_id(conn_ctx.cm_id);
}

int16_t ConnectionProc::build_connection(rdma_cm_id *id) {
  /* 1. build ibv context */
  if (conn_ctx.context) {
    if (conn_ctx.context != id->verbs)
      return FAILURE;
  } else {
    conn_ctx.context = id->verbs;
  }

  conn_ctx.pd = ibv_alloc_pd(conn_ctx.context);
  conn_ctx.comp_channel = ibv_create_comp_channel(conn_ctx.context);
  conn_ctx.cq = ibv_create_cq(conn_ctx.context, conn_params.wr_cq_number,
                              NULL, conn_ctx.comp_channel, 0);

  ibv_req_notify_cq(conn_ctx.cq, 0);

  /* 2. create complete queue polling thread */
  pthread_create(&conn_ctx.cq_poller_thread, NULL, poll_cq, &conn_ctx);

  /* 3. build attributed queue pair */
  ibv_qp_init_attr qp_attr;
  memset(&qp_attr, 0, sizeof(qp_attr));

  qp_attr.send_cq = conn_ctx.cq;
  qp_attr.recv_cq = conn_ctx.cq;
  qp_attr.qp_type = IBV_QPT_RC;

  qp_attr.cap.max_send_wr = conn_params.wr_cq_number;
  qp_attr.cap.max_recv_wr = conn_params.wr_cq_number;
  qp_attr.cap.max_send_sge = 1;
  qp_attr.cap.max_recv_sge = 1;

  qp_attr.sq_sig_all = 0;

  if (rdma_create_qp(id, conn_ctx.pd, &qp_attr)) {
    return FAILURE;
  }

  /* 4. set connection context and meta mr */
  id->context = this;
  assert(conn_ctx.cm_id == id);
  conn_ctx.qp = id->qp;
  conn_params.connected = 0;

  register_message_memory();

  /* 5. prepare full recv wr for meta */
  for (int32_t i = 0; i < conn_params.wr_cq_number; ++i) {
    post_meta_recv_wr();
  }

  /* other meta in class */

  return SUCCESS;
}

