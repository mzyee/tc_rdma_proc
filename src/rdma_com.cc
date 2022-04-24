#include "rdma_com.h"

/********** client **********/
RDMAServer::RDMAServer(EnvironmentProc *env){
  /* set parameters */
  conn_params.env = env->get_params();
  assert(conn_params.env->machine == SERVER);
  conn_params.connected = false;
  conn_params.num_of_qps = 1;
  conn_params.wr_cq_number = 128;
}

int16_t RDMAServer::on_connect_request(rdma_cm_id *id) {
  /* build ibv connection */
  if (build_connection(id)) {
    return FAILURE;
  }

  rdma_conn_param cm_params;

  memset(&cm_params, 0, sizeof(cm_params));
  cm_params.initiator_depth = 1;
  cm_params.responder_resources = 1;
  cm_params.rnr_retry_count = 7; /* infinite retry */

  if (rdma_accept(id, &cm_params)) {
    return FAILURE;
  }

  return SUCCESS;
}

int16_t RDMAServer::on_connection(rdma_cm_id *id) {
  conn_params.connected = true;

  /* TODO mzy: init & send meta on connection */

  post_meta_send_wr();
  return SUCCESS;
}

int16_t RDMAServer::on_disconnect(rdma_cm_id *id) {
  assert (id->context == static_cast<void *>(this));

  destroy_connection();

  /* return FAILURE to break waitting */
  return FAILURE;
}

int16_t RDMAServer::on_event(rdma_cm_event *event) {
  int16_t ret = SUCCESS;

  if (event->event == RDMA_CM_EVENT_CONNECT_REQUEST)
    ret = on_connect_request(event->id);
  else if (event->event == RDMA_CM_EVENT_ESTABLISHED)
    ret = on_connection(event->id);
  else if (event->event == RDMA_CM_EVENT_DISCONNECTED)
    ret = on_disconnect(event->id);
  else
    ret = FAILURE;

  return ret;
}

int16_t RDMAServer::run() {
  rdma_cm_id **cm_id = &conn_ctx.cm_id_control;
  conn_ctx.cm_channel = rdma_create_event_channel();
  if (conn_ctx.cm_channel == NULL) {
    return FAILURE;
  }
  if (rdma_create_id(conn_ctx.cm_channel, cm_id, NULL, RDMA_PS_TCP)) {
    return FAILURE;
  }

  /* Now use rdma cm to establish connection*/
	char *service;
	char* src_ip = NULL;
  addrinfo hints;
  addrinfo *res;
  rdma_cm_event *event;
  rdma_cm_event event_copy;
  rdma_conn_param conn_param;
  sockaddr_in sin;

	memset(&hints, 0, sizeof hints);
	hints.ai_flags    = AI_PASSIVE;
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

  memset(&sin, 0x0, sizeof(sin));
	memset(&conn_param, 0, sizeof(conn_param));

	/* check port */
	if (check_add_port(&service, conn_params.env->server_port, src_ip, &hints, &res)) {
		fprintf(stderr, "Problem in resolving basic address and port\n");
		return FAILURE;
	}

	if (res->ai_family != PF_INET) {
		return FAILURE;
	}
	memcpy(&sin, res->ai_addr, sizeof(sin));
	sin.sin_port = htons((unsigned short)conn_params.env->server_port);
	freeaddrinfo(res);

	/* bound rdma channel with address:port */
	if (rdma_bind_addr(conn_ctx.cm_id_control, (struct sockaddr *)&sin)) {
		fprintf(stderr," rdma_bind_addr failed\n");
		return FAILURE;
	}

	/* set backlog = num_of_qps */
	if (rdma_listen(conn_ctx.cm_id_control, conn_params.num_of_qps)) {
		fprintf(stderr, "rdma_listen failed\n");
		return FAILURE;
	}

  if (rdma_get_cm_event(conn_ctx.cm_channel, &event)) {
		fprintf(stderr, "rdma_get_cm_events failed\n");
		return FAILURE;
	}

	if (event->event != RDMA_CM_EVENT_CONNECT_REQUEST) {
		fprintf(stderr, "error RDMA_CM_EVENT on server connect %d\n", event->event);
		return FAILURE;
	}

	conn_ctx.cm_id = event->id;
	conn_ctx.context = conn_ctx.cm_id->verbs;

	/* retrieve the first pending event, should be
    RDMA_CM_EVENT_CONNECT_REQUEST to build connection */
  if (on_connect_request(event->id)) {
    return FAILURE;
  }
  rdma_ack_cm_event(event);

  /* working on cm_event */
  fprintf(stdout, "working on cm_event\n");
  while (rdma_get_cm_event(conn_ctx.cm_channel, &event) == 0) {
    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);

    if (on_event(&event_copy))
      break;
  }

	rdma_destroy_id(conn_ctx.cm_id_control);
  rdma_destroy_event_channel(conn_ctx.cm_channel);

  return SUCCESS;
}

void RDMAServer::stop() {
  rdma_disconnect(conn_ctx.cm_id);
}


/********** client **********/
RDMAClient::RDMAClient(EnvironmentProc *env) {
  /* set parameters */
  conn_params.env = env->get_params();
  assert(conn_params.env->machine == CLIENT);
  conn_params.connected = false;
  conn_params.num_of_qps = 1;
  conn_params.wr_cq_number = 128;
}


int16_t RDMAClient::on_addr_resolved(rdma_cm_id *id) {
  assert(conn_params.env->machine == CLIENT);

  build_connection(id);

  if (rdma_resolve_route(id, RDMA_TIMEOUT_IN_MS)) {
    fprintf(stderr, "rdma_resolve_route failed\n");
    return FAILURE;
  }
  return SUCCESS;
}

int16_t RDMAClient::on_route_resolved(rdma_cm_id *id) {
  rdma_conn_param cm_params;

  memset(&cm_params, 0, sizeof(cm_params));
  cm_params.initiator_depth = 1;
  cm_params.responder_resources = 1;
  cm_params.retry_count = 7;
  cm_params.rnr_retry_count = 7; /* infinite retry */

  if (rdma_connect(id, &cm_params)) {
    return FAILURE;
  }

  return SUCCESS;
}

int16_t RDMAClient::run() {
  /* create channel */
  rdma_cm_id **cm_id = &conn_ctx.cm_id;
  conn_ctx.cm_channel = rdma_create_event_channel();
  if (conn_ctx.cm_channel == NULL) {
    return FAILURE;
  }
  if (rdma_create_id(conn_ctx.cm_channel, cm_id, NULL, RDMA_PS_TCP)) {
    return FAILURE;
  }

	char *service;
	int num_of_retry= NUM_OF_RETRIES;
	sockaddr_in sin, source_sin;
	sockaddr *source_ptr = NULL;
	addrinfo *res;
	rdma_cm_event *event;
  rdma_cm_event event_copy;
	rdma_conn_param conn_param;
	addrinfo hints;

	memset(&hints, 0, sizeof hints);
	hints.ai_family   = AF_INET;
	hints.ai_socktype = SOCK_STREAM;

	if (check_add_port(&service, conn_params.env->server_port,
                     conn_params.env->server_ip, &hints, &res)) {
		fprintf(stderr, "Problem in resolving basic address and port\n");
		return FAILURE;
	}
	if (res->ai_family != PF_INET) {
		return FAILURE;
	}

	memcpy(&sin, res->ai_addr, sizeof(sin));
	sin.sin_port = htons((unsigned short)conn_params.env->server_port);

	while (1) {
		if (num_of_retry == 0) {
			fprintf(stderr, "Received %d times ADDR_ERROR\n", NUM_OF_RETRIES);
			return FAILURE;
		}

    /* TIMEOUT_IN_MS */
		if (rdma_resolve_addr(conn_ctx.cm_id, source_ptr, (struct sockaddr *)&sin, RDMA_TIMEOUT_IN_MS)) {
			fprintf(stderr, "rdma_resolve_addr failed\n");
			return FAILURE;
		}

		if (rdma_get_cm_event(conn_ctx.cm_channel, &event)) {
			fprintf(stderr, "rdma_get_cm_events failed\n");
			return FAILURE;
		}

		if (event->event == RDMA_CM_EVENT_ADDR_ERROR) {
			num_of_retry--;
			rdma_ack_cm_event(event);
			continue;
		}

		if (event->event != RDMA_CM_EVENT_ADDR_RESOLVED) {
			rdma_ack_cm_event(event);
			return FAILURE;
		}

    conn_ctx.context = conn_ctx.cm_id->verbs;

    /* first event should be RDMA_CM_EVENT_ADDR_RESOLVED */
    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);
    if (on_addr_resolved(event_copy.id)) {
      return FAILURE;
    }

    break;
	}

	while (1) {
		if (num_of_retry <= 0) {
			fprintf(stderr, "Received %d times ADDR_ERROR - aborting\n",NUM_OF_RETRIES);
			return FAILURE;
		}

		if (rdma_get_cm_event(conn_ctx.cm_channel, &event)) {
			fprintf(stderr, "rdma_get_cm_events failed\n");
			return FAILURE;
		}

		if (event->event == RDMA_CM_EVENT_ROUTE_ERROR) {
			num_of_retry--;
			rdma_ack_cm_event(event);
			continue;
		}

		if (event->event != RDMA_CM_EVENT_ROUTE_RESOLVED) {
			fprintf(stderr, "unexpected CM event %d\n",event->event);
			rdma_ack_cm_event(event);
			return FAILURE;
		}

    /* second event should be RDMA_CM_EVENT_ROUTE_RESOLVED */
    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);
    if (on_route_resolved(event_copy.id)) {
      return FAILURE;
    }

		break;
	}

  /* working on cm_event */
  fprintf(stdout, "working on cm_event\n");
  while (rdma_get_cm_event(conn_ctx.cm_channel, &event) == 0) {
      memcpy(&event_copy, event, sizeof(*event));
      rdma_ack_cm_event(event);

      if (on_event(&event_copy))
        break;
  }

  rdma_destroy_event_channel(conn_ctx.cm_channel);

  return SUCCESS;
}

void RDMAClient::stop() {
  rdma_disconnect(conn_ctx.cm_id);
}

int16_t RDMAClient::on_connection(rdma_cm_id *id) {
  conn_params.connected = true;

  /* TODO mzy: init & send meta on connection */

  post_meta_send_wr();
  return SUCCESS;
}

int16_t RDMAClient::on_disconnect(rdma_cm_id *id) {
  assert (id->context == static_cast<void *>(this));

  destroy_connection();

  /* return FAILURE to break waitting */
  return FAILURE;
}

int16_t RDMAClient::on_event(rdma_cm_event *event) {
  int ret = SUCCESS;

  if (event->event == RDMA_CM_EVENT_ADDR_RESOLVED)
      ret = on_addr_resolved(event->id);
  else if (event->event == RDMA_CM_EVENT_ROUTE_RESOLVED)
      ret = on_route_resolved(event->id);
  else if (event->event == RDMA_CM_EVENT_ESTABLISHED)
      ret = on_connection(event->id);
  else if (event->event == RDMA_CM_EVENT_DISCONNECTED)
      ret = on_disconnect(event->id);
  else {
    ret = FAILURE;
  }

  return ret;
}
