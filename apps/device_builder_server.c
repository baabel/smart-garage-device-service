/*
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 Copyright 2021-2022 Mango Home
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
 Licensed under the Apache License, Version 2.0 (the "License");
 you may not use this file except in compliance with the License.
 You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0

 Unless required by applicable law or agreed to in writing, software
 distributed under the License is distributed on an "AS IS" BASIS,
 WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 See the License for the specific language governing permissions and
 limitations under the License.
-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
*/

/* Application Design
*
* support functions:
* app_init
*  initializes the oic/p and oic/d values.
* register_resources
*  function that registers all endpoints, e.g. sets the RETRIEVE/UPDATE handlers for each end point
*
* main 
*  starts the stack, with the registered resources.
*
* Each resource has:
*  global property variables (per resource path) for:
*    the property name
*       naming convention: g_<path>_RESOURCE_PROPERTY_NAME_<propertyname>
*    the actual value of the property, which is typed from the json data type
*      naming convention: g_<path>_<propertyname>
*  global resource variables (per path) for:
*    the path in a variable:
*      naming convention: g_<path>_RESOURCE_ENDPOINT
*    array of interfaces, where by the first will be set as default interface
*      naming convention g_<path>_RESOURCE_INTERFACE
*
*  handlers for the implemented methods (get/post)
*   get_<path>
*     function that is being called when a RETRIEVE is called on <path>
*     set the global variables in the output
*   post_<path>
*     function that is being called when a UPDATE is called on <path>
*     checks the input data
*     if input data is correct
*       updates the global variables
*
*/

#include <time.h>
#include <sys/time.h>
#include "oc_api.h"
#include "oc_core_res.h"
#include "oc_swupdate.h"
#include "port/oc_clock.h"
#include <signal.h>
#include "../api/gpio/gpio.h"
#include "oc_easysetup_enrollee.h"
#include "wifi.h"

#ifdef OC_CLOUD
#include "oc_cloud.h"
#endif
#if defined(OC_IDD_API)
#include "oc_introspection.h"
#endif

#ifdef __linux__
/* linux specific code */
#include <pthread.h>
#ifndef NO_MAIN
static pthread_mutex_t mutex;
static pthread_cond_t cv;
static struct timespec ts;
#endif /* NO_MAIN */
#endif


#ifdef OC_SOFTWARE_UPDATE

#endif /* OC_SOFTWARE_UPDATE */



#define btoa(x) ((x)?"true":"false")

#define MAX_STRING 30           /* max size of the strings. */
#define MAX_PAYLOAD_STRING 65   /* max size strings in the payload */
#define MAX_ARRAY 10            /* max size of the array */
/* Note: Magic numbers are derived from the resource definition, either from the example or the definition.*/

volatile int quit = 0;          /* stop variable, used by handle_signal */

static const char *cis;
static const char *auth_code;
static const char *sid;
static const char *apn;

static void LOG(const char * message);
static void timestamp();


#define SOFT_AP_SSID "Mango-Home-Innovation"
#define SOFT_AP_PSK "111222333"

// There are indicative values and might vary with application requirement
#define MAX_APP_DATA_SIZE 8192
#define MAX_MTU_SIZE 2048

static int g_device_count = 0;



/* global property variables for path: "/doorstate" */
static char g_doorstate_RESOURCE_PROPERTY_NAME_openAlarm[] = "openAlarm"; /* the name for the attribute */
bool g_doorstate_openAlarm = false; /* current value of property "openAlarm" The state of the door open alarm. */
static char g_doorstate_RESOURCE_PROPERTY_NAME_openDuration[] = "openDuration"; /* the name for the attribute */
char g_doorstate_openDuration[ MAX_PAYLOAD_STRING ] = """"; /* current value of property "openDuration" A string representing duration formatted as defined in ISO 8601. Allowable formats are: P[n]Y[n]M[n]DT[n]H[n]M[n]S, P[n]W, P[n]Y[n]-M[n]-DT[0-23]H[0-59]:M[0-59]:S, and P[n]W, P[n]Y[n]M[n]DT[0-23]H[0-59]M[0-59]S. P is mandatory, all other elements are optional, time elements must follow a T. */
static char g_doorstate_RESOURCE_PROPERTY_NAME_openState[] = "openState"; /* the name for the attribute */
char g_doorstate_openState[ MAX_PAYLOAD_STRING ] = """"; /* current value of property "openState" The state of the door (open or closed). */
/* global property variables for path: "/openlevel" */
static char g_openlevel_RESOURCE_PROPERTY_NAME_openLevel[] = "openLevel"; /* the name for the attribute */
int g_openlevel_openLevel = 50; /* current value of property "openLevel" How open or ajar the entity is. */
static char g_openlevel_RESOURCE_PROPERTY_NAME_range[] = "range"; /* the name for the attribute */

/* array range  The valid range for the Property in the Resource as an integer. The first value in the array is the minimum value, the second value in the array is the maximum value. */
int g_openlevel_range[2]; 
size_t g_openlevel_range_array_size;

static char g_openlevel_RESOURCE_PROPERTY_NAME_step[] = "step"; /* the name for the attribute */
int g_openlevel_step = 2; /* current value of property "step" Step value across the defined range when the range is an integer.  This is the increment for valid values across the range; so if range is 0..10 and step is 2 then valid values are 0,2,4,6,8,10. *//* registration data variables for the resources */

/* global resource variables for path: /doorstate */
static char g_doorstate_RESOURCE_ENDPOINT[] = "/doorstate"; /* used path for this resource */
static char g_doorstate_RESOURCE_TYPE[][MAX_STRING] = {"oic.r.door"}; /* rt value (as an array) */
int g_doorstate_nr_resource_types = 1;
static char g_doorstate_RESOURCE_INTERFACE[][MAX_STRING] = {"oic.if.a","oic.if.baseline"}; /* interface if (as an array) */
int g_doorstate_nr_resource_interfaces = 2;

/* global resource variables for path: /openlevel */
static char g_openlevel_RESOURCE_ENDPOINT[] = "/openlevel"; /* used path for this resource */
static char g_openlevel_RESOURCE_TYPE[][MAX_STRING] = {"oic.r.openlevel"}; /* rt value (as an array) */
int g_openlevel_nr_resource_types = 1;
static char g_openlevel_RESOURCE_INTERFACE[][MAX_STRING] = {"oic.if.a","oic.if.baseline"}; /* interface if (as an array) */
int g_openlevel_nr_resource_interfaces = 2;

int validate_purl(const char *purl);
int download_update(size_t device, const char *url);
int check_new_version(size_t device, const char *url, const char *version);
int perform_upgrade(size_t device, const char *url);


// Device 1 Callbaks
static void
wes_prov_cb1(oc_wes_data_t *wes_prov_data, void *user_data)
{
  (void)user_data;
  PRINT("wes_prov_cb1\n");
  if (wes_prov_data == NULL) {
      PRINT("wes_prov_data is NULL\n");
      return;
  }
}

static void
device_prov_cb1(oc_wes_device_data_t *device_prov_data, void *user_data)
{
  (void)user_data;
  PRINT("device_prov_cb1\n");
  if (device_prov_data == NULL) {
      PRINT("device_prov_data is NULL\n");
      return;
  }
  PRINT("Device Name: %s\n", oc_string(device_prov_data->dev_name));
}

static void
wifi_prov_cb1(oc_wes_wifi_data_t *wifi_prov_data, void *user_data)
{
  (void)user_data;
  PRINT("wifi_prov_cb1 triggered\n");
  if (wifi_prov_data == NULL) {
      PRINT("wes_prov_data is NULL\n");
      return;
  }
  PRINT("SSID : %s\n", oc_string(wifi_prov_data->ssid));
  PRINT("Password : %s\n", oc_string(wifi_prov_data->cred));
  PRINT("AuthType : %d\n", wifi_prov_data->auth_type);
  PRINT("EncType : %d\n", wifi_prov_data->enc_type);

  //1  Stop DHCP Server
  wifi_stop_dhcp_server();
  //1 Start WiFi Station
  wifi_start_station();
  //1 Join WiFi AP with ssid, authtype and pwd
  wifi_join(NULL, NULL);
  //1 Start DHCP client
  wifi_start_dhcp_client();
}

// resource proisining callbacks for 1 device
wes_device_callbacks_s g_rsc_cbks[] = {
  {
    .oc_wes_prov_cb_t = &wes_prov_cb1,
    .oc_wes_wifi_prov_cb_t = &wifi_prov_cb1,
    .oc_wes_dev_prov_cb_t = &device_prov_cb1,
  }
};


void garage_door_opening_callback() {
    LOG(" garage_door_opening_callback\n");
    g_openlevel_openLevel=2;
    int result = oc_notify_observers(oc_ri_get_app_resource_by_uri(g_openlevel_RESOURCE_ENDPOINT, strlen(g_openlevel_RESOURCE_ENDPOINT), 0));
    char buffer[80];
    sprintf(buffer, "\n oc_notify_observers result is %d \n", result);    
    LOG(buffer);  
}

void garage_door_closing_callback() {
    LOG(" garage_door_closing_callback\n");
    g_openlevel_openLevel = 98;
    int result = oc_notify_observers(oc_ri_get_app_resource_by_uri(g_openlevel_RESOURCE_ENDPOINT, strlen(g_openlevel_RESOURCE_ENDPOINT), 0));
    char buffer[80];
    sprintf(buffer, "\n oc_notify_observers result is %d \n", result);  
    LOG(buffer);
}

void garage_door_open_callback() {
    LOG(" garage_door_open_callback\n");
    g_openlevel_openLevel = 100;
    int result = oc_notify_observers(oc_ri_get_app_resource_by_uri(g_openlevel_RESOURCE_ENDPOINT, strlen(g_openlevel_RESOURCE_ENDPOINT), 0));
    char buffer[80];
    sprintf(buffer, "\n oc_notify_observers result is %d \n", result);  
    LOG(buffer);
}

void garage_door_closed_callback() {
    LOG(" garage_door_closed_callback\n");
    g_openlevel_openLevel = 0;
    int result = oc_notify_observers(oc_ri_get_app_resource_by_uri(g_openlevel_RESOURCE_ENDPOINT, strlen(g_openlevel_RESOURCE_ENDPOINT), 0));
    char buffer[80];
    sprintf(buffer, "\n oc_notify_observers result is %d \n", result);  
    LOG(buffer);
}

static void timestamp(char * output)
{
  struct timeval curTime;
  gettimeofday(&curTime, NULL);
  int milli = curTime.tv_usec / 1000;

  time_t rawtime;
  struct tm * timeinfo;
  char buffer [80];

  time(&rawtime);
  timeinfo = localtime(&rawtime);

  strftime(buffer, 80, "%Y-%m-%d %H:%M:%S", timeinfo);
  sprintf(output, "%s:%d", buffer, milli);
}

static void LOG(const char * message) {
  printf("About to log %s", message);
  char timedata[84] = "";
  timestamp(timedata);
  printf("%s %s", timedata, message);
}

static void 
set_additional_platform_properties(void *data)
{
  LOG("\nset_additional_platform_properties");
  (void)data;
  // Manufactures Details Link
  oc_set_custom_platform_property(mnml, "http://www.mangohome.com/manufacture");
  // Model Number
  oc_set_custom_platform_property(mnmo, "RPiSG-100");
  // Date of Manufacture
  oc_set_custom_platform_property(mndt, "2020/10/01");
  //Serial Number
  oc_set_custom_platform_property(mnsel, "1234567890");
  // Manufacture firware
  oc_set_custom_platform_property(mnfv, "0.0.1");

  oc_set_custom_platform_property(vid, "Garage door controller");

  oc_set_custom_platform_property(mnnm, "Mango");
  // manufature hardware version
  oc_set_custom_platform_property(mnhw, "0.0.1");

  oc_set_custom_platform_property(mnos, "pi");

  oc_set_custom_platform_property(mnsel, "1234567890");

// Manufacturer's Support Information URL
  oc_set_custom_platform_property(mnsl, "https://www.mangohome.com/support");
}

static void 
set_additional_device_properties(void *data)
{
  (void)data;
  // Device purpose
  oc_set_custom_device_property("purpose", "Garage door controller");
  
}

/**
* function to set up the device.
*
*/
static int
app_init(void)
{
  init_door(&garage_door_opening_callback, &garage_door_closing_callback, &garage_door_open_callback, &garage_door_closed_callback);
  LOG("initGarageDoor()\n");

  int ret = oc_init_platform("mango home", set_additional_platform_properties, NULL);
  /* the settings determine the appearance of the device on the network
     can be OCF1.3.1 or OCF2.0.0 (or even higher)
     supplied values are for OCF1.3.1 */
  ret |= oc_add_device("/oic/d", "oic.d.garagedoor", "SmartGarage Door", 
                       "ocf.2.0.5", /* icv value */
                       "ocf.res.1.3.0, ocf.sh.1.3.0",  /* dmv value */
                       set_additional_device_properties, NULL);
                       
#if defined(OC_IDD_API)
  FILE *fp;
  uint8_t *buffer;
  size_t buffer_size;
  const char introspection_error[] =
    "\tERROR Could not read 'server_introspection.cbor'\n"
    "\tIntrospection data not set for device.\n";
  fp = fopen("./server_introspection.cbor", "rb");
  if (fp) {
    fseek(fp, 0, SEEK_END);
    buffer_size = ftell(fp);
    rewind(fp);

    buffer = (uint8_t *)malloc(buffer_size * sizeof(uint8_t));
    size_t fread_ret = fread(buffer, buffer_size, 1, fp);
    fclose(fp);

    if (fread_ret == 1) {
      oc_set_introspection_data(0, buffer, buffer_size);
      PRINT("\tIntrospection data set from 'server_introspection.cbor' for device %d [bytes]\n", (int)buffer_size);
    } else {
      PRINT("%s", introspection_error);
    }
    free(buffer);
  } else {
    PRINT("%s", introspection_error);
  }
#else
    LOG("\t introspection via header file\n");
#endif

 g_device_count = oc_core_get_num_devices();
 PRINT("Numer of registered  Devices %d\n", g_device_count);
  return ret;
}

/**
* helper function to convert the interface string definition to the constant defintion used by the stack.
* @param interface the interface string e.g. "oic.if.a"
* @return the stack constant for the interface
*/
static int
convert_if_string(char *interface_name)
{
  if (strcmp(interface_name, "oic.if.baseline") == 0) return OC_IF_BASELINE;  /* baseline interface */
  if (strcmp(interface_name, "oic.if.rw") == 0) return OC_IF_RW;              /* read write interface */
  if (strcmp(interface_name, "oic.if.r" )== 0) return OC_IF_R;                /* read interface */
  if (strcmp(interface_name, "oic.if.s") == 0) return OC_IF_S;                /* sensor interface */
  if (strcmp(interface_name, "oic.if.a") == 0) return OC_IF_A;                /* actuator interface */
  if (strcmp(interface_name, "oic.if.b") == 0) return OC_IF_B;                /* batch interface */
  if (strcmp(interface_name, "oic.if.ll") == 0) return OC_IF_LL;              /* linked list interface */
  return OC_IF_A;
}

/**
* helper function to check if the POST input document contains 
* the common readOnly properties or the resouce readOnly properties
* @param name the name of the property
* @return the error_status, e.g. if error_status is true, then the input document contains something illegal
*/
static bool
check_on_readonly_common_resource_properties(oc_string_t name, bool error_state)
{
  if (strcmp ( oc_string(name), "n") == 0) {
    error_state = true;
    LOG ("   property \"n\" is ReadOnly \n");
  }else if (strcmp ( oc_string(name), "if") == 0) {
    error_state = true;
    LOG ("   property \"if\" is ReadOnly \n");
  } else if (strcmp ( oc_string(name), "rt") == 0) {
    error_state = true;
    LOG ("   property \"rt\" is ReadOnly \n");
  } else if (strcmp ( oc_string(name), "id") == 0) {
    error_state = true;
    LOG ("   property \"id\" is ReadOnly \n");
  } else if (strcmp ( oc_string(name), "id") == 0) {
    error_state = true;
    LOG ("   property \"id\" is ReadOnly \n");
  } 
  return error_state;
}


 
/**
* get method for "/doorstate" resource.
* function is called to intialize the return values of the GET method.
* initialisation of the returned values are done from the global property values.
* Resource Description:
* This Resource describes the open state of the door.
* A door is modelled by means of openState (Open/Closed), openDuration (ISO 8601 Time), and openAlarm (boolean).
* For Property "openState", the value 'Open' indicates the door is open.
* The value 'Closed' indicates the door is closed.
* The type of Property "openDuration" is an RFC Time encoded string.
* The Property "openAlarm" value 'true' indicates that the open alarm is active.
* The openAlarm value 'false' indicates that open alarm is not active.
* retrieves the state of the Door.
*
* @param request the request representation.
* @param interfaces the interface used for this call
* @param user_data the user data.
*/
static void
get_doorstate(oc_request_t *request, oc_interface_mask_t interfaces, void *user_data)
{
  (void)user_data;  /* variable not used */
  /* TODO: SENSOR add here the code to talk to the HW if one implements a sensor.
     the call to the HW needs to fill in the global variable before it returns to this function here.
     alternative is to have a callback from the hardware that sets the global variables.
  
     The implementation always return everything that belongs to the resource.
     this implementation is not optimal, but is functionally correct and will pass CTT1.2.2 */
  bool error_state = false;
  
  
  LOG("-- Begin get_doorstate: interface %d\n");
  oc_rep_start_root_object();
  switch (interfaces) {
  case OC_IF_BASELINE:
    /* fall through */
  case OC_IF_A:
  LOG("   Adding Baseline info\n" );
    oc_process_baseline_interface(request->resource);
    
    /* property (boolean) 'openAlarm' */
    oc_rep_set_boolean(root, openAlarm, g_doorstate_openAlarm);
    PRINT("   %s : %s\n", g_doorstate_RESOURCE_PROPERTY_NAME_openAlarm,  btoa(g_doorstate_openAlarm));
    /* property (string) 'openDuration' */
    oc_rep_set_text_string(root, openDuration, g_doorstate_openDuration);
    PRINT("   %s : %s\n", g_doorstate_RESOURCE_PROPERTY_NAME_openDuration, g_doorstate_openDuration);
    /* property (string) 'openState' */
    oc_rep_set_text_string(root, openState, g_doorstate_openState);
    PRINT("   %s : %s\n", g_doorstate_RESOURCE_PROPERTY_NAME_openState, g_doorstate_openState);
    break;
  default:
    break;
  }
  oc_rep_end_root_object();
  if (error_state == false) {
    oc_send_response(request, OC_STATUS_OK);
  }
  else {
    oc_send_response(request, OC_STATUS_BAD_OPTION);
  }
  PRINT("-- End get_doorstate\n");
}
 
/**
* get method for "/openlevel" resource.
* function is called to intialize the return values of the GET method.
* initialisation of the returned values are done from the global property values.
* Resource Description:
* This Resource describes how open or ajar an entity such as a window, door, blind or shutter is.
* The Property "openLevel" can be read (acting as a sensor).
* The "openLevel" can also be set (acting as an actuator).
* The "openLevel" is device dependent across the range provided.
* When the Property "range" is omitted then 0 to 100 is assumed where 0 means closed, 100 means fully open.
* If a "range" is provided then the lower bound=closed, upper bound=open.
* If Property "step" is present then it represents the increment between possible values; if not provided 1 is assumed.
*
* @param request the request representation.
* @param interfaces the interface used for this call
* @param user_data the user data.
*/
static void
get_openlevel(oc_request_t *request, oc_interface_mask_t interfaces, void *user_data)
{
  (void)user_data;  /* variable not used */
  /* TODO: SENSOR add here the code to talk to the HW if one implements a sensor.
     the call to the HW needs to fill in the global variable before it returns to this function here.
     alternative is to have a callback from the hardware that sets the global variables.
  
     The implementation always return everything that belongs to the resource.
     this implementation is not optimal, but is functionally correct and will pass CTT1.2.2 */
  bool error_state = false;
  
  
  LOG("-- Begin get_openlevel: interface %d\n");
  oc_rep_start_root_object();
  switch (interfaces) {
  case OC_IF_BASELINE:
    /* fall through */
  case OC_IF_A:
  PRINT("   Adding Baseline info\n" );
    oc_process_baseline_interface(request->resource);
    
    /* property (integer) 'openLevel' */
    oc_rep_set_int(root, openLevel, g_openlevel_openLevel);
    PRINT("   %s : %d\n", g_openlevel_RESOURCE_PROPERTY_NAME_openLevel, g_openlevel_openLevel);
    
    /* property (array of integers) 'range' */
    oc_rep_set_array(root, range);
    PRINT("   %s int = [ ", g_openlevel_RESOURCE_PROPERTY_NAME_range);
    for (int i=0; i< (int)g_openlevel_range_array_size; i++) {
      oc_rep_add_int(range, g_openlevel_range[i]);
      PRINT("   %d ", g_openlevel_range[i]);
    }
    oc_rep_close_array(root, range);
    /* property (integer) 'step' */
    oc_rep_set_int(root, step, g_openlevel_step);
    PRINT("   %s : %d\n", g_openlevel_RESOURCE_PROPERTY_NAME_step, g_openlevel_step);
    break;
  default:
    break;
  }
  oc_rep_end_root_object();
  if (error_state == false) {
    oc_send_response(request, OC_STATUS_OK);
  }
  else {
    oc_send_response(request, OC_STATUS_BAD_OPTION);
  }
  LOG("-- End get_openlevel\n");
}
 
/**
* post method for "/doorstate" resource.
* The function has as input the request body, which are the input values of the POST method.
* The input values (as a set) are checked if all supplied values are correct.
* If the input values are correct, they will be assigned to the global  property values.
* Resource Description:
* Sets the current Door properties.
* The only property that can be set as part of an update operation is
*   the openAlarm.
* This can be made active (true) or inactive (false)
*
* @param request the request representation.
* @param interfaces the used interfaces during the request.
* @param user_data the supplied user data.
*/
static void
post_doorstate(oc_request_t *request, oc_interface_mask_t interfaces, void *user_data)
{
  (void)interfaces;
  (void)user_data;
  bool error_state = false;
  LOG("-- Begin post_doorstate:\n");
  oc_rep_t *rep = request->request_payload;
  
  /* loop over the request document for each required input field to check if all required input fields are present */
  bool var_in_request= false; 
  rep = request->request_payload;
  while (rep != NULL) {
    if (strcmp ( oc_string(rep->name), g_doorstate_RESOURCE_PROPERTY_NAME_openAlarm) == 0) {
      var_in_request = true;
    }
    rep = rep->next;
  }
  if ( var_in_request == false) 
  { 
      error_state = true;
      PRINT (" required property: 'openAlarm' not in request\n");
  }
  /* loop over the request document to check if all inputs are ok */
  rep = request->request_payload;
  while (rep != NULL) {
    PRINT("key: (check) %s \n", oc_string(rep->name));
    
    error_state = check_on_readonly_common_resource_properties(rep->name, error_state);
    if (strcmp ( oc_string(rep->name), g_doorstate_RESOURCE_PROPERTY_NAME_openAlarm) == 0) {
      /* property "openAlarm" of type boolean exist in payload */
      if (rep->type != OC_REP_BOOL) {
        error_state = true;
        PRINT ("   property 'openAlarm' is not of type bool %d \n", rep->type);
      }
    }rep = rep->next;
  }
  /* if the input is ok, then process the input document and assign the global variables */
  if (error_state == false)
  {
    /* loop over all the properties in the input document */
    oc_rep_t *rep = request->request_payload;
    while (rep != NULL) {
      PRINT("key: (assign) %s \n", oc_string(rep->name));
      /* no error: assign the variables */
      
      if (strcmp ( oc_string(rep->name), g_doorstate_RESOURCE_PROPERTY_NAME_openAlarm)== 0) {
        /* assign "openAlarm" */
        PRINT ("  property 'openAlarm' : %s\n", btoa(rep->value.boolean));
        g_doorstate_openAlarm = rep->value.boolean;
      }
      rep = rep->next;
    }
    /* set the response */
    LOG("Set response \n");
    oc_rep_start_root_object();
    /*oc_process_baseline_interface(request->resource); */
    oc_rep_set_boolean(root, openAlarm, g_doorstate_openAlarm);
    oc_rep_set_text_string(root, openDuration, g_doorstate_openDuration);
    oc_rep_set_text_string(root, openState, g_doorstate_openState);
    
    oc_rep_end_root_object();
    /* TODO: ACTUATOR add here the code to talk to the HW if one implements an actuator.
       one can use the global variables as input to those calls
       the global values have been updated already with the data from the request */
    oc_send_response(request, OC_STATUS_CHANGED);
  }
  else
  {
    LOG("  Returning Error \n");
    /* TODO: add error response, if any */
    //oc_send_response(request, OC_STATUS_NOT_MODIFIED);
    oc_send_response(request, OC_STATUS_BAD_REQUEST);
  }
  LOG("-- End post_doorstate\n");
}
 
/**
* post method for "/openlevel" resource.
* The function has as input the request body, which are the input values of the POST method.
* The input values (as a set) are checked if all supplied values are correct.
* If the input values are correct, they will be assigned to the global  property values.
* Resource Description:
* Sets the desired openLevel.
*
* @param request the request representation.
* @param interfaces the used interfaces during the request.
* @param user_data the supplied user data.
*/
static void
post_openlevel(oc_request_t *request, oc_interface_mask_t interfaces, void *user_data)
{
  (void)interfaces;
  (void)user_data;
  bool error_state = false;
  LOG("-- Begin post_openlevel:\n");
  oc_rep_t *rep = request->request_payload;
  
  /* loop over the request document for each required input field to check if all required input fields are present */
  bool var_in_request= false; 
  rep = request->request_payload;
  while (rep != NULL) {
    if (strcmp ( oc_string(rep->name), g_openlevel_RESOURCE_PROPERTY_NAME_openLevel) == 0) {
      var_in_request = true;
    }
    rep = rep->next;
  }
  if ( var_in_request == false) 
  { 
      error_state = true;
      LOG (" required property: 'openLevel' not in request\n");
  }
  /* loop over the request document to check if all inputs are ok */
  rep = request->request_payload;
  while (rep != NULL) {
    PRINT("key: (check) %s \n", oc_string(rep->name));
    
    error_state = check_on_readonly_common_resource_properties(rep->name, error_state);
    if (strcmp ( oc_string(rep->name), g_openlevel_RESOURCE_PROPERTY_NAME_openLevel) == 0) {
      /* property "openLevel" of type integer exist in payload */
      if (rep->type != OC_REP_INT) {
        error_state = true;
        PRINT ("   property 'openLevel' is not of type int %d \n", rep->type);
      }
    }
    if (strcmp ( oc_string(rep->name), g_openlevel_RESOURCE_PROPERTY_NAME_range) == 0) {
      /* property "range" of type array exist in payload */
      /* check if "range" is read only */
      error_state = true;
      PRINT ("   property 'range' is readOnly \n");
      size_t array_size=0;
      
      long long int *temp_array = 0;  
      oc_rep_get_int_array(rep, "range", &temp_array, &array_size);
      
      if ( array_size > 2)
      {
         error_state = true;
         PRINT ("   property array 'range' is too long: %d expected: 2 \n", (int)array_size);
      }
    }
    if (strcmp ( oc_string(rep->name), g_openlevel_RESOURCE_PROPERTY_NAME_step) == 0) {
      /* property "step" of type integer exist in payload *//* check if "step" is read only */
      error_state = true;
      LOG ("   property 'step' is readOnly \n");
      if (rep->type != OC_REP_INT) {
        error_state = true;
        PRINT ("   property 'step' is not of type int %d \n", rep->type);
      }
      
      
    } rep = rep->next;
  }
  /* if the input is ok, then process the input document and assign the global variables */
  if (error_state == false)
  {
    /* loop over all the properties in the input document */
    oc_rep_t *rep = request->request_payload;
    while (rep != NULL) {
      PRINT("key: (assign) %s \n", oc_string(rep->name));
      /* no error: assign the variables */
      
      if (strcmp ( oc_string(rep->name), g_openlevel_RESOURCE_PROPERTY_NAME_openLevel) == 0) {
        /* assign "openLevel" */
        PRINT ("  property 'openLevel' : %d\n", (int) rep->value.integer);
        g_openlevel_openLevel = (int) rep->value.integer;
      } 
        if (strcmp ( oc_string(rep->name), g_openlevel_RESOURCE_PROPERTY_NAME_range) == 0) {
            /* retrieve the array pointer to the int array of of property "range"
               note that the variable g_openlevel_range_array_size will contain the array size in the payload. */
            long long int *temp_integer = 0;
            oc_rep_get_int_array(rep, "range", &temp_integer, &g_openlevel_range_array_size);
            /* copy over the data of the retrieved array to the global variable */
            for (int j = 0; j < (int)g_openlevel_range_array_size; j++) {
              PRINT(" integer %d ", temp_integer[j]);
              g_openlevel_range[j] = temp_integer[j];
            }
        }
      if (strcmp ( oc_string(rep->name), g_openlevel_RESOURCE_PROPERTY_NAME_step) == 0) {
        /* assign "step" */
        PRINT ("  property 'step' : %d\n", (int) rep->value.integer);
        g_openlevel_step = (int) rep->value.integer;
      }
      rep = rep->next;
    }
    /* set the response */
    LOG("Set response \n");
    oc_rep_start_root_object();
    /*oc_process_baseline_interface(request->resource); */
    oc_rep_set_int(root, openLevel, g_openlevel_openLevel );
     
    oc_rep_set_array(root, range);
    for (int i=0; i< (int)g_openlevel_range_array_size; i++) {
      oc_rep_add_int(range, g_openlevel_range[i]);
    }
    oc_rep_close_array(root, range);
    
    oc_rep_set_int(root, step, g_openlevel_step );
    
    oc_rep_end_root_object();
    /* TODO: ACTUATOR add here the code to talk to the HW if one implements an actuator.
       one can use the global variables as input to those calls
       the global values have been updated already with the data from the request */
  
    if(g_openlevel_openLevel > 50) {
      LOG("\nSending open door to actuator");
      open_door();
      LOG("\nFinished open door");
    } 
    else
    {
      LOG("\nSending close door to actuator");
      close_door();
      LOG("\nFinished close door");
    }
    LOG("\nsend response");
    oc_send_response(request, OC_STATUS_CHANGED);
  }
  else
  {
    LOG("  Returning Error \n");
    /* TODO: add error response, if any */
    //oc_send_response(request, OC_STATUS_NOT_MODIFIED);
    oc_send_response(request, OC_STATUS_BAD_REQUEST);
  }
  LOG("-- End post_openlevel\n");
}
/**
* register all the resources to the stack
* this function registers all application level resources:
* - each resource path is bind to a specific function for the supported methods (GET, POST, PUT)
* - each resource is 
*   - secure
*   - observable
*   - discoverable 
*   - used interfaces (from the global variables).
*/
static void
register_resources(void)
{

  LOG("Register Resource with local path \"/doorstate\"\n");
  oc_resource_t *res_doorstate = oc_new_resource(NULL, g_doorstate_RESOURCE_ENDPOINT, g_doorstate_nr_resource_types, 0);
  PRINT("     number of Resource Types: %d\n", g_doorstate_nr_resource_types);
  for( int a = 0; a < g_doorstate_nr_resource_types; a++ ) {
    PRINT("     Resource Type: \"%s\"\n", g_doorstate_RESOURCE_TYPE[a]);
    oc_resource_bind_resource_type(res_doorstate,g_doorstate_RESOURCE_TYPE[a]);
  }
  for( int a = 0; a < g_doorstate_nr_resource_interfaces; a++ ) {
    oc_resource_bind_resource_interface(res_doorstate, convert_if_string(g_doorstate_RESOURCE_INTERFACE[a]));
  }
  oc_resource_set_default_interface(res_doorstate, convert_if_string(g_doorstate_RESOURCE_INTERFACE[0]));  
  PRINT("     Default OCF Interface: \"%s\"\n", g_doorstate_RESOURCE_INTERFACE[0]);
  oc_resource_set_discoverable(res_doorstate, true);
  /* periodic observable
     to be used when one wants to send an event per time slice
     period is 1 second */
  //oc_resource_set_periodic_observable(res_doorstate, 1);
  /* set observable
     events are send when oc_notify_observers(oc_resource_t *resource) is called.
    this function must be called when the value changes, perferable on an interrupt when something is read from the hardware. */
  oc_resource_set_observable(res_doorstate, true);
   
  oc_resource_set_request_handler(res_doorstate, OC_GET, get_doorstate, NULL); 
  oc_resource_set_request_handler(res_doorstate, OC_POST, post_doorstate, NULL);
#ifdef OC_CLOUD
  PRINT("BEGIN oc_cloud_add_resource(doorstate)\n");
  oc_cloud_add_resource(res_doorstate);
#endif
  oc_add_resource(res_doorstate);

  LOG("Register Resource with local path \"/openlevel\"\n");
  oc_resource_t *res_openlevel = oc_new_resource(NULL, g_openlevel_RESOURCE_ENDPOINT, g_openlevel_nr_resource_types, 0);
  PRINT("     number of Resource Types: %d\n", g_openlevel_nr_resource_types);
  for( int a = 0; a < g_openlevel_nr_resource_types; a++ ) {
    PRINT("     Resource Type: \"%s\"\n", g_openlevel_RESOURCE_TYPE[a]);
    oc_resource_bind_resource_type(res_openlevel,g_openlevel_RESOURCE_TYPE[a]);
  }
  for( int a = 0; a < g_openlevel_nr_resource_interfaces; a++ ) {
    oc_resource_bind_resource_interface(res_openlevel, convert_if_string(g_openlevel_RESOURCE_INTERFACE[a]));
  }
  oc_resource_set_default_interface(res_openlevel, convert_if_string(g_openlevel_RESOURCE_INTERFACE[0]));  
  PRINT("     Default OCF Interface: \"%s\"\n", g_openlevel_RESOURCE_INTERFACE[0]);
  oc_resource_set_discoverable(res_openlevel, true);
  /* periodic observable
     to be used when one wants to send an event per time slice
     period is 1 second */
  //oc_resource_set_periodic_observable(res_openlevel, 1);
  /* set observable
     events are send when oc_notify_observers(oc_resource_t *resource) is called.
    this function must be called when the value changes, perferable on an interrupt when something is read from the hardware. */
  oc_resource_set_observable(res_openlevel, true);
   
  oc_resource_set_request_handler(res_openlevel, OC_GET, get_openlevel, NULL);
#ifdef OC_CLOUD
  PRINT("BEGIN oc_cloud_add_resource(openlevel");
  oc_cloud_add_resource(res_openlevel);
#endif
  oc_resource_set_request_handler(res_openlevel, OC_POST, post_openlevel, NULL);
#ifdef OC_CLOUD
 // oc_cloud_add_resource(res_openlevel);
#endif
  oc_add_resource(res_openlevel);


  wifi_mode supported_mode[NUM_WIFIMODE] = {WIFI_11A, WIFI_11B,WIFI_11G, WIFI_11N, WIFI_11AC, WIFI_MODE_MAX};
  wifi_freq supported_freq[NUM_WIFIFREQ] = {WIFI_24G, WIFI_5G, WIFI_FREQ_MAX};
  char *device_name = "MangoGarageDoor";

  oc_wes_set_resource_callbacks(0, g_rsc_cbks[0].oc_wes_prov_cb_t,
          g_rsc_cbks[0].oc_wes_wifi_prov_cb_t, g_rsc_cbks[0].oc_wes_dev_prov_cb_t);

    // Set Device Info
  if (oc_wes_set_device_info(0, supported_mode, supported_freq, device_name) == OC_ES_ERROR)
         PRINT("oc_wes_set_device_info error!\n");
  
}


#ifdef OC_SECURITY
void
random_pin_cb(const unsigned char *pin, size_t pin_len, void *data)
{
  (void)data;
  PRINT("\n====================\n");
  PRINT("Random PIN: %.*s\n", (int)pin_len, pin);
  PRINT("====================\n");
}
#endif /* OC_SECURITY */

static int
read_pem(const char *file_path, char *buffer, size_t *buffer_len)
{
  FILE *fp = fopen(file_path, "r");
  if (fp == NULL) {
    char buffer[80] = "";
    sprintf(buffer, "ERROR: unable to open PEM: %s\n", file_path);
    LOG(buffer);
    return -1;
  }
  if (fseek(fp, 0, SEEK_END) != 0) {
    LOG("ERROR: unable to read PEM\n");
    fclose(fp);
    return -1;
  }
  long pem_len = ftell(fp);
  if (pem_len < 0) {
    LOG("ERROR: could not obtain length of file\n");
    fclose(fp);
    return -1;
  }
  if (pem_len > (long)*buffer_len) {
    LOG("ERROR: buffer provided too small\n");
    fclose(fp);
    return -1;
  }
  if (fseek(fp, 0, SEEK_SET) != 0) {
    char buffer[80] = "";
    sprintf(buffer, "ERROR: unable to seek PEM: %s\n", file_path);
    LOG(buffer);
    fclose(fp);
    return -1;
  }
  if (fread(buffer, 1, pem_len, fp) < (size_t)pem_len) {
    char buffer[80] = "";
    sprintf(buffer, "ERROR: unable to read PEM: %s\n", file_path);
    LOG(buffer);
    fclose(fp);
    return -1;
  }
  fclose(fp);
  buffer[pem_len] = '\0';
  *buffer_len = (size_t)pem_len;
  return 0;
}

void
factory_presets_cb(size_t device, void *data)
{
  (void)device;
  (void)data;
//#if defined(OC_SECURITY) && defined(OC_PKI)
/* code to include an pki certificate and root trust anchor */
#include "oc_pki.h"
#include "pki_certs.h"
  LOG("\nFactory presets callback");
  int credid =
    oc_pki_add_mfg_cert(0, (const unsigned char *)my_cert, strlen(my_cert), (const unsigned char *)my_key, strlen(my_key));
  if (credid < 0) {
    LOG("ERROR installing PKI certificate\n");
  } else {
    LOG("Successfully installed PKI certificate\n");
  }

  if (oc_pki_add_mfg_intermediate_cert(0, credid, (const unsigned char *)int_ca, strlen(int_ca)) < 0) {
    LOG("ERROR installing intermediate CA certificate\n");
  } else {
    LOG("Successfully installed intermediate CA certificate\n");
  }

  if (oc_pki_add_mfg_trust_anchor(0, (const unsigned char *)root_ca, strlen(root_ca)) < 0) {
    LOG("ERROR installing root certificate\n");
  } else {
    LOG("Successfully installed root certificate\n");
  }

  unsigned char cloud_ca[4096];
  size_t cert_len = 4096;
  LOG("Load cloudca cert from file\n");
  if (read_pem("pki_certs/cloudca.pem", (char *)cloud_ca, &cert_len) < 0) {
    LOG("ERROR: unable to read cloud trust anchor certificates\n");
    return;
  }

  LOG("Add PKI cloudca trust anchor\n");
  int rootca_credid =
    oc_pki_add_trust_anchor(0, (const unsigned char *)cloud_ca, cert_len);
  if (rootca_credid < 0) {
    LOG("ERROR installing cloud root cert\n");
    return;
  } else {
	LOG("Cloud PKI trust anchor added\n");  
  }

  oc_pki_set_security_profile(0, OC_SP_BLACK, OC_SP_BLACK, credid);

}


/**
* intializes the global variables
* registers and starts the handler

*/
void
initialize_variables(void)
{
  /* initialize global variables for resource "/doorstate" */  g_doorstate_openAlarm = false; /* current value of property "openAlarm" The state of the door open alarm. */
  strcpy(g_doorstate_openDuration, """");  /* current value of property "openDuration" A string representing duration formatted as defined in ISO 8601. Allowable formats are: P[n]Y[n]M[n]DT[n]H[n]M[n]S, P[n]W, P[n]Y[n]-M[n]-DT[0-23]H[0-59]:M[0-59]:S, and P[n]W, P[n]Y[n]M[n]DT[0-23]H[0-59]M[0-59]S. P is mandatory, all other elements are optional, time elements must follow a T. */
  strcpy(g_doorstate_openState, """");  /* current value of property "openState" The state of the door (open or closed). */
  /* initialize global variables for resource "/openlevel" */
  g_openlevel_openLevel = 100; /* current value of property "openLevel" How open or ajar the entity is. */
  /* initialize array "range" : The valid range for the Property in the Resource as an integer. The first value in the array is the minimum value, the second value in the array is the maximum value. */g_openlevel_range[0] = 0;
  g_openlevel_range[1] = 100;
  g_openlevel_range_array_size = 2;
  
  g_openlevel_step = 2; /* current value of property "step" Step value across the defined range when the range is an integer.  This is the increment for valid values across the range; so if range is 0..10 and step is 2 then valid values are 0,2,4,6,8,10. */
  
  /* set the flag for NO oic/con resource. */
  oc_set_con_res_announced(false);

}

#ifndef NO_MAIN

#ifdef __linux__
/**
* signal the event loop (Linux)
* wakes up the main function to handle the next callback
*/
static void
signal_event_loop(void)
{
  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cv);
  pthread_mutex_unlock(&mutex);
}
#endif /* __linux__ */

/**
* handle Ctrl-C
* @param signal the captured signal
*/
void
handle_signal(int signal)
{
  (void)signal;
  signal_event_loop();
  quit = 1;
}


#ifdef OC_CLOUD
/**
* cloud status handler.
* handler to print out the status of the cloud connection
*/
static void
cloud_status_handler(oc_cloud_context_t *ctx, oc_cloud_status_t status,
                     void *data)
{
  (void)data;
  LOG("\nCloud Manager Status:\n");
  if (status & OC_CLOUD_REGISTERED) {
    LOG("\t\t-Registered\n");
  }
  if (status & OC_CLOUD_TOKEN_EXPIRY) {
    LOG("\t\t-Token Expiry: ");
    if (ctx) {
      PRINT("%d\n", oc_cloud_get_token_expiry(ctx));
    } else {
      LOG("\n");
    }
  }
  if (status & OC_CLOUD_FAILURE) {
    LOG("\t\t-Failure\n");
  }
  if (status & OC_CLOUD_LOGGED_IN) {
    LOG("\t\t-Logged In\n");
  }
  if (status & OC_CLOUD_LOGGED_OUT) {
    LOG("\t\t-Logged Out\n");
  }
  if (status & OC_CLOUD_DEREGISTERED) {
    LOG("\t\t-DeRegistered\n");
  }
  if (status & OC_CLOUD_REFRESHED_TOKEN) {
    LOG("\t\t-Refreshed Token\n");
  }
}
#endif // OC_CLOUD

/**
* main application.
* intializes the global variables
* registers and starts the handler
* handles (in a loop) the next event.
* shuts down the stack
*/
int
main(int argc, char *argv[])
{
int init;
  oc_clock_time_t next_event;
 
 if(argc == 0) {
   LOG("No arguments");
 }

  if (argc > 1) {
    auth_code = argv[1];
    PRINT("auth_code: %s\n", argv[1]);
  }
  if (argc > 2) {
    cis = argv[2];
    PRINT("cis : %s\n", argv[2]);
  }
  if (argc > 3) {
    sid = argv[3];
    PRINT("sid: %s\n", argv[3]);
  }
  if (argc > 4) {
    apn = argv[4];
    PRINT("apn: %s\n", argv[4]);
  }


  /* linux specific */
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = handle_signal;
  /* install Ctrl-C */
  sigaction(SIGINT, &sa, NULL);


  PRINT("wifi: Start\n");

  //1 TODO : Platform Interface
  wifi_start_softap(SOFT_AP_SSID, SOFT_AP_PSK);
  wifi_start_dhcp_server();

  LOG("Used input file : \"../device_output/out_codegeneration_merged.swagger.json\"\n");
  LOG("OCF Server name : \"server_lite_4918\"\n");

  /*intialize the variables */
  initialize_variables();


/*
 for Linux (as default) the folder is created in the makefile, with $target as name with _cred as post fix.
*/
#ifdef OC_SECURITY
  PRINT("Intialize Secure Resources\n");
  PRINT("\t storage at './device_builder_server_creds' \n");
  oc_storage_config("./device_builder_server_creds");  
#endif /* OC_SECURITY */

  /* initializes the handlers structure */
  static const oc_handler_t handler = {.init = app_init,
                                       .signal_event_loop = signal_event_loop,
                                       .register_resources = register_resources
#ifdef OC_CLIENT
                                       ,
                                       .requests_entry = 0 
#endif
                                       };

 
  oc_set_factory_presets_cb(factory_presets_cb, NULL);
  

#ifdef OC_SOFTWARE_UPDATE
  LOG("Initialize Software Update");
  static oc_swupdate_cb_t swupdate_impl;
  swupdate_impl.validate_purl = validate_purl;
  swupdate_impl.check_new_version = check_new_version;
  swupdate_impl.download_update = download_update;
  swupdate_impl.perform_upgrade = perform_upgrade;
  oc_swupdate_set_impl(&swupdate_impl);
  
#endif /* OC_SOFTWARE_UPDATE */
 
  /* start the stack */
  init = oc_main_init(&handler);

  if (init < 0) {
    PRINT("oc_main_init failed %d, exiting.\n", init);
    return init;
  }

#ifdef OC_CLOUD
  /* get the cloud context and start the cloud */
  LOG("Start Cloud Manager\n");
  oc_cloud_context_t *ctx = oc_cloud_get_context(0);
  if (ctx) {
    oc_cloud_manager_start(ctx, cloud_status_handler, NULL);
    if (cis) {
      LOG("Provision Cloud Conf Resource\n");
      oc_cloud_provision_conf_resource(ctx, cis, auth_code, sid, apn);
    }
  }
#endif 

  LOG("Garage door server \"server_lite\" running, waiting on incoming connections.\n");

  
#ifdef __linux__
  /* linux specific loop */
  while (quit != 1) {
    next_event = oc_main_poll();
    pthread_mutex_lock(&mutex);
    if (next_event == 0) {
      pthread_cond_wait(&cv, &mutex);
    } else {
      ts.tv_sec = (next_event / OC_CLOCK_SECOND);
      ts.tv_nsec = (next_event % OC_CLOCK_SECOND) * 1.e09 / OC_CLOCK_SECOND;
      pthread_cond_timedwait(&cv, &mutex, &ts);
    }
    pthread_mutex_unlock(&mutex);
  }
#endif

  /* shut down the stack */
#ifdef OC_CLOUD
  PRINT("Stop Cloud Manager\n");
  oc_cloud_manager_stop(ctx);
#endif
  oc_main_shutdown();
  return 0;
}
#endif /* NO_MAIN */


#ifdef OC_SOFTWARE_UPDATE
int
validate_purl(const char *purl)
{
  (void)purl;
 // uri::uri instance(purl);
 // if (instance.is_valid() == 0) {
 //   return -1;
 // }
  return 0;
}

int
check_new_version(size_t device, const char *url, const char *version)
{
  if (!url) {
    oc_swupdate_notify_done(device, OC_SWUPDATE_RESULT_INVALID_URL);
    return -1;
  }
  PRINT("Package url %s\n", url);
  if (version) {
    PRINT("Package version: %s\n", version);
  }
  oc_swupdate_notify_new_version_available(device, "2.0",
                                           OC_SWUPDATE_RESULT_SUCCESS);
  return 0;
}

int
download_update(size_t device, const char *url)
{
  (void)url;
  oc_swupdate_notify_downloaded(device, "2.0", OC_SWUPDATE_RESULT_SUCCESS);
  return 0;
}

int
perform_upgrade(size_t device, const char *url)
{
  (void)url;
  oc_swupdate_notify_upgrading(device, "2.0", oc_clock_time(),
                               OC_SWUPDATE_RESULT_SUCCESS);

  oc_swupdate_notify_done(device, OC_SWUPDATE_RESULT_SUCCESS);
  return 0;
}

#endif