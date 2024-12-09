#include <furi.h>
#include <gui/gui.h>
#include <input/input.h>
#include <notification/notification_messages.h>

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
    canvas_set_font(canvas, FontPrimary);
    
    const char* unit_names[] = {"PolyLyahi", "Lyahi", "MegaLyahi", "GigaLyahi"};
    canvas_draw_str(canvas, 2, 12, unit_names[state->unit]);
    canvas_draw_str(canvas, 60, 12, "XyeTA");
    char value_str[32];
    if(state->is_measuring) {
        snprintf(value_str, sizeof(value_str), "%.2lf", (double)state->current_value);
        canvas_draw_str(canvas, 2, 32, value_str);
        
        uint8_t bar_width = (uint8_t)((state->measure_progress / 250.0f) * 100);
        canvas_draw_box(canvas, 2, 40, bar_width, 6);
    } else {
        canvas_draw_str(canvas, 2, 32, "---");
    }
}

void input_callback(InputEvent* input_event, void* ctx) {
    PHMeterApp* app = ctx;
    furi_message_queue_put(app->input_queue, input_event, FuriWaitForever);
}

static const NotificationSequence sequence_c4 = {
    &message_note_c4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_d4 = {
    &message_note_d4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_e4 = {
    &message_note_e4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_f4 = {
    &message_note_f4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_g4 = {
    &message_note_g4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_a4 = {
    &message_note_a4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_b4 = {
    &message_note_b4,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static const NotificationSequence sequence_c5 = {
    &message_note_c5,
    &message_delay_50,
    &message_sound_off,
    NULL,
};

static void play_note_by_progress(NotificationApp* notifications, float progress) {
    const NotificationSequence* sequences[] = {
        &sequence_c4,
        &sequence_d4,
        &sequence_e4,
        &sequence_f4,
        &sequence_g4,
        &sequence_a4,
        &sequence_b4,
        &sequence_c5
    };
    
    int note_index = (int)((progress / 250.0f) * 7);
    if(note_index < 0) note_index = 0;
    if(note_index > 7) note_index = 7;
    
    notification_message(notifications, sequences[note_index]);
}

static void update_measurement(PHMeterApp* app) {
    PHMeterState* state = app->state;
    uint32_t current_time = furi_get_tick();
    
    if(state->is_measuring) {
        if(state->measure_progress < state->target_value) {
            state->measure_progress += 5.0f;
            if(state->measure_progress > state->target_value) state->measure_progress = state->target_value;
        }
        
        state->fluctuation = (float)(rand() % 100) / 100.0f - 0.5f;
        state->current_value = state->measure_progress + state->fluctuation;
        
        // Play sound every 100ms
        if(current_time - state->last_sound_time > 100) {
            play_note_by_progress(state->notifications, state->measure_progress);
            state->last_sound_time = current_time;
        }
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
    furi_record_close(RECORD_GUI);
    furi_record_close(RECORD_NOTIFICATION);
    free(app->state);
    free(app);
    
    return 0;
}