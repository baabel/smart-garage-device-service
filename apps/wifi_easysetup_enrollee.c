/****************************************************************************
 *
 * Copyright (c) 2019-2020 Samsung Electronics
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specificlanguage governing permissions and
 * limitations under the License.
 *
 ******************************************************************/
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include "oc_api.h"
#include "oc_core_res.h"
#include "port/oc_clock.h"
#include "oc_easysetup_enrollee.h"
#include "wifi.h"

#define SOFT_AP_SSID "Mango-Home-Innovation"
#define SOFT_AP_PSK "111222333"

// There are indicative values and might vary with application requirement
#define MAX_APP_DATA_SIZE 8192
#define MAX_MTU_SIZE 2048

static int g_device_count = 0;
static pthread_mutex_t mutex;
static pthread_cond_t cond;
static struct timespec ts;
static bool g_exit = 0;

int delete_directory(const char *path)
{
   int r = -1;
   size_t path_len = strlen(path);
   DIR *dir = opendir(path);
   if (dir) {
      struct dirent *p;
      r = 0;
      while (!r && (p=readdir(dir))) {
          int r2 = -1; char *buf; size_t len;
          if (!strcmp(p->d_name, ".") || !strcmp(p->d_name, ".."))  continue;
          len = path_len + strlen(p->d_name) + 2;
          buf = malloc(len);
          if (buf) {
             struct stat statbuf;
             snprintf(buf, len, "%s/%s", path, p->d_name);
             if (!stat(buf, &statbuf)) {
                if (S_ISDIR(statbuf.st_mode)) r2 = delete_directory(buf);
                else r2 = unlink(buf);
             }
             free(buf);
          }
          r = r2;
      }
      closedir(dir);
   }
   if (!r) r = rmdir(path);
   return r;
}

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

// Device 2 Callbaks
static void
wes_prov_cb2(oc_wes_data_t *wes_prov_data, void *user_data)
{
  (void)user_data;
  PRINT("wes_prov_cb2\n");
  if (wes_prov_data == NULL) {
      PRINT("wes_prov_data is NULL\n");
      return;
  }
}

static void
device_prov_cb2(oc_wes_device_data_t *device_prov_data, void *user_data)
{
  (void)user_data;
  PRINT("device_prov_cb2\n");
  if (device_prov_data == NULL) {
      PRINT("device_prov_data is NULL\n");
      return;
  }
  PRINT("Device Name: %s\n", oc_string(device_prov_data->dev_name));
}

static void
wifi_prov_cb2(oc_wes_wifi_data_t *wifi_prov_data, void *user_data)
{
  (void)user_data;
  PRINT("wifi_prov_cb2\n");
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
  wifi_join(oc_string(wifi_prov_data->ssid), oc_string(wifi_prov_data->cred));
  //1 Start DHCP client
  wifi_start_dhcp_client();
}

// resource proisining callbacks for 2 devices
wes_device_callbacks_s g_rsc_cbks[] = {
  {
    .oc_wes_prov_cb_t = &wes_prov_cb1,
    .oc_wes_wifi_prov_cb_t = &wifi_prov_cb1,
    .oc_wes_dev_prov_cb_t = &device_prov_cb1,
  },
  {
    .oc_wes_prov_cb_t = &wes_prov_cb2,
    .oc_wes_wifi_prov_cb_t = &wifi_prov_cb2,
    .oc_wes_dev_prov_cb_t = &device_prov_cb2,
  }
};

static int
app_init(void)
{
  void *user_data = NULL;

  PRINT("Initialize OCF platform\n");
  int err = oc_init_platform("SAMSUNG", NULL, NULL);
  if(err) {
    PRINT("oc_init_platform error %d\n", err);
    return err;
  }

  // oc_create_wifi_easysetup_resource will be called by IoT Core
  // user_data passed here will be returned in resource callbacks,
  // application shall allocate and free the memory for user_data
  PRINT("Add device\n");
  err = oc_add_device("/oic/d", "oic.d.binaryswitch", "Binary Switch", "ocf.1.0.0",
                       "ocf.res.1.0.0", NULL, user_data);
  if(err) {
    PRINT("Add oic.d.binaryswitch device error %d\n", err);
    return err;
  }
/*
  err = oc_add_device("/oic/d", "oic.d.voiceassistant", "Voice Assistant", "ocf.1.0.0",
                       "ocf.res.1.0.0", NULL, user_data);
  if(err) {
    PRINT("Add oic.d.voiceassistant device error %d\n", err);
    return err;
  }
*/
  g_device_count = oc_core_get_num_devices();
  PRINT("Numer of registered  Devices %d\n", g_device_count);
  return err;
}

static void
signal_event_loop(void)
{
  pthread_mutex_lock(&mutex);
  pthread_cond_signal(&cond);
  pthread_mutex_unlock(&mutex);
}

static void
register_resources(void)
{
  wifi_mode supported_mode[NUM_WIFIMODE] = {WIFI_11A, WIFI_11B,WIFI_11G, WIFI_11N, WIFI_11AC, WIFI_MODE_MAX};
  wifi_freq supported_freq[NUM_WIFIFREQ] = {WIFI_24G, WIFI_5G, WIFI_FREQ_MAX};
  char *device_name = "WiFiTestDevice";

  for(int dev_index = 0; dev_index < g_device_count; ++dev_index) {

    // Set callbacks for Resource operations
    oc_wes_set_resource_callbacks(dev_index, g_rsc_cbks[dev_index].oc_wes_prov_cb_t,
          g_rsc_cbks[dev_index].oc_wes_wifi_prov_cb_t, g_rsc_cbks[dev_index].oc_wes_dev_prov_cb_t);

    // Set Device Info
     if (oc_wes_set_device_info(dev_index, supported_mode, supported_freq, device_name) == OC_ES_ERROR)
         PRINT("oc_wes_set_device_info error!\n");
  }
}

static void
handle_signal(int signal)
{
  (void)signal;
  signal_event_loop();
  g_exit = true;
}

void
main(void)
{
  struct sigaction sa;
  sigfillset(&sa.sa_mask);
  sa.sa_flags = 0;
  sa.sa_handler = handle_signal;
  sigaction(SIGINT, &sa, NULL);

  PRINT("wifi_easysetup_enrollee : Start\n");

  //1 TODO : Platform Interface
  wifi_start_softap(SOFT_AP_SSID, SOFT_AP_PSK);
  wifi_start_dhcp_server();

  pthread_mutex_init(&mutex, NULL);
  pthread_cond_init(&cond, NULL);
  // Create OCF handler
  PRINT("Create OCF handler\n");
  static const oc_handler_t handler = {.init = app_init,
                                       .signal_event_loop = signal_event_loop,
                                       .register_resources =  register_resources };


  oc_set_mtu_size(MAX_MTU_SIZE);
  oc_set_max_app_data_size(MAX_APP_DATA_SIZE);

#ifdef OC_SECURITY
  delete_directory("wifi_easysetup_enrollee_creds");
  oc_storage_config("./wifi_easysetup_enrollee_creds");
#endif

  PRINT("oc_main_init\n");

  if (oc_main_init(&handler) < 0) {
    PRINT("oc_main_init failed\n");
    return;
  }

  oc_clock_time_t next_event;

  PRINT("Start loop\n");
  while (g_exit != 1) {
    next_event = oc_main_poll();
    pthread_mutex_lock(&mutex);
    if (next_event == 0) {
      pthread_cond_wait(&cond, &mutex);
    } else {
      ts.tv_sec = (next_event / OC_CLOCK_SECOND);
      ts.tv_nsec = (next_event % OC_CLOCK_SECOND) * 1.e09 / OC_CLOCK_SECOND;
      pthread_cond_timedwait(&cond, &mutex, &ts);
    }
    pthread_mutex_unlock(&mutex);
  }
  PRINT("Initiate main shutdown\n");
  oc_main_shutdown();

  for(int dev_index = 0; dev_index < g_device_count; ++dev_index) {
    oc_delete_wifi_easysetup_resource(dev_index);
  }
  wifi_stop_dhcp_server();

  PRINT("wifi_easysetup_enrollee : Exit\n");

  return;
}
