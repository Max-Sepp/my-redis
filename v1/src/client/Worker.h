#ifndef MY_REDIS_SENDER_H
#define MY_REDIS_SENDER_H

void SenderWorker(int sock);
void ReceiverWorker(int sock);

#endif  // MY_REDIS_SENDER_H
