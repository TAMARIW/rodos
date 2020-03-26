#include "rodos.h"

void MAIN() {
    PRINTF("please write some lines\n");
    activateTopicCharInput(); //<........ THIS IS IT!!!!
}


class CharReceiver : public Subscriber {
  public:
    CharReceiver() : Subscriber(charInput, "CharReceiver") {}

    void putFromInterrupt([[gnu::unused]] const long topicId, const void* data, [[gnu::unused]] int len) {
        GenericMsgRef* msg = (GenericMsgRef*)data;
        xprintf("\n Async: %d %s\n", (int)msg->msgLen, msg->msgPtr); // no PRINTF in interrupts (Sempahore)
    }
} charReceiver;
