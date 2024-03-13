#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"
#include "net/linkaddr.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <stdio.h> 
#include "sys/node-id.h"
#include "net/mac/tsch/tsch.h"
#include "sys/energest.h"
#include "sys/log.h"
#define LOG_MODULE "Node"
#define LOG_LEVEL LOG_LEVEL_INFO

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"

#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678
// #define ENERGEST_CONF_CURRENT_TIME clock_time
// #define ENERGEST_CONF_TIME_T clock_time_t
// #define ENERGEST_CONF_SECOND CLOCK_SECOND
#define SEND_INTERVAL       (10 * CLOCK_SECOND)
#define SEND_TIME       (random_rand() % (SEND_INTERVAL))


struct NodeLocation {
  double x;
  double y;
};

static struct NodeLocation nodeLocations[] = {
  {-7.50, 0.00},
  {7.50, 0.00},
  {0.00, 10.00},
  {0.00, -10.00}
  
};
// static unsigned long to_seconds(uint64_t time)
// {
//   return (unsigned long)(time / ENERGEST_SECOND);
// }

static struct simple_udp_connection broadcast_connection;

PROCESS(broadcast_example_process, "Client");
AUTOSTART_PROCESSES(&broadcast_example_process);


PROCESS_THREAD(broadcast_example_process, ev, data)
{

  static struct etimer periodic_timer;
  char message[32];
  uip_ipaddr_t addr; //dest
  static int tx_count;
  int is_coordinator;
  
  struct NodeLocation nodeLocation = nodeLocations[node_id-1];
  double node_x = nodeLocation.x;
  double node_y = nodeLocation.y;

  PROCESS_BEGIN();
 
  is_coordinator = 0;
  
  #if CONTIKI_TARGET_COOJA || CONTIKI_TARGET_SKY
    is_coordinator = (node_id == 1);
  #endif
 
  
  if(is_coordinator) {  /* Running on the root? */
    NETSTACK_ROUTING.root_start();
  }
  NETSTACK_MAC.on();
  
  /* Initialize UDP connection */
  simple_udp_register(&broadcast_connection, UDP_CLIENT_PORT, NULL, UDP_SERVER_PORT, NULL);
  
  etimer_set(&periodic_timer, CLOCK_SECOND * 10);
  
  while(1) { 
    LOG_INFO("X: %lf", node_x);
   PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));       
    if(NETSTACK_ROUTING.node_is_reachable() && NETSTACK_ROUTING.get_root_ipaddr(&addr)) {
      printf("Sending broadcast %d\n", tx_count);
      snprintf(message,sizeof(message), "Node ID:%d X:%lf Y:%lf", node_id, node_x, node_y);
      uip_create_linklocal_allnodes_mcast(&addr);
      uip_ip6addr(&addr, 0xfe80, 0x0, 0x0, 0x0, 0x205, 0x5, 0x5, 0x5);
      simple_udp_sendto(&broadcast_connection, message, strlen(message),&addr);
      tx_count++;
    } else {
      LOG_INFO("Not reachable yet\n");
    }
    
//      energest_flush();

//    printf(" CPU          %4lu LPM   %4lu \n DEEP LPM %4lu \n  Total time %lu\n",
//        (unsigned long)to_seconds(energest_type_time(ENERGEST_TYPE_CPU)),
//        (unsigned long)to_seconds(energest_type_time(ENERGEST_TYPE_LPM)),
//        (unsigned long)to_seconds(energest_type_time(ENERGEST_TYPE_DEEP_LPM)),
//        (unsigned long)to_seconds(ENERGEST_GET_TOTAL_TIME()-0));
// printf(" Radio LISTEN %4lu \n TRANSMIT %4lu \n OFF      %4lu\n",
//        (unsigned long)to_seconds(energest_type_time(ENERGEST_TYPE_LISTEN)),
//        (unsigned long)to_seconds(energest_type_time(ENERGEST_TYPE_TRANSMIT)),
//        (unsigned long)to_seconds(ENERGEST_GET_TOTAL_TIME()
//                               - energest_type_time(ENERGEST_TYPE_TRANSMIT)
//                               - energest_type_time(ENERGEST_TYPE_LISTEN)));
    etimer_reset(&periodic_timer);
  }
  PROCESS_END();
}
/*---------------------------------------------------------------------------*/












