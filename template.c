#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>
#include <furi_hal.h>

#define SPEED 10.0f
#define MIN_FREQ 100
#define MAX_FREQ 1000 
#define ALERT_THRESHOLD 200.0f

#include "font.h"
#include "russian.h"

typedef enum {
    PHUnit_PolyLyah,
    PHUnit_Lyah,
    PHUnit_MegaLyah,
    PHUnit_GigaLyah,
} PHUnit;

typedef struct {
    float current_value;
    float target_value;
    PHUnit unit;
    bool is_measuring;
    float measure_progress;
    NotificationApp* notifications;
    float fluctuation;
    uint32_t last_sound_time;
    bool alert_state;
    uint32_t last_alert_time;
} PHMeterState;

typedef struct {
    PHMeterState* state;
    FuriMessageQueue* input_queue;
    ViewPort* view_port;
    Gui* gui;
} PHMeterApp;

void draw_callback(Canvas* canvas, void* ctx) {
    PHMeterApp* app = ctx;
    PHMeterState* state = app->state;
    
    canvas_clear(canvas);
    canvas_set_custom_u8g2_font(canvas, u8g2_font_6x12_t_cyrillic);
    
    const char* unit_names[] = {"Полуляхов", "Ляхов", "Мегаляхов", "Гигаляхов"};
    draw_utf8_str(canvas, 40, 32, unit_names[state->unit]);
    draw_utf8_str(canvas, 2, 12, "ПолнаяХуйняМетр");
    char value_str[32];
    if(state->is_measuring) {
        snprintf(value_str, sizeof(value_str), "%.2lf", (double)state->current_value);
        draw_utf8_str(canvas, 2, 32, value_str);
        
        uint8_t bar_width = (uint8_t)((state->measure_progress / 250.0f) * 115);
        canvas_draw_box(canvas, 2, 40, bar_width, 6);
    } else {
        draw_utf8_str(canvas, 2, 32, "---");
    }
}

void input_callback(InputEvent* input_event, void* ctx) {
    PHMeterApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

static void update_alerts(PHMeterApp* app) {
    PHMeterState* state = app->state;
    uint32_t current_time = furi_get_tick();
    
    if(state->measure_progress >= ALERT_THRESHOLD) {
        if(current_time - state->last_alert_time > 100) {
            state->alert_state = !state->alert_state;
            state->last_alert_time = current_time;
            
            if(state->alert_state) {
                notification_message(state->notifications, &sequence_set_only_red_255);
                notification_message(state->notifications, &sequence_set_vibro_on);
            } else {
                notification_message(state->notifications, &sequence_reset_red);
                notification_message(state->notifications, &sequence_reset_vibro);
            }
        }
    } else {
        if(state->alert_state) {
            notification_message(state->notifications, &sequence_reset_vibro);
            notification_message(state->notifications, &sequence_reset_red);
            state->alert_state = false;
        }
    }
}

static void play_smooth_sound(float progress) {
    if(!furi_hal_speaker_is_mine()) {
        if(!furi_hal_speaker_acquire(1000)) {
            return;
        }
    }
    
    float progress_normalized = progress / 250.0f;
    if(progress_normalized > 1.0f) progress_normalized = 1.0f;
    if(progress_normalized < 0.0f) progress_normalized = 0.0f;
    
    uint32_t frequency = MIN_FREQ + (uint32_t)(progress_normalized * (MAX_FREQ - MIN_FREQ));
    
    if(frequency < MIN_FREQ) frequency = MIN_FREQ;
    if(frequency > MAX_FREQ) frequency = MAX_FREQ;
    
    furi_hal_speaker_start(frequency, 0.5f);
}

static void stop_sound() {
    if(furi_hal_speaker_is_mine()) {
        furi_hal_speaker_stop();
        furi_hal_speaker_release();
    }
}

static void update_measurement(PHMeterApp* app) {
    PHMeterState* state = app->state;
    uint32_t current_time = furi_get_tick();
    
    if(state->is_measuring) {
        if(state->measure_progress < state->target_value) {
            state->measure_progress += SPEED;
            if(state->measure_progress > state->target_value) 
                state->measure_progress = state->target_value;
        }
        
        state->fluctuation = (float)(rand() % 100) / 100.0f - 0.5f;
        state->current_value = state->measure_progress + state->fluctuation;
        
        if(current_time - state->last_sound_time > 100) {
            FURI_CRITICAL_ENTER();
            play_smooth_sound(state->measure_progress);
            FURI_CRITICAL_EXIT();
            state->last_sound_time = current_time;
        }
        update_alerts(app);
    } else {
        stop_sound();
        notification_message(state->notifications, &sequence_reset_vibro);
        notification_message(state->notifications, &sequence_reset_red);
    }
}

int32_t phmetr_app(void* p) {
    UNUSED(p);
    PHMeterApp* app = malloc(sizeof(PHMeterApp));
    app->state = malloc(sizeof(PHMeterState));
    
    app->state->current_value = 0.0f;
    app->state->target_value = 250.0f;
    app->state->unit = PHUnit_PolyLyah;
    app->state->is_measuring = false;
    app->state->measure_progress = 0.0f;
    app->state->fluctuation = 0.0f;
    app->state->notifications = furi_record_open(RECORD_NOTIFICATION);
    app->state->last_sound_time = 0;
    app->state->alert_state = false;
    app->state->last_alert_time = 0;
    
    app->input_queue = furi_message_queue_alloc(8, sizeof(InputEvent));
    app->view_port = view_port_alloc();
    view_port_draw_callback_set(app->view_port, draw_callback, app);
    view_port_input_callback_set(app->view_port, input_callback, app);
    
    app->gui = furi_record_open(RECORD_GUI);
    gui_add_view_port(app->gui, app->view_port, GuiLayerFullscreen);
    
    InputEvent input;
    while(1) {
        if(furi_message_queue_get(app->input_queue, &input, 50) == FuriStatusOk) {
            if(input.key == InputKeyOk) {
                if(input.type == InputTypePress) {
                    app->state->is_measuring = true;
                    app->state->current_value = 0.0f;
                    app->state->measure_progress = 0.0f;
                } else if(input.type == InputTypeRelease) {
                    app->state->is_measuring = false;
                    stop_sound();
                    notification_message(app->state->notifications, &sequence_reset_vibro);
                    notification_message(app->state->notifications, &sequence_reset_red);
                }
            } else if(input.type == InputTypePress) {
                switch(input.key) {
                    case InputKeyRight:
                        app->state->unit = (app->state->unit + 1) % 4;
                        break;
                    case InputKeyLeft:
                        app->state->unit = (app->state->unit + 3) % 4;
                        break;
                    case InputKeyUp:
                        app->state->target_value += 10.0f;
                        if(app->state->target_value > 250.0f) app->state->target_value = 250.0f;
                        break;
                    case InputKeyDown:
                        app->state->target_value -= 10.0f;
                        if(app->state->target_value < 1.0f) app->state->target_value = 1.0f;
                        break;
                    case InputKeyBack:
                        goto exit;
                    case InputKeyMAX:
                        break;
                    default:
                        break;
                }
            }
        }
        
        if(app->state->is_measuring) {
            update_measurement(app);
        }
        view_port_update(app->view_port);
    }

exit:
    furi_message_queue_free(app->input_queue);
    view_port_enabled_set(app->view_port, false);
    gui_remove_view_port(app->gui, app->view_port);
    view_port_free(app->view_port);
    stop_sound();
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app->state);
    free(app);
    
    return 0;
}