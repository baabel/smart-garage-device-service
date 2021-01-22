/*
 * gpio.c:
 * 
 *
 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
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

int g_lower_state = -1;
int g_upper_state = -1;

void print_door_state(enum garage_door_state_codes state);
void lower_sensor_changed_cb();
void upper_sensor_changed_cb();

/* lower sensor is the limit sensor (reed switch at lower part of door) that is set to 0 when door is closed
  and set to 1 when the door is not completely closed (could be open, opening or closing)

   Switch should be NC such that a contact causes open circuit. A value of 1 indicates the circuit is closed
   and happens when the magenetic part is not near the lower switch. i.e., door is not closed.

   upper sensor is the limit sensor (reed switch at upper part of door) that is set to 0 when door is completely
   open and will be 1 when the door is not open.

*/
enum garage_door_event_codes { upper_sensor_separated, upper_sensor_actuated, lower_sensor_separated, lower_sensor_actuated};

int __main (void) {
  
 init_door(NULL, NULL, NULL, NULL);
 for (;;) {
    delay(100);
    if(strlen(msg))
      printf("%s \n", msg);
    msg[0] = 0;
 }
 return 0;
} 

enum garage_door_state_codes door_state = closed;

void (*g_door_opening) (void) = NULL;
void (*g_door_closing) (void) = NULL;
void (*g_door_open) (void) = NULL;
void (*g_door_closed) (void) = NULL;

void flash_actuator();

FILE* fp;


int init_door (void (*opening) (void), void (*closing) (void), void (*open) (void), void (*closed) (void))
{
  printf ("Setup garage door pins\n") ;
  fp = fopen("gpio.txt", "w");    
  if (wiringPiSetup () == -1)
    return 1 ;

  g_door_closed = closed;
  g_door_closing = closing;
  g_door_opening = opening;
  g_door_open = open;


  msg[0] = 0;
  pinMode (LOWER_SENSOR_PIN, INPUT);
  pinMode (UPPER_SENSOR_PIN, INPUT);
  pinMode (DOOR_ACTUATOR_BACKUP_PIN, OUTPUT);
  pinMode (DOOR_ACTUATOR_PIN, OUTPUT);
  
  // callbacks for sensor changes
  wiringPiISR(LOWER_SENSOR_PIN, INT_EDGE_BOTH, lower_sensor_changed_cb);
  wiringPiISR(UPPER_SENSOR_PIN, INT_EDGE_BOTH, upper_sensor_changed_cb);


  // activate the pi built-in pull up resistors - otherwise floating values
  pullUpDnControl(UPPER_SENSOR_PIN, PUD_UP);
  pullUpDnControl(LOWER_SENSOR_PIN, PUD_UP);
  
// read initial values
  g_lower_state = digitalRead(LOWER_SENSOR_PIN);
  g_upper_state = digitalRead(UPPER_SENSOR_PIN);

 
  flash_actuator();

  door_state = get_door_state(g_lower_state, g_lower_state);
  
  sprintf(msg, "Initial lower sensor state %d \n", g_lower_state);
  printf(msg);
  fputs(msg, fp);

  sprintf(msg, "Initial upper sensor state %d \n", g_upper_state);
  printf(msg);
  fputs(msg, fp);


  sprintf(msg, "Initial door state %s \n", STATE_STRING[door_state]);
  printf(msg);
  fputs(msg, fp);
  return 0;
}

/*
circuit is assumed to be NC, read value 1 means no magnet in contact with switch,
0 means magnet in contact with switch
*/
void lower_sensor_changed_cb() {

  int value = digitalRead(LOWER_SENSOR_PIN);

  if(value == 0) { // magnet in contact = closed door
    door_state = closed;
    if(value != g_lower_state && g_door_closed) {
      g_door_closed();
      print_door_state(door_state);
    }
   
  } else {
    if(value != g_lower_state) {
      door_state = opening;
      print_door_state(door_state);
      if(g_door_opening) {
        g_door_opening();
      }
    }
  }
  g_lower_state = value;    
  sprintf(msg, "\n\n------------------------------------------------------------------");
  fputs(msg, fp);
  sprintf(msg, " \n********** LOWER SENSOR fired and is %d, the door is %s", value, STATE_STRING[door_state]);  
  printf(msg);
  fputs(msg, fp);
  sprintf(msg, "\n------------------------------------------------------------------  ");
  fputs(msg, fp);
  fflush(fp);
}

void upper_sensor_changed_cb() {
  int value = digitalRead(UPPER_SENSOR_PIN);
  if(value == 0) { // magent in contact = open
    door_state = open;
    if(value != g_upper_state && g_door_open) {
      g_door_open();
      print_door_state(door_state);
    }
    
  } else {
    if(value != g_upper_state) {
      door_state = closing;
      if(g_door_closing) {
        g_door_closing();
      }
      print_door_state(door_state);
    }
  }

  g_upper_state = value;
  sprintf(msg, "\n\n------------------------------------------------------------------  ");
  fputs(msg, fp);
  sprintf(msg, "\n *********** UPPER SENSOR fired and is %d, the door is %s", value, STATE_STRING[door_state]);
  fputs(msg, fp);
  sprintf(msg, "\n------------------------------------------------------------------  ");
  fputs(msg, fp);
}


void print_door_state(enum garage_door_state_codes state) {
  sprintf(msg, "\n\n------------------------------------------------------------------  ");
  fputs(msg, fp);
  sprintf(msg, "\n *********** Door state is %s", STATE_STRING[state]);
  fputs(msg, fp);
  sprintf(msg, "\n--------------------------------------------------------------------  ");
  fputs(msg, fp);
}
  
enum garage_door_state_codes get_door_state(int lowerSensorState, int upperSensorState) {

  if(lowerSensorState == 0) {
    return closed;
  }
  if(upperSensorState == 0) {
    return open;
  }
  return closed;
}

int close_door() {

  digitalWrite(DOOR_ACTUATOR_PIN, 0);
  digitalWrite(DOOR_ACTUATOR_BACKUP_PIN, 0);
  sleep(1);
  digitalWrite(DOOR_ACTUATOR_PIN, 1);
  digitalWrite(DOOR_ACTUATOR_BACKUP_PIN, 1);
  return 0;
}

int open_door() {
  digitalWrite(DOOR_ACTUATOR_PIN, 0);
  sleep(1);
  digitalWrite(DOOR_ACTUATOR_PIN, 1);
  return 0;
}

void flash_actuator() {

  for(int i=0; i<3; i++) {
    digitalWrite(DOOR_ACTUATOR_PIN, 0);
    digitalWrite(DOOR_ACTUATOR_BACKUP_PIN, 0);
    sleep(1);
    printf("turn off relay %d", i);
    digitalWrite(DOOR_ACTUATOR_PIN, 1);
    digitalWrite(DOOR_ACTUATOR_BACKUP_PIN, 1);
    sleep(1);
  }
}