#include "../nfc_i.h"
#include <dolphin/dolphin.h>
#include <applications/gui/view_dispatcher_i.h>

#define NFC_MF_UL_DATA_NOT_CHANGED (0UL)
#define NFC_MF_UL_DATA_CHANGED (1UL)

void nfc_emulate_mifare_ul_worker_callback(NfcWorkerEvent event, void* context) {
    UNUSED(event);
    Nfc* nfc = context;

    if(event == NfcWorkerEventSuccess) {
        scene_manager_set_scene_state(
            nfc->scene_manager, NfcSceneEmulateMifareUl, NFC_MF_UL_DATA_CHANGED);
    } else if(event == NfcWorkerEventPwdAuth) {
        MfUltralightAuth* auth = nfc_worker_get_extra_context(nfc->worker);
        if(auth != NULL) {
            nfc_text_store_set(
                nfc,
                "AUTH: %02x %02x %02x %02x",
                auth->pwd.raw[0],
                auth->pwd.raw[1],
                auth->pwd.raw[2],
                auth->pwd.raw[3]);
            // HACK: force viewport update
            view_port_update(nfc->view_dispatcher->view_port);
        }
    }
}

void nfc_scene_emulate_mifare_ul_on_enter(void* context) {
    Nfc* nfc = context;
    DOLPHIN_DEED(DolphinDeedNfcEmulate);

    // Setup view
    Popup* popup = nfc->popup;
    if(strcmp(nfc->dev->dev_name, "")) {
        nfc_text_store_set(nfc, "%s", nfc->dev->dev_name);
    }
    popup_set_icon(popup, 0, 3, &I_RFIDDolphinSend_97x61);
    popup_set_header(popup, "Emulating\nMf Ultralight", 56, 31, AlignLeft, AlignTop);
    popup_set_text(popup, nfc->text_store, 30, 56, AlignLeft, AlignTop);

    // Setup and start worker
    view_dispatcher_switch_to_view(nfc->view_dispatcher, NfcViewPopup);
    nfc_worker_start(
        nfc->worker,
        NfcWorkerStateEmulateMifareUltralight,
        &nfc->dev->dev_data,
        nfc_emulate_mifare_ul_worker_callback,
        nfc);
    nfc_blink_start(nfc);
}

bool nfc_scene_emulate_mifare_ul_on_event(void* context, SceneManagerEvent event) {
    Nfc* nfc = context;
    bool consumed = false;

    if(event.type == SceneManagerEventTypeTick) {
        consumed = true;
    } else if(event.type == SceneManagerEventTypeBack) {
        // Stop worker
        nfc_worker_stop(nfc->worker);
        // Check if data changed and save in shadow file
        if(scene_manager_get_scene_state(nfc->scene_manager, NfcSceneEmulateMifareUl) ==
           NFC_MF_UL_DATA_CHANGED) {
            scene_manager_set_scene_state(
                nfc->scene_manager, NfcSceneEmulateMifareUl, NFC_MF_UL_DATA_NOT_CHANGED);
            nfc_device_save_shadow(nfc->dev, nfc->dev->dev_name);
        }
        consumed = false;
    }
    return consumed;
}

void nfc_scene_emulate_mifare_ul_on_exit(void* context) {
    Nfc* nfc = context;

    // Clear view
    popup_reset(nfc->popup);

    nfc_blink_stop(nfc);
}