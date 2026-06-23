#ifndef MYREDIS_CLIENT_WORKER_H_
#define MYREDIS_CLIENT_WORKER_H_

namespace myredis {

void SenderWorker(int sock);
void ReceiverWorker(int sock);

}  // namespace myredis

#endif  // MYREDIS_CLIENT_WORKER_H_
