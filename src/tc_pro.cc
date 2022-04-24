#include <thread>

#include "rdma_com.h"

void rdma_conn_thread(ConnectionProc *conn) {
  conn->set_alive(true);
  if (conn->is_server()) {
    static_cast<RDMAServer *>(conn)->run();
  } else {
    static_cast<RDMAClient *>(conn)->run();
  }
  conn->set_alive(false);
}

// TODO mzy: extract necessary function from libperf and get rid of if
int main(int argc, char *argv[]) {
  EnvironmentProc    user_env;
  if (user_env.parse_params(argc, argv)) {
    std::cerr << "Can't parse input parameters!" << std::endl;
  return 1;
}
  if (user_env.check_env()) {
    std::cerr << "Checking enviroment fail!" << std::endl;
  return 1;
}

  ConnectionProc *conn = nullptr;
  if (user_env.is_server()) {
    conn = (RDMAServer *) malloc(sizeof(RDMAServer));
    RDMAServer *server = new (conn) RDMAServer(&user_env);
  } else {
    conn = (RDMAClient *) malloc(sizeof(RDMAClient));
    RDMAClient *server = new (conn) RDMAClient(&user_env);
  }
  std::thread tc(rdma_conn_thread, conn);
  tc.detach();

  sleep(1);

  while (conn->is_alive()) {}

  return 0;
}

