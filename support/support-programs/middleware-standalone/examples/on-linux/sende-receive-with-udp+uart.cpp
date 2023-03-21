/*
 * main.cpp
 *
 *  Created on: 10.07.2013
 *      Author: Erik
 */

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "rodos-middleware.h"
#include <sys/time.h>
#include <unistd.h>
#include "../topics.h"

#define ON_LINUX //if not defined: stm32f4
#ifdef ON_LINUX

UDPInOut udp(udpPortNr);
LinkinterfaceUDP linkif(&udp);
static Gateway gw(&linkif, true);


static HAL_UART uart(UART_IDX4);

int64_t NOW() {
  struct timeval t;
  gettimeofday(&t, 0);
  return (t.tv_sec * 1000000LL + t.tv_usec) * 1000;
}

#else
#include "stm32f4xx.h"

static HAL_UART uart(UART_IDX3);
HAL_UART uart_stdout(UART_IDX2);

int64_t NOW() {
  static int64_t faketime = 0;
  return faketime++;
}

#endif

/*
  static HAL_CAN can(CAN_IDX1);
  static LinkinterfaceCAN linkif(&can);
  //static LinkinterfaceUART linkif(&uart);
  static Gateway gw(&linkif);
*/


int32_t timer = 0;

//__________________________________________________________________

class MessageHandler : public Putter {
  // Warning long, int for len, etc are deprecated, but we have to keep them for compatibility
  bool putGeneric(const long topicId,  const unsigned int len,  const void* msg,  const NetMsgInfo& ) override {
    MyTime* myTime = (MyTime*)msg;
    printf("Got topic %d, len %d : ", (int)topicId, (int)len);
    if(topicId == topicIdRodos2Linux ) {
      printf(" counter %d, time %lld\n", (int)myTime->msgIndex, (long long)myTime->timeNow);
    } else {
      printf(" got unexpected topic\n");
    }
    return true;
  }
} msgHandler;


int main() {
#ifndef ON_LINUX
  SystemCoreClockUpdate();
  uart_stdout.init(115200);
  // can.init(100000);
#endif
  uart.init(); // Warning! not used!!


  Position pos = {"main in Linux", 0, 0,0,0};

  gw.init(3);                // 3 ist my simulated nodeid
  gw.setPutter(&msgHandler); // ony one

  while (1) {
    gw.pollMessages();

    pos.cnt++;
    pos.x = (double)pos.cnt / 10;
    pos.y = (double)pos.cnt + 10;
    pos.z = (double)pos.cnt * 10;

    gw.sendNetworkMessage((char *)&pos, sizeof(Position), topicIdLinux2Rodos, NOW()); // WARNING! signature != from Rodos Gateway!
    printf("sending %d\n", (int)pos.cnt);
    usleep(500000);
  }
  return 0;
}







