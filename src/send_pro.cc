#include "rdma_com.h"

// TODO mzy: extract necessary function from libperf and get rid of if
int main(int argc, char *argv[]) {
    Environment_Proc    user_env;
    if (user_env.parse_params(argc, argv)) {
        std::cerr << "Can't parse input parameters!" << std::endl;
		return 1;
	}
    if (user_env.check_env()) {
        std::cerr << "Checking enviroment fail!" << std::endl;
		return 1;
	}

    if (user_env.is_server()) {
        RDMA_Server rdma_server(&user_env);
        rdma_server.run();
    } else {
        RDMA_Client rdma_client(&user_env);
        rdma_client.run();
    }

    return 0;
}
