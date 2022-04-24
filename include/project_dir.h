#ifndef __project_dir_h__
#define __project_dir_h__

#if defined(WIN32)
#define rw_export __declspec(dllexport)
#define rw_import __declspec(dllimport)
#define rw_local

#ifndef __win__
#define __win__
#endif
#else
#define rw_export __attribute__ ((visibility ("default")))
#define rw_import __attribute__ ((visibility ("default")))
#define rw_local  __attribute__ ((visibility ("hidden")))

#ifndef __linux__
#define __linux__
#endif
#endif

#define res_path "/root/tc_rdma_proc/data"

#endif
