#ifndef _HDTN_PATHS_H
#define _HDTN_PATHS_H
//addresses for ZMQ TCP transport
//#define HDTN_STORAGE_TELEM_PATH "tcp://0.0.0.0:10460"
//#define HDTN_RELEASE_TELEM_PATH "tcp://0.0.0.0:10461"
#define HDTN_STORAGE_TELEM_PATH "tcp://127.0.0.1:10460"
#define HDTN_RELEASE_TELEM_PATH "tcp://127.0.0.1:10461"

//push-pull from ingress to egress 
//#define HDTN_CUT_THROUGH_PATH "tcp://0.0.0.0:10100"
#define HDTN_BOUND_INGRESS_TO_CONNECTING_EGRESS_PATH "tcp://127.0.0.1:10100"
//push-pull from ingress to storage 
//#define HDTN_STORAGE_PATH "tcp://0.0.0.0:10110"
#define HDTN_BOUND_INGRESS_TO_CONNECTING_STORAGE_PATH "tcp://127.0.0.1:10110"
#define HDTN_CONNECTING_STORAGE_TO_BOUND_INGRESS_PATH "tcp://127.0.0.1:10150"
//push-pull from storage to release 
//#define HDTN_RELEASE_PATH "tcp://0.0.0.0:10120"
#define HDTN_CONNECTING_STORAGE_TO_BOUND_EGRESS_PATH "tcp://127.0.0.1:10120"
#define HDTN_BOUND_EGRESS_TO_CONNECTING_STORAGE_PATH "tcp://127.0.0.1:10130"
//req-reply to reg server
#define HDTN_REG_SERVER_PATH "tcp://127.0.0.1:10140"
//pub-sub from scheduler to modules
//#define HDTN_SCHEDULER_PATH "tcp://0.0.0.0:10200"
#define HDTN_BOUND_SCHEDULER_PUBSUB_PATH "tcp://127.0.0.1:10200"

//addresses for ZMQ IPC transport
#define HDTN_STORAGE_WORKER_PATH "inproc://hdtn3.storage.worker"
#endif