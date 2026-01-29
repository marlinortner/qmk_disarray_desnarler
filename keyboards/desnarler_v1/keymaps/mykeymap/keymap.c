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
#define BASE_LAYER_1 0
#define BASE_LAYER_2 4
#define TAPPING_TERM 200
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
// ----------------------
// Keymap
// ----------------------
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
    [_WORK] = LAYOUT(
        // Simple Mod-Taps work here for standard modifiers
        // Tap: Play, Hold: Prev Track
        MT(MOD_LCTL, KC_MPLY), KC_MUTE, LCTL_T(KC_C), LWIN(LSFT(KC_S)) 
    )
};

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

    // Only run if the state has changed to prevent spamming layer_move
    if (new_mode != switch_on) {
        switch_on = new_mode;
        if (switch_on) {
            layer_move(_WORK); // Clears other layers, jumps to WORK
        } else {
            layer_move(_MEET); // Clears other layers, jumps to MEET
        }
    } 
    // ------ start slider once ------
    if (!slider_ready){
        last_vol_change = timer_read32();
        slider_ready = true;
        return ;
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
// --- 7. The Tap/Hold Logic (Runs on Key Press) ---
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    // Track any key press to light up LED3
    if (record->event.pressed) {
        last_key_press = timer_read32();
    }
    
    // We only care about custom keys. Standard keys (Work Layer) are handled automatically by QMK.
    switch (keycode) {
        
        // --- MIC: Tap=Mute (Ctrl+D), Hold=PTT (Space) ---
        case CK_MIC:
            if (record->event.pressed) {
                key_timer = timer_read();
                register_code(KC_SPACE); // Immediate PTT start
            } else {
                unregister_code(KC_SPACE); // PTT stop
                if (timer_elapsed(key_timer) < HOLD_TIME) {
                    tap_code16(LCTL(KC_D)); // Toggle Mute
                }
            }
            return false;

        // --- CAM: Tap=Cam (Ctrl+E), Hold=Participants ---
        case CK_CAM:
            if (record->event.pressed) {
                key_timer = timer_read();
            } else {
                if (timer_elapsed(key_timer) < HOLD_TIME) {
                    tap_code16(LCTL(KC_E));
                } else {
                    tap_code16(LCA(KC_P));
                }
            }
            return false;

        // --- HAND: Tap=Hand, Hold=Chat ---
        case CK_HAND:
            if (record->event.pressed) {
                key_timer = timer_read();
            } else {
                if (timer_elapsed(key_timer) < HOLD_TIME) {
                    tap_code16(LCA(KC_H));
                } else {
                    tap_code16(LCA(KC_C));
                }
            }
            return false;

        // --- LEAVE: Tap=Layout, Hold=Close Tab ---
        case CK_LEAVE:
            if (record->event.pressed) {
                key_timer = timer_read();
            } else {
                if (timer_elapsed(key_timer) < HOLD_TIME) {
                    tap_code16(LSFT(KC_SLSH)); // Shortcuts menu
                } else {
                    tap_code16(LCTL(KC_W)); // Close Tab
                }
            }
            return false;
// ==========================================================
        // PROFILE B: WORK MODE LOGIC
        // ==========================================================

        case CK_MEDIA: // Tap: Play/Pause | Hold: Prev Track
            if (record->event.pressed) {
                key_timer = timer_read();
            } else {
                if (timer_elapsed(key_timer) < HOLD_TIME) {
                    tap_code(KC_MPLY); // Play/Pause
                } else {
                    tap_code(KC_MPRV); // Previous Track
                }
            }
            return false;

        case CK_AUDIO: // Tap: Mute Audio | Hold: Next Track
            if (record->event.pressed) {
                key_timer = timer_read();
            } else {
                if (timer_elapsed(key_timer) < HOLD_TIME) {
                    tap_code(KC_MUTE); // System Mute
                } else {
                    tap_code(KC_MNXT); // Next Track
                }
            }
            return false;

        case CK_COPY: // Tap: Copy (Ctrl+C) | Hold: Paste (Ctrl+V)
            if (record->event.pressed) {
                key_timer = timer_read();
            } else {
                if (timer_elapsed(key_timer) < HOLD_TIME) {
                    tap_code16(LCTL(KC_C)); // Copy
                } else {
                    tap_code16(LCTL(KC_V)); // Paste
                }
            }
            return false;

        case CK_SHOT: // Tap: Screenshot | Hold: Alt+Tab
            if (record->event.pressed) {
                key_timer = timer_read();
            } else {
                if (timer_elapsed(key_timer) < HOLD_TIME) {
                    tap_code16(LWIN(LSFT(KC_S))); // Win+Shift+S
                } else {
                    // Quick Alt-Tab (Switch to last window)
                    register_code(KC_LALT);
                    tap_code(KC_TAB);
                    unregister_code(KC_LALT);
                }
            }
            return false;
    }

    return true; // Return true allows standard keys (Layer 1) to work normally
}