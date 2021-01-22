static const int LOWER_SENSOR_PIN = 4;
static const int UPPER_SENSOR_PIN = 5;

static const int DOOR_ACTUATOR_PIN = 7;

static const int DOOR_ACTUATOR_BACKUP_PIN = 0;




enum garage_door_state_codes { opening, closing, open, closed};
int init_door(void (*opening) (void), void (*closing) (void), void (*open) (void), void (*closed) (void) );

enum garage_door_state_codes get_door_state(int, int);

int close_door();
int open_door();

