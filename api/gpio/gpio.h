static const int LOWER_SENSOR_PIN = 4;
static const int UPPER_SENSOR_PIN = 5;

void process_lower_sensor();
void process_upper_sensor();

enum garage_door_state_codes { opening, closing, open, closed};
int initGarageDoor(void (*opening) (void), void (*closing) (void), void (*open) (void), void (*closed) (void) );

enum garage_door_state_codes getDoorState(int, int);

