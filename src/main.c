#include <unistd.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <lo/lo.h>
#include <linux/input.h>

#include "lvgl/lvgl.h"
#include "ui/ui.h"
#include "src/lib/driver_backends.h"
#include "src/lib/simulator_util.h"
#include "src/lib/simulator_settings.h"

#define ADS_THRESHOLD 500

/* contains the name of the selected backend if user
 * has specified one on the command line */
static char *selected_backend;

/* Global simulator settings, defined in lv_linux_backend.c */
extern simulator_settings_t settings;

typedef struct {
    lv_obj_t **panel;  // Pointer to the panel reference
    int16_t grid_x;
    int16_t grid_y;
} PanelInfo;

// Pointers to SquareLine's panel references
static PanelInfo panels[] = {
    {&ui_Channel1, 0, 0},
    {&ui_Channel2, 1, 0},
    {&ui_Channel3, 2, 0},
    {&ui_Channel4, 0, 1},
    {&ui_Channel5, 1, 1},
    {&ui_Channel6, 2, 1}
};

typedef enum {
    SOUND_KICK = 0,
    SOUND_SNARE,
    SOUND_HIHAT,
    SOUND_OPEN_HIHAT,
    SOUND_BASS_TOM,
    SOUND_MED_TOM,
    SOUND_HIGH_TOM,
    SOUND_CRASH,
    SOUND_COUNT  // Total number of sounds
} SoundType;

// Sound names for OSC paths and display
const char* sound_names[SOUND_COUNT] = {
    "Kick",
    "Snare", 
    "Hihat",
    "OpenHihat",
    "BassTom",
    "MedTom",
    "HighTom",
    "Crash"
};


#define NUM_CHANNELS 6
SoundType channel_mapping[NUM_CHANNELS] = {
    SOUND_KICK,      // Channel 0 -> Kick
    SOUND_SNARE,     // Channel 1 -> Snare
    SOUND_HIHAT,     // Channel 2 -> Hihat
    SOUND_OPEN_HIHAT,// Channel 3 -> Open Hihat
    SOUND_BASS_TOM,  // Channel 4 -> Bass Tom
    SOUND_CRASH      // Channel 5 -> Crash
};

static int current_panel_index = 0;
static int triggered_channel = 6;
static int current_screen = 0;

static lv_obj_t* get_panel(uint8_t index) {
    if (index >= sizeof(panels)/sizeof(PanelInfo)) {
        printf("Error: Panel index %d out of bounds\n", index);
        return NULL;
    }
    
    if (panels[index].panel == NULL) {
        printf("Error: Panel %d pointer is NULL\n", index);
        return NULL;
    }
    
    if (*panels[index].panel == NULL) {
        printf("Error: Panel %d object is NULL\n", index);
        return NULL;
    }
    return *panels[index].panel;
}

void display_init(){
    selected_backend = NULL;

    driver_backends_register();

    const char *env_w = getenv("LV_SIM_WINDOW_WIDTH");
    const char *env_h = getenv("LV_SIM_WINDOW_HEIGHT");

    settings.window_width = atoi(env_w ? env_w : "160");
    settings.window_height = atoi(env_h ? env_h : "128");

    lv_init();

    if (driver_backends_init_backend(selected_backend) == -1) {
        die("Failed to initialize display backend");
    }

    ui_init();
}

void set_channel_mapping(int channel, SoundType sound) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        printf("Invalid channel: %d\n", channel);
        return;
    }
    if (sound < 0 || sound >= SOUND_COUNT) {
        printf("Invalid sound: %d\n", sound);
        return;
    }
    
    channel_mapping[channel] = sound;
    printf("Channel %d mapped to %s\n", channel, sound_names[sound]);
}

void setup_sound_roller(lv_obj_t* roller) {
    // Create the options string for the roller
    // LVGL roller expects options separated by '\n'
    char options[256] = "";
    
    for (int i = 0; i < SOUND_COUNT; i++) {
        strcat(options, sound_names[i]);
        if (i < SOUND_COUNT - 1) {
            strcat(options, "\n");
        }
    }
    
    lv_roller_set_options(roller, options, LV_ROLLER_MODE_NORMAL);

    lv_roller_set_selected(roller, channel_mapping[current_panel_index], LV_ANIM_OFF);
}

// Function to update the roller when switching channels
void update_roller_for_channel(lv_obj_t* roller) {
    lv_roller_set_selected(roller, channel_mapping[current_panel_index], LV_ANIM_OFF);
}

void set_channel_trigger(lo_address t, int channel, float value) {
    if (channel < 0 || channel >= NUM_CHANNELS) {
        printf("Invalid channel: %d\n", channel);
        return;
    }
    SoundType sound = channel_mapping[channel];
    const char* sound_name = sound_names[sound];

    char osc_path[256];
    snprintf(osc_path, sizeof(osc_path), "/drumkit/%s", sound_name);
    lo_send(t, osc_path, "f", value);
}


static int16_t prev_ads_values[6] = {0};
static bool ads_triggered[6] = {false};

void process_ads_triggers(lo_address t, int16_t values[6]) {
    for (int ads_ch = 0; ads_ch < 6; ads_ch++) {
        // Obtener el canal Faust correspondiente
        int faust_ch = 6  - ads_ch;
        
        // Saltar si no hay mapeo válido
        if (faust_ch < 0 || faust_ch >= NUM_CHANNELS) {
            continue;
        }
        
        int16_t current_value = values[ads_ch];
        int16_t prev_value = prev_ads_values[ads_ch];
        
        // Detectar rising edge (cruzar el threshold hacia arriba)
        if (!ads_triggered[ads_ch] && current_value > ADS_THRESHOLD && prev_value <= ADS_THRESHOLD) {
            // Activar trigger
            set_channel_trigger(t, faust_ch, 1.0f);
            ads_triggered[ads_ch] = true;
            printf("ADS ch%d triggered -> Faust ch%d (value: %d)\n", ads_ch + 1, faust_ch, current_value);
        }
        // Detectar falling edge (cruzar el threshold hacia abajo)
        else if (ads_triggered[ads_ch] && current_value <= ADS_THRESHOLD && prev_value > ADS_THRESHOLD) {
            // Desactivar trigger
            set_channel_trigger(t, faust_ch, 0.0f);
            ads_triggered[ads_ch] = false;
            printf("ADS ch%d released -> Faust ch%d (value: %d)\n", ads_ch + 1, faust_ch, current_value);
        }
        
        // Actualizar valor anterior
        prev_ads_values[ads_ch] = current_value;
    }
}
void process_keyev(int file,lo_address t) {
    struct input_event ie;
    
    ssize_t bytes_read = read(file, &ie, sizeof(struct input_event));
    
    // Check if we actually read a complete event
    if (bytes_read != sizeof(struct input_event)) {
        // No event available or incomplete read - just return
        return;
    }
    
    if (ie.type == EV_KEY) {
        if(ie.value == 1){
            if(current_screen == 0){
                switch (ie.code) {
                    case KEY_UP: 
                        lv_obj_set_style_border_color(get_panel(current_panel_index), lv_color_hex(0x4ADFF3), LV_PART_MAIN | LV_STATE_DEFAULT);
                        current_panel_index = (current_panel_index-3)+6*(current_panel_index<3);
                        lv_obj_set_style_border_color(get_panel(current_panel_index), lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
                        break;
                    case KEY_DOWN: 
                        lv_obj_set_style_border_color(get_panel(current_panel_index), lv_color_hex(0x4ADFF3), LV_PART_MAIN | LV_STATE_DEFAULT);
                        current_panel_index = (current_panel_index+3)-6*(current_panel_index>=3);
                        lv_obj_set_style_border_color(get_panel(current_panel_index), lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
                        break;
                    case KEY_LEFT: 
                        lv_obj_set_style_border_color(get_panel(current_panel_index), lv_color_hex(0x4ADFF3), LV_PART_MAIN | LV_STATE_DEFAULT);
                        current_panel_index = (current_panel_index-1)+3*(current_panel_index%3==0);
                        lv_obj_set_style_border_color(get_panel(current_panel_index), lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
                        break;
                    case KEY_RIGHT: 
                        lv_obj_set_style_border_color(get_panel(current_panel_index), lv_color_hex(0x4ADFF3), LV_PART_MAIN | LV_STATE_DEFAULT);
                        current_panel_index = (current_panel_index+1)-3*(current_panel_index%3==2);
                        lv_obj_set_style_border_color(get_panel(current_panel_index), lv_color_black(), LV_PART_MAIN | LV_STATE_DEFAULT);
                        break;
                    case KEY_ENTER:
                        lv_screen_load(ui_Screen2);  
                        update_roller_for_channel(ui_Roller1);
                        current_screen = 1;              
                        break;
                    case KEY_ESC: 
                        set_channel_trigger(t,current_panel_index,1);
                        triggered_channel = current_panel_index;
                        break;
                    default:
                        break;
                }
            }else{
                switch (ie.code) {
                    case KEY_UP: 
                        if (channel_mapping[current_panel_index] > 0) {
                            lv_roller_set_selected(ui_Roller1, channel_mapping[current_panel_index]-1,LV_ANIM_OFF);
                            set_channel_mapping(current_panel_index, channel_mapping[current_panel_index]-1);
                        }
                        break;
                    case KEY_DOWN: 
                        if (channel_mapping[current_panel_index] < (SOUND_COUNT-1)) {
                            lv_roller_set_selected(ui_Roller1, channel_mapping[current_panel_index]+1,LV_ANIM_OFF);
                            set_channel_mapping(current_panel_index, channel_mapping[current_panel_index]+1);
                        }
                        break;
                    case KEY_LEFT: 
                        break;
                    case KEY_RIGHT: 
                        break;
                    case KEY_ENTER:
                        lv_screen_load(ui_Screen1);           
                        current_screen = 0;     
                        break;
                    case KEY_ESC: 
                        set_channel_trigger(t,current_panel_index,1);
                        triggered_channel = current_panel_index;
                        break;
                    default:
                        break;
                }
            }

        } else if(ie.value == 0 && ie.code == KEY_ESC){
            set_channel_trigger(t,triggered_channel,0);
        }
    
    }
}

int main(){

    display_init();

    int fADC = ADS1115_init(0);
    if(fADC< 0) return 1;

    int fEv = open("/dev/input/event3", O_RDONLY|O_NONBLOCK);
    if (fEv == -1) {
        perror("Opening /dev/input/event3");
        return EXIT_FAILURE;
    }
    
    lo_address t = lo_address_new("localhost", "5510");

    int channel = 0;

    int level;
    int16_t value;
    uint32_t idle_time;

    lv_obj_set_style_border_color(get_panel(0),lv_color_black(),LV_PART_MAIN | LV_STATE_DEFAULT);

    setup_sound_roller(ui_Roller1);
    set_channel_mapping(0,SOUND_HIGH_TOM);

    int file = ADS1115_init();

    if(file < 0) {
        return 1;
    }

    int s = 0;
    int16_t values[6];

    struct timeval start_time, current_time;
    gettimeofday(&start_time, NULL);
    
    int volume = 100;

    while (true) {
        /* Returns the time to the next timer execution */
        idle_time = lv_timer_handler();

        s++;

        for(int i = 0; i < 3; i++){
            ADS1115_start_reading(i, file);
            ADS1115_start_reading(i+4, file);
            values[i] = ADS1115_get_result(i+1, file);
            values[i+3] = ADS1115_get_result(i+5, file);
        }

        process_ads_triggers(t, values);

        int vpot= 100-(ADS1115_read(channel,fADC)/259);
        if (vpot < (volume - 1)||vpot > (volume+1))
            volume = vpot;
            lv_slider_set_value(ui_Volume,volume,LV_ANIM_OFF);

        process_keyev(fEv,t);

        if (idle_time > 1000) {  // Sanity check
            idle_time = 30;
        }

        gettimeofday(&current_time, NULL);
        long elapsed_us = (current_time.tv_sec - start_time.tv_sec) * 1000000 + 
                        (current_time.tv_usec - start_time.tv_usec);
        
        if(elapsed_us >= 1000000) {
            float elapsed_seconds = elapsed_us / 1000000.0f;
            printf("%.1f SPS\n", s / elapsed_seconds);
            s = 0;
            start_time = current_time;
        }

        usleep(idle_time * 1000);
    }
    return 0;
}
