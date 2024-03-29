/***********************************************************************************************//**
 * \file   main.c
 * \brief  Silicon Labs BT Mesh Empty Example Project
 *
 * This example demonstrates the bare minimum needed for a Blue Gecko BT Mesh C application.
 * The application starts unprovisioned Beaconing after boot
 ***************************************************************************************************
 * <b> (C) Copyright 2017 Silicon Labs, http://www.silabs.com</b>
 ***************************************************************************************************
 * This file is licensed under the Silabs License Agreement. See the file
 * "Silabs_License_Agreement.txt" for details. Before using this software for
 * any purpose, you must agree to the terms of that agreement.
 **************************************************************************************************/

/* Board headers */
#include "init_mcu.h"
#include "init_board.h"
#include "init_app.h"
#include "ble-configuration.h"
#include "board_features.h"

/* Bluetooth stack headers */
#include "bg_types.h"
#include "native_gecko.h"
#include "gatt_db.h"
#include <gecko_configuration.h>
#include <mesh_sizes.h>

/* Libraries containing default Gecko configuration values */
#include "em_emu.h"
#include "em_cmu.h"
#include <em_gpio.h>

/* Device initialization header */
#include "hal-config.h"

#if defined(HAL_CONFIG)
#include "bsphalconfig.h"
#else
#include "bspconfig.h"
#endif

#include "log.h"
/***********************************************************************************************//**
 * @addtogroup Application
 * @{
 **************************************************************************************************/

/***********************************************************************************************//**
 * @addtogroup app
 * @{
 **************************************************************************************************/

// bluetooth stack heap
                    #define MAX_CONNECTIONS 2
// One for bgstack; one for general mesh operation;
// N for mesh GATT service advertisements
#define MAX_ADVERTISERS (2 + 4)

uint8_t bluetooth_stack_heap[DEFAULT_BLUETOOTH_HEAP(MAX_CONNECTIONS) + BTMESH_HEAP_SIZE + 1760];

// bluetooth stack configuration
extern const struct bg_gattdb_def bg_gattdb_data;

// Flag for indicating DFU Reset must be performed
uint8_t boot_to_dfu = 0;

const gecko_configuration_t config =
{
  .bluetooth.max_connections = MAX_CONNECTIONS,
  .bluetooth.max_advertisers = MAX_ADVERTISERS,
  .bluetooth.heap = bluetooth_stack_heap,
  .bluetooth.heap_size = sizeof(bluetooth_stack_heap) - BTMESH_HEAP_SIZE,
  .bluetooth.sleep_clock_accuracy = 100,
  .gattdb = &bg_gattdb_data,
  .btmesh_heap_size = BTMESH_HEAP_SIZE,
#if (HAL_PA_ENABLE) && defined(FEATURE_PA_HIGH_POWER)
  .pa.config_enable = 1, // Enable high power PA
  .pa.input = GECKO_RADIO_PA_INPUT_VBAT, // Configure PA input to VBAT
#endif // (HAL_PA_ENABLE) && defined(FEATURE_PA_HIGH_POWER)
};

static void handle_gecko_event(uint32_t evt_id, struct gecko_cmd_packet *evt);
void mesh_native_bgapi_init(void);
bool mesh_bgapi_listener(struct gecko_cmd_packet *evt);

const uint8_t static_oob_data[16] = {1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16};

int main()
{
  // Initialize device
  initMcu();
  // Initialize board
  initBoard();
  // Initialize application
  initApp();

  gecko_stack_init(&config);
  gecko_bgapi_class_dfu_init();
  gecko_bgapi_class_system_init();
  gecko_bgapi_class_le_gap_init();
  gecko_bgapi_class_le_connection_init();
  gecko_bgapi_class_gatt_init();
  gecko_bgapi_class_gatt_server_init();
  gecko_bgapi_class_endpoint_init();
  gecko_bgapi_class_hardware_init();
  gecko_bgapi_class_flash_init();
  gecko_bgapi_class_test_init();
  gecko_bgapi_class_sm_init();
  mesh_native_bgapi_init();
  gecko_initCoexHAL();

  INIT_LOG();
  LOGI(RTT_CTRL_CLEAR"--------- Compiled - %s - %s ---------\r\n", (uint32_t)__DATE__, (uint32_t)__TIME__);

  while (1) {
    struct gecko_cmd_packet *evt = gecko_wait_event();
    bool pass = mesh_bgapi_listener(evt);
    if (pass) {
      handle_gecko_event(BGLIB_MSG_ID(evt->header), evt);
    }
  }
}

static void handle_gecko_event(uint32_t evt_id, struct gecko_cmd_packet *evt)
{
  switch (evt_id) {
    case gecko_evt_system_boot_id:
    	LOGW("Factory reset started.\r\n");
    	gecko_cmd_flash_ps_erase_all();
    	LOGW("Factory reset done.\r\n");
    	if(gecko_cmd_mesh_prov_init()->result)
    		LOGE("Init Prov Error.\r\n");
    	else
    		LOGD("Init Prov Success.\r\n");
      break;

    case gecko_evt_mesh_prov_initialized_id:
    	if(gecko_cmd_mesh_prov_set_oob_requirements(0,2,0,0,0,0)->result)
    		LOGE("Set OOB requirements Error.\r\n");
    	else
    	    LOGD("Set OOB requirements Success.\r\n");

    	if(gecko_cmd_mesh_prov_scan_unprov_beacons()->result)
    		LOGE("Scan unprov beacons Error.\r\n");
    	else
    		LOGD("Scan unprov beacons Success.\r\n");
    	break;

    case gecko_evt_mesh_prov_oob_auth_request_id:
    	if (gecko_cmd_mesh_prov_oob_auth_rsp(16, static_oob_data)->result)
    		LOGE("ERROR responding to oob request\r\n");
    	else
    		LOGD("Responding to oob request success.\r\n");
    	break;

    case gecko_evt_mesh_prov_unprov_beacon_id: {
    	struct gecko_msg_mesh_prov_unprov_beacon_evt_t *beacon_evt = (struct gecko_msg_mesh_prov_unprov_beacon_evt_t *) &(evt->data);
    	if (beacon_evt->uuid.data[11] == 0xE6) {
    		LOGD("gecko_evt_mesh_prov_unprov_beacon_id-> Provision start...\r\n");
    		uint8_t netkey_id = gecko_cmd_mesh_prov_create_network(0, (const uint8 *) "")->network_id;

    		struct gecko_msg_mesh_prov_provision_device_rsp_t *prov_resp_adv;
    		prov_resp_adv = gecko_cmd_mesh_prov_provision_device(netkey_id, 16, beacon_evt->uuid.data);

    		if (prov_resp_adv->result == 0) {
    			LOGD("Successful call of gecko_cmd_mesh_prov_provision_device\r\n");
    		} else {
    			LOGD("Failed call to provision node. %x\r\n", prov_resp_adv->result);
    		}
    	}
    	break;
    }

	case gecko_evt_mesh_prov_provisioning_failed_id: {
		struct gecko_msg_mesh_prov_provisioning_failed_evt_t *fail_evt = (struct gecko_msg_mesh_prov_provisioning_failed_evt_t*) &(evt->data);
		LOGE("Provisioning failed. Reason: %x\r\n", fail_evt->reason);
		break;
	}

	case gecko_evt_mesh_prov_device_provisioned_id:
		LOGI("Node successfully provisioned.\r\n");
	break;

    case gecko_evt_gatt_server_user_write_request_id:
      if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_ota_control) {
        /* Set flag to enter to OTA mode */
        boot_to_dfu = 1;
        /* Send response to Write Request */
        gecko_cmd_gatt_server_send_user_write_response(
          evt->data.evt_gatt_server_user_write_request.connection,
          gattdb_ota_control,
          bg_err_success);

        /* Close connection to enter to DFU OTA mode */
        gecko_cmd_le_connection_close(evt->data.evt_gatt_server_user_write_request.connection);
      }
      break;
    default:
      break;
  }
}
