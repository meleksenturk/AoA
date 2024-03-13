#include "contiki.h"
#include "net/routing/routing.h"
#include "random.h"
#include "net/netstack.h"
#include "sys/etimer.h"
#include "net/ipv6/uip.h"
#include "net/linkaddr.h"
#include "net/ipv6/uip-ds6.h"
#include "net/ipv6/simple-udp.h"
#include <stdint.h>
#include <inttypes.h>
#include "net/packetbuf.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h> 
#include "sys/energest.h"

#include <math.h>
#include "net/mac/tsch/tsch.h"
//--------------------------------------------
// #define ENERGEST_CONF_CURRENT_TIME clock_time
// #define ENERGEST_CONF_TIME_T clock_time_t
// #define ENERGEST_CONF_SECOND CLOCK_SECOND
#define SEND_INTERVAL       (10 * CLOCK_SECOND)
#define SEND_TIME       (random_rand() % (SEND_INTERVAL))
//----------------------------------------------------
#include "sys/log.h"
#define LOG_MODULE "UnknownNode"
#define LOG_LEVEL LOG_LEVEL_INFO

#define DEBUG DEBUG_PRINT
#include "net/ipv6/uip-debug.h"
#define WITH_SERVER_REPLY  1
#define UDP_CLIENT_PORT	8765
#define UDP_SERVER_PORT	5678

#define A_REF -45.0 // Reference signal strength in dBm at 1m distance
#define N_FACTOR  2.0 // Signal propagation factor (varies depending on the environment)
#define M_PI 3.14159265358979323846
double bC = 0.0;

struct Anchors {
    uint8_t node_id;
    double x;
    double y;
    double rssi;
    double distance;
    struct Anchors* next;
};

struct solutionArray{
    double x;
    double y;
};
struct Anchors anchorList;
struct solutionArray result;
static struct simple_udp_connection broadcast_connection;

double firstRegion, secondRegion, thirdRegion, fourthRegion = 0;

struct Anchors* createAnchor(uint8_t node_id, double x, double y, double rssi, double distance, struct Anchors* anchorList) {
    struct Anchors* newNode = (struct Anchors*)malloc(sizeof(struct Anchors));

    if (newNode == NULL) {
        LOG_INFO("Bellek tahsis hatasi");
        exit(1);
    }

    newNode->node_id = node_id;
    newNode->x = x;
    newNode->y = y;
    newNode->rssi = rssi;
    newNode->distance = distance;
    newNode->next = NULL;

    if (anchorList == NULL || node_id < anchorList->node_id) {
        newNode->next = anchorList;
        return newNode;
    }

    struct Anchors* prev = NULL;
    struct Anchors* current = anchorList;


    while (current != NULL && node_id > current->node_id) {
        prev = current;
        current = current->next;
    }

    if (current != NULL && node_id == current->node_id) {
        current->x = x;
        current->y = y;
        current->rssi = rssi;
        current->distance = distance;
        free(newNode);
    } else {
        if (prev != NULL) {
            prev->next = newNode;
            newNode->next = current;
        }
    }

    if (current == NULL) {
        prev->next = newNode;
    }

    

    return anchorList;
}



static double calculate_distance(double rssi)
{
    double distance = powf(10, (-1 * ((rssi + 86.90876) / 93.6264) + log10(50)));
    return distance;
}

static int calculate_circle(double distanceBetwCenter, double node1Dist, double node2Dist){
    double sumOfDistances = node1Dist + node2Dist;

    if(distanceBetwCenter > sumOfDistances){ 
        // LOG_INFO("Cemberler kesismezler ve bir cember digerinin icinde degildir.\n");
        return 1;
    } else if(distanceBetwCenter == sumOfDistances){
        // LOG_INFO("Cemberler distan tegettir.\n");
        return 2;
    } else if(distanceBetwCenter == fabs(node1Dist - node2Dist)){
        // LOG_INFO("Cemberler icten tegettir.\n");
        return 3;
    } else if(distanceBetwCenter < fabs(node1Dist - node2Dist)){
        // LOG_INFO("Cemberler kesismez ve bir cember digerinin icindedir.\n");
        return 4;
    } else if(distanceBetwCenter < sumOfDistances && distanceBetwCenter > fabs(node1Dist - node2Dist)){
        // LOG_INFO("Cemberler iki noktada kesisirler.");
        return 5;
    }
    return -1; // Hata durumu
}
static double calculateAngle(double node1_distance, double node2_distance,double b_rssi, double c_rssi,double d_rssi,double f_rssi){
    double angleE, x, cosE = 0.0;

    if(d_rssi==f_rssi){
        if(c_rssi>b_rssi){
            LOG_INFO("Birinci bolge..");
            angleE = 0;
        }else if(b_rssi>c_rssi){
            LOG_INFO("Ikinci bolge..");
            angleE = 180;
        }
    }else{
         x = sqrt((powf(node1_distance, 2) + powf(node2_distance, 2) - (powf(bC, 2)/2.0)) / 2.0);
    }

            LOG_INFO("Yaricap: %f\n", x);
     
    if(x != 0.0){
        int ucgenMi = 0;
    
        if(firstRegion || fourthRegion){
            angleE = 0.0;

            
            ucgenMi = 0;
            LOG_INFO("Yaricap: %f\n", x);
       
            if ((bC/2.0 + node1_distance > x && fabs((bC/2.0)-node1_distance)<x) && (bC/2.0 + x > node1_distance && fabs((bC/2.0)-x)<node1_distance) && (node1_distance + x > bC/2.0 && fabs(node1_distance-x)<bC/2.0)) {
                // printf("Bu uzunluklarla bir ucgen olusturulabilir.\n");
                ucgenMi= 1;
            } else {
                printf("Bu uzunluklarla bir ucgen olusturulamaz.\n");
                ucgenMi = 0;
            }
            // LOG_INFO("Kenar1:%f\n",bC/2.0);
            // LOG_INFO("Kenar2:%f\n",node1_distance);
            // LOG_INFO("Kenar3:%f\n",x);
            if(ucgenMi){
                cosE = ((x*x) + ((bC / 2.0)*(bC/2.0) ) - (node2_distance *node2_distance)) / (2.0 * x * (bC/2.0));
                // LOG_INFO("cosE: %f", cosE);
                int newCosE = cosE * 1000000.0;
                double cosEnew = (double)newCosE / 1000000.0;
                angleE = acos(cosEnew) * (180.0 / M_PI);
                // LOG_INFO("cosE: %f acos: %f\n", cosEnew, acos(cosEnew));
                if(d_rssi>f_rssi){
                    angleE = 360-angleE;
                }else{
                    angleE = angleE;
                }
            }
        }else if(secondRegion || thirdRegion){
            angleE = 0.0;
            ucgenMi = 0;
            LOG_INFO("X: %f\n", x);
        
            if ((bC/2.0 + node1_distance > x && fabs((bC/2.0)-node1_distance)<x) && (bC/2.0 + x > node1_distance && fabs((bC/2.0)-x)<node1_distance) && (node1_distance + x > bC/2.0 && fabs(node1_distance-x)<bC/2.0)) {
                // printf("Bu uzunluklarla bir ucgen olusturulabilir.\n");
                ucgenMi= 1;
            }else {
                // printf("Bu uzunluklarla bir ucgen olusturulamaz.\n");
                ucgenMi = 0;
            }
            // LOG_INFO("Kenar1:%f",bC/2.0);
            // LOG_INFO("Kenar2:%f",node1_distance);
            // LOG_INFO("Kenar3:%f",x);
            if(ucgenMi){
                cosE = ((x*x) + ((bC / 2.0)*(bC/2.0) ) - (node1_distance *node1_distance)) / (2.0 * x * (bC/2.0));
                // LOG_INFO("cosE: %f", cosE);
                int newCosE = cosE * 1000000.0;
                double cosEnew = (double)newCosE / 1000000.0;
                angleE = acos(cosEnew) * (180.0 / M_PI);
                // LOG_INFO("cosE: %f acos: %f\n", cosEnew, acos(cosEnew));
                if(d_rssi>f_rssi){
                    angleE = angleE + 180;
                }else if(f_rssi>d_rssi){
                    angleE = 180-angleE ;
                }
            }
        }
    }
    LOG_INFO("Angle: %f\n",angleE);
    // LOG_INFO("KonumX: %f KonumY: %f\n", result.x, result.y);

    return angleE;
}
static void solutionArray(struct Anchors* anchorList){
    double node1_x, node1_y,node1_distance, node2_x, node2_y, node2_distance, d_rssi,b_rssi,c_rssi, f_rssi= 0;
    struct Anchors* current = anchorList;

    while (current != NULL) {
        if (current->node_id == 1) {
            // LOG_INFO("CurrenNodeId %d",current->node_id);
            node1_x = current->x;
            node1_y = current->y;
            node1_distance = current->distance;
            b_rssi=current->rssi;
            LOG_INFO("BRSSI: %f", b_rssi);
            LOG_INFO("BDISTANCE: %f\n",node1_distance);
        } else if (current->node_id == 2) {
            // LOG_INFO("CurrenNodeId %d",current->node_id);
            node2_x = current->x;
            node2_y = current->y;
            node2_distance = current->distance;
            c_rssi= current->rssi;
            LOG_INFO("CRSSI: %f", c_rssi);
            LOG_INFO("CDISTANCE: %f\n",node2_distance);

        }else if(current->node_id == 3){
            LOG_INFO("CurrenNodeId %d",current->node_id);

            d_rssi = current->rssi;
            LOG_INFO("DRSSI: %f\n", d_rssi);


        }else if(current->node_id == 4){
            LOG_INFO("CurrenNodeId4 %d",current->node_id);
             f_rssi = current->rssi;
            LOG_INFO("FRSSI: %f\n", f_rssi);

        }
        current = current->next;  
    }
     bC = fabs(node2_x - node1_x);


    double resultCircle = calculate_circle(bC,node1_distance,node2_distance);
    
    

        
        if(resultCircle ){
            double a = fmin(node1_distance + node2_distance, fmax(node1_distance - node2_distance, (pow(node1_distance, 2) - pow(node2_distance, 2) + pow(bC, 2)) / (2 * bC)));
        double a_squared = pow(node1_distance, 2) - pow(a, 2);
        double h = (a_squared >= 0) ? sqrt(a_squared) : 0;
        // double h = sqrt(pow(node1_distance, 2) - pow(a, 2));
        double x3 = node1_x + a * (node2_x - node1_x) / bC;
        double y3 = node1_y + a * (node2_y - node1_y) / bC;

        double intersection1_x = x3 + h * (node2_y - node1_y) / bC;
        double intersection1_y = y3 - h * (node2_x - node1_x) / bC;

        double intersection2_x = x3 - h * (node2_y - node1_y) / bC;
        double intersection2_y = y3 + h * (node2_x - node1_x) / bC;

        // Kesişim noktalarını yazdır
        printf("Kesisim Noktalari:\n");
        printf("Nokta 1: (%lf, %lf)\n", intersection1_x, intersection1_y);
        printf("Nokta 2: (%lf, %lf)\n", intersection2_x, intersection2_y);
       


        if(c_rssi > b_rssi && f_rssi>d_rssi){
            // LOG_INFO("Birinci bolgedeyim...");
            firstRegion= 0;
            secondRegion=0;
             thirdRegion=0; 
             fourthRegion = 0;
            result.x = intersection1_x;
            result.y = intersection1_y;
            firstRegion = 1;
        }else if( b_rssi> c_rssi && f_rssi>d_rssi){
            // LOG_INFO("Ikinci bolgedeyim...");
            firstRegion= 0;
            secondRegion=0;
             thirdRegion=0; 
             fourthRegion = 0;
            result.x = intersection1_x;
            result.y = intersection1_y;

            secondRegion = 1;
        }else if(b_rssi>c_rssi && d_rssi>f_rssi){
            firstRegion= 0;
            secondRegion=0;
             thirdRegion=0; 
             fourthRegion = 0;
            // LOG_INFO("Ucuncu bolgedeyim...");
            result.x = intersection2_x;
            result.y = intersection2_y;
            thirdRegion = 1;
        }else if(c_rssi>b_rssi && d_rssi>f_rssi){
            firstRegion= 0;
            secondRegion=0;
             thirdRegion=0; 
             fourthRegion = 0;
            // LOG_INFO("Dorduncu bolgedeyim...");
            result.x = intersection2_x;
            result.y = intersection2_y;
            fourthRegion = 1;
        }

        }
        // double h_dist=0.0;
        // if(c_rssi > d_rssi){
        //      h_dist = fabs(result.y- node2_y);
        // }else{
        //     h_dist =fabs(result.y-node1_y);
        // }
        // LOG_INFO("Hdist:%f",h_dist);
    
        calculateAngle(node1_distance,node2_distance, b_rssi, c_rssi, d_rssi,f_rssi);
    
        

    
}

PROCESS(broadcast_example_process, "Server");
AUTOSTART_PROCESSES(&broadcast_example_process);

static void receiver(struct simple_udp_connection *c,
                    const uip_ipaddr_t *sender_addr,
                    uint16_t sender_port,
                    const uip_ipaddr_t *receiver_addr,
                    uint16_t receiver_port,
                    const uint8_t *data,
                    uint16_t datalen)
{




    LOG_INFO("Paket alindi.\n");
    char buffer[100];  // Veriyi işlemek için bir tampon
    LOG_INFO("Gelen Veri: %s\n", buffer);

    if (datalen >= sizeof(buffer)) {
        LOG_INFO("Alınan veri çok büyük\n");
        return;
    }

    memcpy(buffer, data, datalen);
    buffer[datalen] = '\0'; 

    int node_id = 0;
    double node_x = 0;
    double node_y = 0;
    // double b_c_distance = 0;

    if (sscanf(buffer, "Node ID: %d X: %lf Y: %lf ", &node_id, &node_x, &node_y) == 3) {
        LOG_INFO("Node ID: %d\n", node_id);

        LOG_INFO("NodeX: %f\n", node_x);
        LOG_INFO("NodeY: %f\n", node_y);

    } else {
        LOG_INFO("Veri ayrıştırma hatası\n");
        return;  
    }
    


    uint8_t hop_count = uip_ds6_if.cur_hop_limit - UIP_IP_BUF->ttl;
    int16_t channel = packetbuf_attr(PACKETBUF_ATTR_CHANNEL);
    LOG_INFO("Channel: %d\n", channel);
    int16_t lqi = packetbuf_attr( PACKETBUF_ATTR_LINK_QUALITY);
    LOG_INFO("LQI: %d\n",lqi);
    // LOG_INFO("CurHop: %d",uip_ds6_if.cur_hop_limit);
    // LOG_INFO("TTL: %d", UIP_IP_BUF->ttl);
    // LOG_INFO("Hop: %d NodeId: %d", hop_count, node_id);
    if (hop_count == 0) {
        int16_t rssi = packetbuf_attr(PACKETBUF_ATTR_RSSI);
        
        double d_rssi = rssi;
        // printf("RSSI: %f", d_rssi);
        double d_x = node_x;
        // LOG_INFO("X: %f", d_x);
        
        double d_y = node_y;
        // LOG_INFO("Y: %f", d_y);
        double distance = calculate_distance(d_rssi);
        // LOG_INFO("Distance: %f\n", distance);

        // printf("Distance: %f\n", distance);
        createAnchor(node_id, d_x, d_y, d_rssi, distance, &anchorList);
        solutionArray(&anchorList);
        // multilateration(&anchorList);
       
    }
}






// static unsigned long to_seconds(uint64_t time)
// {
//   return (unsigned long)(time / ENERGEST_SECOND);
// }

/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_example_process, ev, data)
{  static struct etimer periodic_timer;

  PROCESS_BEGIN();
  
  simple_udp_register(&broadcast_connection, UDP_SERVER_PORT, NULL, UDP_CLIENT_PORT, receiver);
  etimer_set(&periodic_timer, CLOCK_SECOND * 10);

  while(1) {
       PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));       


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

