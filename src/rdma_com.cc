#include "rdma_com.h"

RDMA_Server::RDMA_Server(Environment_Proc *env){
  /* set parameters */
  env_basic_param *params = env->get_params();
  conn_params.servername = params->servername;
  conn_params.port = params->port;
  conn_params.connection_type = params->connection_type;
  conn_params.machine = params->machine;
  assert(conn_params.machine == SERVER);
  conn_params.connected = false;
  conn_params.num_of_qps = 1;
  conn_params.wr_cq_number = 128;
}

int16_t RDMA_Server::on_connect_request(rdma_cm_id *id) {
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

int16_t RDMA_Server::on_connection(rdma_cm_id *id) {
  conn_params.connected = true;

  /* TODO zheyu: init & send meta on connection */

  post_meta_send_wqe();
  return SUCCESS;
}

int16_t RDMA_Server::on_disconnect(rdma_cm_id *id) {
  destroy_connection();
  return FAILURE;
}

int16_t RDMA_Server::on_event(rdma_cm_event *event) {
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

int16_t RDMA_Server::run() {
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
	if (check_add_port(&service, conn_params.port, src_ip, &hints, &res)) {
		fprintf(stderr, "Problem in resolving basic address and port\n");
		return FAILURE;
	}

	if (res->ai_family != PF_INET) {
		return FAILURE;
	}
	memcpy(&sin, res->ai_addr, sizeof(sin));
	sin.sin_port = htons((unsigned short)conn_params.port);

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

	conn_ctx.cm_id = (rdma_cm_id *)event->id;
	conn_ctx.context = conn_ctx.cm_id->verbs;

	/* retrieve the first pending event, should be
    RDMA_CM_EVENT_CONNECT_REQUEST to build connection */
  if (on_connect_request(event->id)) {
    return FAILURE;
  }
  rdma_ack_cm_event(event);

  /* working on cm_event */
  while (rdma_get_cm_event(conn_ctx.cm_channel, &event) == 0) {
    memcpy(&event_copy, event, sizeof(*event));
    rdma_ack_cm_event(event);

    if (on_event(&event_copy))  // TODO mzy: finish all
      break;
  }

	rdma_destroy_id(conn_ctx.cm_id_control);
	freeaddrinfo(res);

  return SUCCESS;
}

void RDMA_Server::stop() {
    
}



/********** client **********/
RDMA_Client::RDMA_Client(Environment_Proc *env) {
  /* set parameters */
  env_basic_param *params = env->get_params();
  conn_params.servername = params->servername;
  conn_params.port = params->port;
  conn_params.connection_type = params->connection_type;
  conn_params.machine = params->machine;
  assert(conn_params.machine == CLIENT);
  conn_params.connected = false;
  conn_params.num_of_qps = 1;
  conn_params.wr_cq_number = 128;
  conn_params.source_ip = params->source_ip;
}


int16_t RDMA_Client::on_addr_resolved(rdma_cm_id *id) {
  assert(conn_params.machine == CLIENT);

  build_connection(id);

  if (rdma_resolve_route(id, RDMA_TIMEOUT_IN_MS)) {
    return FAILURE;
  }
  return SUCCESS;
}

int16_t RDMA_Client::on_route_resolved(rdma_cm_id *id) {
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

int16_t RDMA_Client::run() {
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

	if (check_add_port(&service, conn_params.port, conn_params.servername, &hints, &res)) {
		fprintf(stderr, "Problem in resolving basic address and port\n");
		return FAILURE;
	}
	if (res->ai_family != PF_INET) {
		return FAILURE;
	}

	memcpy(&sin, res->ai_addr, sizeof(sin));
	sin.sin_port = htons((unsigned short)conn_params.port);

	if (check_add_port(&service, 0x0, conn_params.source_ip, &hints, &res)) {
    fprintf(stderr, "Problem in resolving basic address and port\n");
    return FAILURE;
	}
  memset(&source_sin, 0x0, sizeof(source_sin));
  memcpy(&source_sin, res->ai_addr, sizeof(source_sin));
  source_ptr = (struct sockaddr *)&source_sin;

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
    rdma_ack_cm_event(event);

    break;
	}

	while (1) {
		if (num_of_retry <= 0) {
			fprintf(stderr, "Received %d times ADDR_ERROR - aborting\n",NUM_OF_RETRIES);
			return FAILURE;
		}

		if (rdma_resolve_route(conn_ctx.cm_id, RDMA_TIMEOUT_IN_MS)) {
			fprintf(stderr, "rdma_resolve_route failed\n");
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
  while (rdma_get_cm_event(conn_ctx.cm_channel, &event) == 0) {
      memcpy(&event_copy, event, sizeof(*event));
      rdma_ack_cm_event(event);

      if (on_event(&event_copy))  // TODO mzy: finish all
        break;
  }

  rdma_destroy_event_channel(conn_ctx.cm_channel);

  return SUCCESS;
}

void RDMA_Client::stop() {

}

int16_t RDMA_Client::on_connection(rdma_cm_id *id) {
  conn_params.connected = true;

  /* TODO zheyu: init & send meta on connection */

  post_meta_send_wqe();
  return SUCCESS;
}

int16_t RDMA_Client::on_disconnect(rdma_cm_id *id) {
  destroy_connection();
  /* return FAILURE to break waitting */
  return FAILURE;
}

int16_t RDMA_Client::on_event(rdma_cm_event *event) {
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