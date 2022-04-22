#include "rdma_com.h"


int main(int argc, char *argv[]) {
    Environment_Proc    user_env;
    //TODO: use perftest interface to parse parameters for convenience,
    //		we need to rewrite it...
    if (user_env.parse_params(argc, argv)) {
        std::cerr << "Can't parse input parameters!" << std::endl;
		return 1;
	}
    if (user_env.check_env()) {
        std::cerr << "Checking enviroment fail!" << std::endl;
		return 1;
	}

    RDMA_Server rdma_server(&user_env);
    rdma_server.run();
    



    return 0;
}
