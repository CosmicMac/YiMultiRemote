#ifndef YMR_CONFIG_H
#define YMR_CONFIG_H

/**
 * AP
 */
#define AP_IP               IPAddress(192, 168, 0, 1)
#define AP_SSID             "YiMultiRemote"
#define AP_PWD              "YMR12345"

/**
 * CLIENTS
 */
#define CLIENT1_IP          IPAddress(192, 168, 0, 10)
#define CLIENT2_IP          IPAddress(192, 168, 0, 11)
#define CLIENT_PORT         7878

/**
 * ONEBUTTON
 */
#define BTN_PIN             D7    // GPIO13
#define BTN_CLICK_DELAY_MS  500   // Number of millisec before a click is detected (triggers action)
#define BTN_LPRESS_DELAY_MS 1000  // Number of millisec before a long press is detected (triggers restart)

/**
 * LED
 */
#define LED_PIN             D6    // GPIO12

/**
 * BUZZER
 */
#define BUZZER_PIN          D5    // GPIO14
#define BUZZER_FREQUENCY    3000
#define BUZZER_DURATION     100

/**
 * CAMERA MODES
 */
#define DEFAULT_VIDEO_MODE  "record"
#define DEFAULT_PHOTO_MODE  "precise quality"

/**
 * DNS SERVER
 */
#define DNS_PORT            53

/**
 * BEEP ON VIDEO
 * Beep a few seconds after video start, and a few secondes before video stop
 * May be helpful to control and adjust sync between left and right camera
 */
#define BEEP_ON_VIDEO       true

/**
 * YI MESSAGES IDS
 */
#define YI_MSG_CAPTURE_PHOTO   769
#define YI_MSG_START_VIDEO_REC 513
#define YI_MSG_STOP_VIDEO_REC  514

#endif //YMR_CONFIG_H
