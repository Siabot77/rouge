#include <stdio.h>
#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <flipper_format/flipper_format.h>
#include "gpio_item.h"
#include "zeitraffer_icons.h"

#define CONFIG_FILE_DIRECTORY_PATH "/ext/apps_data/intravelometer"
#define CONFIG_FILE_PATH CONFIG_FILE_DIRECTORY_PATH "/intravelometer.conf"

// Part of code stolen from https://github.com/zmactep/flipperzero-hello-world

int32_t Time = 10; // Timer
int32_t Count = 10; // Number of frames
int32_t WorkTime = 0; // Timer counter
int32_t WorkCount = 0; // Frame counter
bool InfiniteShot = false; // Infinite shooting
bool Bulb = false; // BULB Mode
int32_t Backlight = 0; // Backlight: on/off/auto
int32_t Delay = 3; // delay to bounce
bool Work = false;

const NotificationSequence sequence_click = {
    &message_note_c7,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

typedef enum {
    EventTypeTick,
    EventTypeInput,
} EventType;

typedef struct {
    event type;
    InputEvent input;
} ZetrafferEvent;

static void draw_callback(Canvas* canvas, void* ctx) {
    UNUSED(ctx);
    char temp_str[36];
    canvas_clear(canvas);
    canvas_set_font(canvas, FontPrimary);
    switch(Count) {
    case -1:
        snprintf(temp_str, sizeof(temp_str), "Set: BULB %li sec", Time);
        break;
    case 0:
        snprintf(temp_str, sizeof(temp_str), "Set: infinite, %li sec", Time);
        break;
    default:
        snprintf(temp_str, sizeof(temp_str), "Set: %li frames, %li sec", Count, Time);
    }
    canvas_draw_str(canvas, 3, 15, temp_str);
    snprintf(temp_str, sizeof(temp_str), "Left: %li frames, %li sec", WorkCount, WorkTime);
    canvas_draw_str(canvas, 3, 35, temp_str);

    switch(Backlight) {
    case 1:
        canvas_draw_str(canvas, 13, 55, "ON");
        break;
    case 2:
        canvas_draw_str(canvas, 13, 55, "OFF");
        break;
    default:
        canvas_draw_str(canvas, 13, 55, "AUTO");
    }

    if(Work) {
        canvas_draw_icon(canvas, 85, 41, &I_ButtonUpHollow_7x4);
        canvas_draw_icon(canvas, 85, 57, &I_ButtonDownHollow_7x4);
        canvas_draw_icon(canvas, 59, 48, &I_ButtonLeftHollow_4x7);
        canvas_draw_icon(canvas, 72, 48, &I_ButtonRightHollow_4x7);
    } else {
        canvas_draw_icon(canvas, 85, 41, &I_ButtonUp_7x4);
        canvas_draw_icon(canvas, 85, 57, &I_ButtonDown_7x4);
        canvas_draw_icon(canvas, 59, 48, &I_ButtonLeft_4x7);
        canvas_draw_icon(canvas, 72, 48, &I_ButtonRight_4x7);
    }

    canvas_draw_icon(canvas, 3, 48, &I_Pin_star_7x7);

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 65, 55, "F");

    canvas_set_font(canvas, FontPrimary);
    canvas_draw_str(canvas, 85, 55, "S");

    //canvas_draw_icon(canvas, 59, 48, &I_ButtonLeft_4x7);
    //canvas_draw_icon(canvas, 72, 48, &I_ButtonRight_4x7);

    if(Work) {
        canvas_draw_icon(canvas, 106, 46, &I_loading_10px);
    }
}

static void input_callback(InputEvent* input_event, void* ctx) {
    // Check if the context is not null
    furi_assert(ctx);
    FuriMessageQueue* event_queue = ctx;

    ZeitrafferEvent event = {.type = EventTypeInput, .input = *input_event};
    furi_message_queue_put(event_queue, &event, FuriWaitForever);
}

static void timer_callback(FuriMessageQueue* event_queue) {
    // Check if the context is not null
    furi_assert(event_queue);

    ZeitrafferEvent event = {.type = EventTypeTick};
    furi_message_queue_put(event_queue, &event, 0);
}

int32_t zeitraffer_app(void* p) {
    UNUSED(p);

    // Current event of custom type ZeitrafferEvent
    ZetrafferEvent event;
    // Queue of events for 8 elements of size ZeitrafferEvent
    FuriMessageQueue* event_queue = furi_message_queue_alloc(8, sizeof(ZeitrafferEvent));

    // Create a new viewport
    ViewPort* view_port = view_port_alloc();
    // Create a draw callback, no context
    view_port_draw_callback_set(view_port, draw_callback, NULL);
    // Create a callback for keystrokes, pass as context
    // our message queue to push these events into it
    view_port_input_callback_set(view_port, input_callback, event_queue);

    // Create a GUI application
    Gui* gui = furi_record_open(RECORD_GUI);
    // Connect view port to GUI in full screen mode
    gui_add_view_port(gui, view_port, GuiLayerFullscreen);

    // Configure pins
    gpio_item_configure_all_pins(GpioModeOutputPushPull);

    // Create a periodic timer with a callback, where as
    // context will be passed to our event queue
    FuriTimer* timer = furi_timer_alloc(timer_callback, FuriTimerTypePeriodic, event_queue);
    // Start timer
    //furi_timer_start(timer, 1500);

    // Enable notifications
    NotificationApp* notifications = furi_record_open(RECORD_NOTIFICATION);

    Storage* storage = furi_record_open(RECORD_STORAGE);

    // Load settings
    FlipperFormat* load = flipper_format_file_alloc(storage);

    do {
        if(!storage_simply_mkdir(storage, CONFIG_FILE_DIRECTORY_PATH)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_file_open_existing(load, CONFIG_FILE_PATH)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_read_int32(load, "Time", &Time, 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_read_int32(load, "Count", &Count, 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_read_int32(load, "Backlight", &Backlight, 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_read_int32(load, "Delay", &Delay, 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        notification_message(notifications, &sequence_success);

    } while(0);

    flipper_format_free(load);

    // Infinite event queue processing loop
    while(1) {
        // Select an event from the queue into the event variable (wait indefinitely if the queue is empty)
        // and check that we managed to do it
        furi_check(furi_message_queue_get(event_queue, &event, FuriWaitForever) == FuriStatusOk);

        // Our event is a button click
        if(event.type == EventTypeInput) {
            if(event.input.type == InputTypeShort) { // Short presses

                if(event.input.key == InputKeyBack) {
                    if(Work) { // If the timer is running - don't touch the buttons!
                        notification_message(notifications, &sequence_error);
                    } else {
                        WorkCount = Count;
                        WorkTime = 3;
                        if(Count == 0) {
                            InfiniteShot = true;
                            WorkCount = 1;
                        } else
                            InfiniteShot = false;

                        notification_message(notifications, &sequence_success);
                    }
                }
                if(event.input.key == InputKeyRight) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_error);
                    } else {
                        Count++;
                        notification_message(notifications, &sequence_click);
                    }
                }
                if(event.input.key == InputKeyLeft) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_error);
                    } else {
                        Count--;
                        notification_message(notifications, &sequence_click);
                    }
                }
                if(event.input.key == InputKeyUp) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_error);
                    } else {
                        Time++;
                        notification_message(notifications, &sequence_click);
                    }
                }
                if(event.input.key == InputKeyDown) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_error);
                    } else {
                        time--;
                        notification_message(notifications, &sequence_click);
                    }
                }
                if(event.input.key == InputKeyOk) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_click);
                        furi_timer_stop(timer);
                        work = false;
                    } else {
                        furi_timer_start(timer, 1000);
                        work = true;

                        if(WorkCount == 0) WorkCount = Count;

                        if(WorkTime == 0) worktime = delay;

                        if(Count == 0) {
                            InfiniteShot = true;
                            WorkCount = 1;
                        } else
                            InfiniteShot = false;

                        if(Count == -1) {
                            gpio_item_set_pin(4, true);
                            gpio_item_set_pin(5, true);
                            bulb = true;
                            WorkCount = 1;
                            worktime = time;
                        } else
                            bulb = false;

                        notification_message(notifications, &sequence_success);
                    }
                }
            }
            if(event.input.type == InputTypeLong) { // Long presses
                // If the "back" button is pressed, then we exit the loop, and therefore the application
                if(event.input.key == InputKeyBack) {
                    if(furi_timer_is_running(
                           timer)) { // And if the timer is running, don't exit :D
                        notification_message(notifications, &sequence_error);
                    } else {
                        notification_message(notifications, &sequence_click);
                        gpio_item_set_all_pins(false);
                        furi_timer_stop(timer);
                        notification_message(
                            notifications, &sequence_display_backlight_enforce_auto);
                        break;
                    }
                }
                if(event.input.key == InputKeyOk) {
                    // We don't need your highlighting! Or is it needed?
                    backlight++;
                    if(Backlight > 2) Backlight = 0;
                }
            }

            if(event.input.type == InputTypeRepeat) { // Pressed buttons
                if(event.input.key == InputKeyRight) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_error);
                    } else {
                        Count = Count + 10;
                    }
                }
                if(event.input.key == InputKeyLeft) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_error);
                    } else {
                        Count = Count - 10;
                    }
                }
                if(event.input.key == InputKeyUp) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_error);
                    } else {
                        Time = Time + 10;
                    }
                }
                if(event.input.key == InputKeyDown) {
                    if(furi_timer_is_running(timer)) {
                        notification_message(notifications, &sequence_error);
                    } else {
                        time = time - 10;
                    }
                }
            }
        }

        // Our event is a fired timer
        else if(event.type == EventTypeTick) {
            WorkTime--;

            if(WorkTime < 1) { // take a photo
                notification_message(notifications, &sequence_blink_white_100);
                if(bulb) {
                    gpio_item_set_all_pins(false);
                    WorkCount = 0;
                } else {
                    WorkCount--;
                    view_port_update(view_port);
                    notification_message(notifications, &sequence_click);
                    // kicking legs
                    //gpio_item_set_all_pins(true);
                    gpio_item_set_pin(4, true);
                    gpio_item_set_pin(5, true);
                    furi_delay_ms(400); // The camera does not respond well to short presses
                    gpio_item_set_pin(4, false);
                    gpio_item_set_pin(5, false);
                    //gpio_item_set_all_pins(false);

                    if(InfiniteShot) WorkCount++;

                    worktime = time;
                    view_port_update(view_port);
                }
            } else {
                // Send notification of blinking blue LED
                notification_message(notifications, &sequence_blink_blue_100);
            }

            if(WorkCount < 1) { // finished
                work = false;
                gpio_item_set_all_pins(false);
                furi_timer_stop(timer);
                notification_message(notifications, &sequence_audiovisual_alert);
                WorkTime = 3;
                WorkCount = 0;
            }

            switch(Backlight) { // what about the backlight?
            case 1:
                notification_message(notifications, &sequence_display_backlight_on);
                break;
            case 2:
                notification_message(notifications, &sequence_display_backlight_off);
                break;
            default:
                notification_message(notifications, &sequence_display_backlight_enforce_auto);
            }
        }
        if(Time < 1) Time = 1; // We do not allow to unscrew the timer less than one
        if(Count < -1)
            Count =
                0; // And here we give, more than 0 frames is an endless shooting, and -1 frames - BULB
    }

    // We store the settings
    FlipperFormat* save = flipper_format_file_alloc(storage);

    do {
        if(!flipper_format_file_open_always(save, CONFIG_FILE_PATH)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_write_header_cstr(save, "Zeitraffer", 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_write_comment_cstr(
               save,
               "Zeitraffer app settings: # of frames, interval time, backlight type, Delay")) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_write_int32(save, "Time", &Time, 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_write_int32(save, "Count", &Count, 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_write_int32(save, "Backlight", &Backlight, 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }
        if(!flipper_format_write_int32(save, "Delay", &Delay, 1)) {
            notification_message(notifications, &sequence_error);
            break;
        }

    } while(0);

    flipper_format_free(save);

    furi_record_close(RECORD_STORAGE);

    // Clear the timer
    furi_timer_free(timer);

    // Special cleanup of memory occupied by the queue
    furi_message_queue_free(event_queue);

    // Clean up the created objects associated with the interface
    gui_remove_view_port(gui, view_port);
    view_port_free(view_port);
    furi_record_close(RECORD_GUI);

    // Clear notifications
    furi_record_close(RECORD_NOTIFICATION);

    return 0;
}