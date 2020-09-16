/*
 * gpio.c:
 * 
 *
 */

#include <stdio.h>
#include <string.h>
#include <wiringPi.h>
#include "gpio.h"

char msg[255];

#define FOREACH_STATE(STATE) \
        STATE(OPENING)   \
        STATE(CLOSING)  \
        STATE(OPEN)   \
        STATE(CLOSED)  \

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

enum STATE_ENUM {
    FOREACH_STATE(GENERATE_ENUM)
};

static const char *STATE_STRING[] = {
    FOREACH_STATE(GENERATE_STRING)
};
/* lower sensor is the limit sensor (reed switch at lower part of door) that is set to 0 when door is closed
  and set to 1 when the door is not completely closed (could be open, opening or closing)

   Switch should be NC such that a contact causes open circuit. A value of 1 indicates the circuit is closed
   and happens when the magenetic part is not near the lower switch. i.e., door is not closed.

   upper sensor is the limit sensor (reed switch at upper part of door) that is set to 0 when door is completely
   open and will be 1 when the door is not open.

*/
enum garage_door_event_codes { upper_sensor_separated, upper_sensor_actuated, lower_sensor_separated, lower_sensor_actuated};

int __main (void) {
  
 initGarageDoor(NULL, NULL, NULL, NULL);
 for (;;) {
    delay(100);
    if(strlen(msg))
      printf("%s \n", msg);
    msg[0] = 0;
 }
 return 0;
} 

enum garage_door_state_codes current_state = closed;

void (*g_door_opening) (void) = NULL;
void (*g_door_closing) (void) = NULL;
void (*g_door_open) (void) = NULL;
void (*g_door_closed) (void) = NULL;

int initGarageDoor (void (*opening) (void), void (*closing) (void), void (*open) (void), void (*closed) (void))
{
  printf ("Setup garage door pins\n") ;
    
  if (wiringPiSetup () == -1)
    return 1 ;

  g_door_closed = closed;
  g_door_closing = closing;
  g_door_opening = opening;
  g_door_open = open;

  msg[0] = 0;
  pinMode (LOWER_SENSOR_PIN, INPUT);
  pinMode (UPPER_SENSOR_PIN, INPUT);
  pullUpDnControl(LOWER_SENSOR_PIN, PUD_UP);
  pullUpDnControl(UPPER_SENSOR_PIN, PUD_UP);
  wiringPiISR(LOWER_SENSOR_PIN, INT_EDGE_RISING, process_lower_sensor);
  wiringPiISR(UPPER_SENSOR_PIN, INT_EDGE_RISING, process_upper_sensor);
  int lowerState = digitalRead(LOWER_SENSOR_PIN);
  int upperState = digitalRead(UPPER_SENSOR_PIN);
  current_state = getDoorState(lowerState, upperState);
  printf("Read lower sensor %d \n", lowerState);
  printf("Read upper sensor %d \n", upperState);
  return 0;
}

void process_lower_sensor() {
  int value = digitalRead(LOWER_SENSOR_PIN);
  
  if(value == 0) { // sensor actuated
    current_state = closed;
    if(g_door_closed)
      g_door_closed();
  } else {
    current_state = opening;
    if(g_door_opening)
      g_door_opening();
  }
  sprintf(msg, "LOWER SENSOR fired and is %d, the door is %s", value, STATE_STRING[current_state]);  
}

void process_upper_sensor() {
  int value = digitalRead(UPPER_SENSOR_PIN);
  if(value == 0) {
    current_state = open;
    if(g_door_open)
      g_door_open();
  } else {
    current_state = closing;
    if(g_door_closing)
      g_door_closing();
  }
  sprintf(msg, "UPPER SENSOR fired and is %d, the door is %s", value, STATE_STRING[current_state]);
}

enum garage_door_state_codes getDoorState(int lowerSensorState, int upperSensorState) {

  if(lowerSensorState == 0) {
    return closed;
  }
  if(upperSensorState == 0) {
    return open;
  }
  return closed;
}
