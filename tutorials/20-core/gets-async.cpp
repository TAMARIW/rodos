#include "rodos.h"

static Application module01("Testgetchar");

class TestGets : public StaticThread<> {
  public:
    TestGets() : StaticThread<>("testgetchar") {}
    void run() {
        char* s;
        PRINTF("Please type string of characters. Run at least 40 seconds\n");

        activateTopicCharInput(); //<........ THIS IS IT!!!!

        TIME_LOOP(30 * SECONDS, 1 * SECONDS) {
            PRINTF("I call getsnowait -> topic deactivated! \n");
            if((s = getsNoWait()) != 0) { PRINTF("\n\n********* String: %s'%s'%s **********\n", SCREEN_GREEN, s, SCREEN_RESET); }
        }
    }
} testGets;

/******************************/

class CharReceiver : public Subscriber {
  public:
    CharReceiver() : Subscriber(charInput, "CharReceiver") {}

    void putFromInterrupt([[gnu::unused]] const long topicId, const void* data, [[gnu::unused]] int len) {
        GenericMsgRef* msg       = (GenericMsgRef*)data;
        msg->msgPtr[msg->msgLen] = 0;
        xprintf("\n Async: %d %s\n", (int)msg->msgLen, msg->msgPtr); // no PRINTF in interrupts (Sempahore)
    }
} charReceiver;
