/*
 * SmallOLED-PCMonitor - User Configuration
 *
 * ============================================
 *   THIS IS THE ONLY FILE YOU NEED TO EDIT
 *   TO CONFIGURE YOUR HARDWARE!
 * ============================================
 *
 * Modify these values to match your hardware setup.
 */

#ifndef USER_CONFIG_H
#define USER_CONFIG_H

// ========== Display Configuration ==========
// Display type:
//   3 = ST7735 160x128 colour TFT (SPI, landscape)
//
// This project is configured for an ST7735 TFT.  The display type is kept as
// a build-time setting so custom PlatformIO environments can override it.
#define DEFAULT_DISPLAY_TYPE 3

// Legacy I2C pins (unused by the ST7735 SPI configuration)
#define I2C_SDA_PIN 8
#define I2C_SCL_PIN 9

// Native ST7735 landscape resolution.  Rendering uses these dimensions
// directly; no scaling is performed.
#define SCREEN_WIDTH 160
#define SCREEN_HEIGHT 128

// ========== Display Interface ==========
// Interface type:
//   1 = SPI (required by ST7735; uses SPI pins below)
//
// ST7735 has no I2C interface.
#define DISPLAY_INTERFACE 1

// SPI pins for ESP32-S3 Super Mini (used by the ST7735)
#define SPI_MOSI_PIN 3 // TFT SDA / MOSI
#define SPI_SCK_PIN 2  // TFT SCK
#define SPI_CS_PIN 6   // TFT CS
#define SPI_DC_PIN 4   // TFT A0 / DC
#define SPI_RST_PIN 5  // TFT RES; set to -1 when connected to ESP reset

// ========== WiFi Configuration ==========
// Access Point name for initial setup.
// AP_PASSWORD: leave as "" for an open (passwordless) AP — easiest for users.
//              Set a password string to require one (e.g. "monitor123").
#define AP_NAME "PCMonitor-Setup"
#define AP_PASSWORD ""

// ========== Optional Hardcoded WiFi Credentials ==========
// Use this if your ESP32 module has a faulty WiFi AP mode
// Set SSID and password to your home network, then upload
// Leave as empty strings "" to use normal WiFiManager portal
#define HARDCODED_WIFI_SSID ""
#define HARDCODED_WIFI_PASSWORD ""

// WiFi reconnection timeout (ms) - restart if WiFi lost for this long
#define WIFI_RECONNECT_TIMEOUT 60000

// ========== Network Configuration ==========
// UDP port for receiving PC stats
#define UDP_PORT 4210

// NTP servers
#define NTP_SERVER_PRIMARY "pool.ntp.org"
#define NTP_SERVER_SECONDARY "time.nist.gov"

// NTP resync interval (1 hour in ms)
#define NTP_RESYNC_INTERVAL 3600000

// ========== Timing Configuration ==========
// Timeout for PC stats (ms) - show clock if no data received
#define STATS_TIMEOUT 10000

// Maximum time (ms) that animated clocks can override NTP time
// After this, force resync even if animation is still running
// Prevents clock drift when packets are dropped during animations
#define TIME_OVERRIDE_MAX_MS 60000

// ========== Watchdog Configuration ==========
// Watchdog timeout in seconds
#define WATCHDOG_TIMEOUT_SECONDS 30

// ========== Touch Button Configuration ==========
// TTP223 capacitive touch sensor support
// - Quick tap (< 500ms): Toggle metrics/clock mode or cycle clock styles
// - Medium press (500ms-1s, release): Toggle LED night light on/off
// - Long hold (> 1s): Ramp LED brightness up/down (gamma-corrected)
// Note: If TTP223 is not connected, GPIO 7 just floats harmlessly
#define TOUCH_BUTTON_ENABLED 1  // 1 = enabled, 0 = disabled (always enabled now)
#define TOUCH_BUTTON_PIN 7      // GPIO pin for TTP223 signal (default: GPIO 7)
#define TOUCH_DEBOUNCE_MS 50    // Debounce delay in milliseconds (default: 100ms)
#define TOUCH_ACTIVE_LEVEL HIGH // HIGH = active HIGH, LOW = active LOW (TTP223 default: HIGH)

// ========== LED PWM Night Light Configuration ==========
// Filament LED night light control via GPIO 1 and 2N2222 transistor
// Gesture-based control using TTP223 touch button
#define LED_PWM_ENABLED 0    // 1 = enabled, 0 = disabled (default: 0)
#define LED_PWM_PIN 1        // GPIO pin for PWM LED control (GPIO 1)
#define LED_PWM_CHANNEL 0    // PWM channel (0-15)
#define LED_PWM_FREQ 5000    // PWM frequency in Hz
#define LED_PWM_RESOLUTION 8 // 8-bit resolution (0-255 brightness levels)

// ========== QR Code Setup Configuration ==========
// Display QR code during WiFi AP setup for easy mobile connection
// When enabled: OLED shows scannable QR code instead of text instructions
// When disabled: Traditional text instructions (original behavior)
#define QR_SETUP_ENABLED 0 // 1 = QR code, 0 = text instructions

// ========== BLE WiFi Setup Configuration ==========
// Bluetooth Low Energy provisioning for the SmallOLED Android app.
// When enabled: on first boot (no saved WiFi), the device advertises as a BLE
//   GATT server. The Android app connects, sends home WiFi SSID + password,
//   device connects and saves credentials. Subsequent boots connect silently.
//   If BLE times out (2 min) or fails, falls back to AP mode automatically.
// When disabled: original WiFiManager AP portal (PCMonitor-Setup) is used.
//
// IMPORTANT: Requires min_spiffs.csv partition table (set in platformio.ini).
#define BLE_SETUP_ENABLED 0         // 1 = BLE provisioning, 0 = AP mode (default)
#define BLE_DEVICE_NAME "SmallOLED" // BLE advertised name (shown in Android app scan)

// ========== Improv-Serial WiFi Setup (Web Flasher) ==========
// In-browser WiFi provisioning over USB serial, used by the web flasher at
// docs/. After flashing, ESP Web Tools probes the device for
// Improv-Serial and shows a "Configure WiFi" dialog right in the browser tab:
// the user picks their home network and the credentials are pushed over USB.
// Active only on first boot (no saved WiFi); the WiFiManager AP portal
// (PCMonitor-Setup) keeps running in parallel as a fallback. Once WiFi is
// saved, subsequent boots skip Improv entirely.
//
// Keep this ENABLED for the released web-flasher binaries so they are
// WiFi-push capable. Costs nothing if no browser is listening.
#define IMPROV_SETUP_ENABLED 1        // 1 = Improv-Serial WiFi push, 0 = AP portal only
#define IMPROV_SETUP_WINDOW_MS 180000 // 3-min Improv listen window on first boot

#endif // USER_CONFIG_H
