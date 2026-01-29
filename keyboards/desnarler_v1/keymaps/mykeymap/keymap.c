// Copyright 2023 QMK
// SPDX-License-Identifier: GPL-2.0-or-later

#include "analog.h"
#include QMK_KEYBOARD_H

// ----------------------
// Slider setup
// ----------------------
#define SLIDER_PIN 29

static uint32_t last_vol_change = 0;
static bool slider_ready = false;
const int16_t center = 512;
const int16_t dead_zone = 70;

// ----------------------
// LEDs
// ----------------------
#define LED1_PIN 26 // left LED
#define LED2_PIN 28
#define LED3_PIN 27 // right LED

// ----------------------
// Switch
// ----------------------
#define LAYER_SWITCH_PIN GP0
static bool switch_on = false;

// ----------------------
// Layer definitions
// ----------------------
#define _MEET 0
#define _WORK 1
#define TAPPING_TERM 200
#include QMK_KEYBOARD_H

// --- 1. Define Your Setup ---
// How long (ms) to wait before deciding if it's a Tap or Hold
#define TAPPING_TERM 200 

// Define Layers
enum layers {
    _MEET = 0, // Profile A: Google Meet
    _WORK      // Profile B: Deep Work
};

// Define Custom Keycodes for Tap Dance
// Custom Keycode
enum custom_keycodes {
    TD_MIC_PTT = 0,  // Tap: Mute (Ctrl+D), Hold: Push-to-Talk (Space)
    TD_CAM_OPT,      // Tap: Cam (Ctrl+E),  Hold: Participants (Ctrl+Alt+P)
    TD_HND_CHT,      // Tap: Hand (C+A+H),  Hold: Chat (C+A+C)
    TD_LEAVE,         // Tap: Tile View,     Hold: Close Tab (Safety)
    // Work Mode Keys
    CK_MEDIA,    // Tap: Play/Pause, Hold: Prev Track
    CK_AUDIO,    // Tap: Mute System, Hold: Next Track
    CK_COPY,     // Tap: Copy, Hold: Paste
    CK_SHOT      // Tap: Screenshot, Hold: Alt-Tab
};

// --- 2. Tap Dance Logic (The Complex Part) ---

// A. MIC: Tap = Toggle Mute (Ctrl+D), Hold = Push-to-Talk (Space)
void mic_finished(qk_tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) { // Single Tap
        if (state->pressed) { // Key is still held down -> HOLD ACTION
            register_code(KC_SPACE); 
        } else {              // Key was released -> TAP ACTION
            tap_code16(LCTL(KC_D));
        }
    }
}

void mic_reset(qk_tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        if (state->pressed) { // Release the Hold
            unregister_code(KC_SPACE);
        }
    }
}

// B. CAM: Tap = Toggle Cam (Ctrl+E), Hold = Participants (Ctrl+Alt+P)
void cam_finished(qk_tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        if (state->pressed) { // HOLD
             register_code(KC_LCTRL); register_code(KC_LALT); register_code(KC_P);
        } else {              // TAP
            tap_code16(LCTL(KC_E));
        }
    }
}

void cam_reset(qk_tap_dance_state_t *state, void *user_data) {
    if (state->pressed) { // Cleanup Hold
        unregister_code(KC_P); unregister_code(KC_LALT); unregister_code(KC_LCTRL);
    }
}

// C. HAND: Tap = Raise Hand, Hold = Chat
void hand_finished(qk_tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        if (state->pressed) { // HOLD: Chat
             tap_code16(LCA(KC_C)); // Just a tap needed to toggle chat pane
        } else {              // TAP: Raise Hand
             tap_code16(LCA(KC_H));
        }
    }
}

// D. LEAVE: Tap = Tile View, Hold = Close Tab (Safety)
void leave_finished(qk_tap_dance_state_t *state, void *user_data) {
    if (state->count == 1) {
        if (state->pressed) { // HOLD: Close Tab (Ctrl+W)
             tap_code16(LCTL(KC_W));
        } else {              // TAP: Tile View (Usually just click, but mapping Shift+? for Help)
             tap_code16(LSFT(KC_SLSH)); // Opens Shortcuts menu as placeholder
        }
    }
}

// Register the functions
qk_tap_dance_action_t tap_dance_actions[] = {
    [TD_MIC_PTT] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, mic_finished, mic_reset),
    [TD_CAM_OPT] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, cam_finished, cam_reset),
    [TD_HND_CHT] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, hand_finished, NULL), // No reset needed for simple taps
    [TD_LEAVE]   = ACTION_TAP_DANCE_FN_ADVANCED(NULL, leave_finished, NULL)
};


// --- 3. The Keymap Array ---

const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    /* Profile A: Google Meet
     * Switch Position 1 (OFF) defaults to this layer
     * -----------------------------------------------------------
     * |  MIC/PTT  |  CAM/PART  |  HAND/CHAT  |  TILE/LEAVE  |
     * -----------------------------------------------------------
     */
    [_MEET] = LAYOUT(
        TD(TD_MIC_PTT),  TD(TD_CAM_OPT),  TD(TD_HND_CHT),  TD(TD_LEAVE)
    ),

    /* Profile B: Deep Work
     * Switch Position 2 (ON) triggers this layer
     * -----------------------------------------------------------
     * |  PLAY/PREV  |  MUTE/NEXT |  COPY/PASTE |  SHOT/APP  |
     * -----------------------------------------------------------
     */
    [_WORK] = LAYOUT( MT(MOD_LCTL, KC_MPLY), KC_MUTE, LCTL_T(KC_C), LWIN(LSFT(KC_S)) 
    )
};


// --- 4. The Switch Logic (process_record_user) ---

// Assuming the physical switch is wired to the matrix at index 4 (or a specific pin)
// Let's assume it's "Key 5" in the matrix for this example.
// If it is a LATCHING switch, it stays "pressed" when ON.

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Handling the Physical Profile Switch
    // Replace KC_F24 with whatever key your switch is wired to in the matrix
    if (keycode == KC_F24) { 
        if (record->event.pressed) {
            layer_on(_WORK); // Switch ON = Work Profile
        } else {
            layer_off(_WORK); // Switch OFF = Meet Profile
        }
        return false; // Don't send F24 to computer
    }
    
    // Manual "Tap vs Hold" override (The code structure you asked for)
    // ONLY necessary if you refuse to use Tap Dance above.
    /*
    switch (keycode) {
        case SOME_CUSTOM_KEY:
            // This is the implementation of your snippet
            if (record->tap.count == 0) { // This check isn't standard in process_record, use timers!
                 // See below for explanation
            }
            break;
    }
    */
    
    return true;
}

void initial_blink(void){
    for (int i = 0; i < 10; i++) {
        writePinHigh(LED1_PIN);
        writePinLow(LED2_PIN);
        writePinHigh(LED3_PIN);
        wait_ms(200);
        writePinLow(LED1_PIN);
        writePinHigh(LED2_PIN);
        writePinLow(LED3_PIN);
        wait_ms(200);
    }
    writePinLow(LED2_PIN);
}

// ----------------------
// Called once at boot
// ----------------------
void matrix_init_user(void) {
    // Initialize OS Switch
    setPinInputHigh(LAYER_SWITCH_PIN);

    // Initialize LEDs
    setPinOutput(LED1_PIN);
    setPinOutput(LED2_PIN);
    setPinOutput(LED3_PIN);

    initial_blink();
}

// ----------------------
// Called when layer changes
// ----------------------
layer_state_t layer_state_set_user(layer_state_t state) {
    // Store the current layer for use in matrix_scan_user
    is_work_profile = get_highest_layer(state) == _WORK;

    // Control LED1 and LED2 based on mode
    writePinLow(LED1_PIN);
    writePinLow(LED2_PIN);

    // Show LED1 in meet mode, LED2 in work mode
    if (is_work_profile) {
        writePinHigh(LED2_PIN);
    } else {
        writePinHigh(LED1_PIN);
    }

    return state;
}
// --- Global Variables to track state ---
static uint16_t key_timer;          // Tracks when a key was pressed
static bool     is_work_profile = false; // False = Meet Mode, True = Work Mode
static uint32_t last_key_press = 0; // Tracks when the last key was pressed
#define KEY_LED_TIMEOUT 500 // LED3 stays on for 500ms after key press
// ----------------------
// matrix_scan_user: repeated loop
// ----------------------
void matrix_scan_user(void) {
    bool new_mode = !readPin(LAYER_SWITCH_PIN); 
    if (new_mode != switch_on) {
        switch_on           = new_mode;
        uint8_t target_base = switch_on ? _WORK : _MEET;
        layer_move(target_base); // activate the correct base layer
    }

    // ------ read slider --------
    int16_t raw = analogReadPin(SLIDER_PIN);
    if (timer_elapsed(last_vol_change) < 100){
        return ;
    }
   
    if (raw < center - dead_zone){
        tap_code(KC_AUDIO_VOL_DOWN);
        last_vol_change = timer_read32();
    }
    else if (raw > center + dead_zone){
        tap_code(KC_AUDIO_VOL_UP);
        last_vol_change = timer_read32();
    }

    // ------ Update LED3 based on key press timing ------
    if (timer_elapsed32(last_key_press) < KEY_LED_TIMEOUT) {
        writePinHigh(LED3_PIN);
    } else {
        writePinLow(LED3_PIN);
    }
}