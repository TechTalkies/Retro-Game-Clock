/*
 * SmallOLED-PCMonitor - Dino Runner Clock (clockStyle 11)
 *
 * Chrome T-Rex homage. The dino runs in place near the bottom of the screen
 * while the world scrolls past: dashed ground, drifting parallax clouds and
 * the occasional cactus, which the dino hops over with a proper gravity arc.
 * Every so often a pterodactyl flaps across the midfield for flavour.
 *
 * At the top of each minute the pterodactyl turns courier: it swoops in from
 * the right at digit height, snatches the old digit (which hangs from its
 * claws as it flies off-screen left), and the new digit then drops in from
 * above, landing with a little dust puff. One changed digit at a time.
 *
 * The whole scene is 1-bit native - everything is procedural rects, lines
 * and triangles, no bitmaps. All state is file-local. resetDinoAnimation()
 * (called from resetClockAnimationState) returns everything to baseline.
 */

#include "../config/config.h"
#include "../display/display.h"
#include "clocks.h"
#include "clock_globals.h"

// ========== Monochrome "Classic Dino" Color ==========
// ========== Custom Dino Colors ==========
#define DINO_GREY ((0x0F << 11) | (0x1F << 5) | (0x0F << 0)) // Light Grey / Silver for the world
#define DINO_TIME 0xFD20                                     // For the clock/text
#define DINO_GREEN 0x07E0

// ========== Scaled Layout / tuning ==========
#define DINO_SCALE 2 // 2x Graphics Scale
#define DINO_TIME_Y_TOP 10
#define DINO_TIME_Y_CENTER 28
#define DINO_TRIGGER_SECOND 56
#define DINO_DIGIT_W 28
#define DINO_GROUND_Y 110
#define DINO_X 24
#define DINO_MAX_CACTI 3
#define DINO_MAX_CLOUDS 2
#define DINO_MAX_DUST 6
#define DINO_PTERO_SPEED 120.0f
#define DINO_DROP_START -60.0f
#define DINO_PHASE_TIMEOUT 6.0f

// Centered X positions for 5 digits
static const int DINO_DIGIT_X[5] = {11, 41, 69, 91, 121};

enum DinoPhase
{
  DINO_IDLE,
  DINO_PTERO_ENTER,
  DINO_PTERO_CARRY,
  DINO_DIGIT_DROP
};

struct DinoCactus
{
  bool active;
  float x;
  bool tall; // small (4x8) or tall (5x12) variant
};

struct DinoCloud
{
  float x;
  int y;
};

struct DinoDust
{
  bool active;
  float x, y, vx, vy;
  float life;
};

static DinoPhase dino_phase = DINO_IDLE;

// Dino body
static float dino_jump_y = 0.0f; // 0 = on ground, negative = airborne
static float dino_jump_vy = 0.0f;
static bool dino_airborne = false;
static int dino_leg_frame = 0;
static unsigned long last_leg_toggle = 0;

// World
static DinoCactus dino_cacti[DINO_MAX_CACTI];
static DinoCloud dino_clouds[DINO_MAX_CLOUDS];
static DinoDust dino_dust[DINO_MAX_DUST];
static float dino_cactus_timer = 3.0f; // until the next cactus spawns
static float dino_ground_phase = 0.0f; // scroll offset for ground dashes

// Pterodactyl (idle flybys + the minute-change courier)
static bool ptero_active = false;
static float ptero_x = 0, ptero_y = 0;
static int ptero_wing_frame = 0;
static unsigned long last_wing_toggle = 0;
static float ptero_idle_timer = 12.0f; // until the next idle flyby

// Minute-change bookkeeping
static int dino_change_idx[4];
static uint8_t dino_change_val[4];
static int dino_num_changes = 0;
static int dino_cur_change = 0;
static float dino_phase_timer = 0.0f;
static int last_minute_dino = -1;
static bool dino_triggered = false;

static unsigned long last_dino_update = 0;
static bool dino_init_done = false;

// ========== Helpers ==========
static int dinoTimeY()
{
  // return settings.dinoShowDate ? DINO_TIME_Y_TOP : DINO_TIME_Y_CENTER;
  return DINO_TIME_Y_CENTER;
}

static float dinoRandf(float lo, float hi)
{
  return lo + (hi - lo) * (random(0, 1001) / 1000.0f);
}

static float dinoScrollSpeed()
{
  // Doubled from 30.0f to match the 2x graphical scale
  return 60.0f * (settings.dinoSpeed / 10.0f);
}

static void dinoCactusGap(float &lo, float &hi)
{
  switch (settings.dinoCactusFreq)
  {
  case 0:
    lo = 8.0f;
    hi = 16.0f;
    break; // rare
  case 2:
    lo = 3.0f;
    hi = 6.0f;
    break; // frequent
  default:
    lo = 5.0f;
    hi = 10.0f;
    break; // normal
  }
}

static void dinoSpawnDust(float x, float y)
{
  int spawned = 0;
  for (int i = 0; i < DINO_MAX_DUST && spawned < 4; i++)
  {
    if (dino_dust[i].active)
      continue;
    dino_dust[i].active = true;
    dino_dust[i].x = x + dinoRandf(-4, 4);
    dino_dust[i].y = y;
    dino_dust[i].vx = dinoRandf(-36, 36);  // Scaled velocity
    dino_dust[i].vy = dinoRandf(-44, -16); // Scaled velocity
    dino_dust[i].life = dinoRandf(0.25f, 0.45f);
    spawned++;
  }
}

// ========== Reset ==========
void resetDinoAnimation()
{
  dino_phase = DINO_IDLE;
  dino_jump_y = 0.0f;
  dino_jump_vy = 0.0f;
  dino_airborne = false;
  dino_leg_frame = 0;
  for (int i = 0; i < DINO_MAX_CACTI; i++)
    dino_cacti[i].active = false;
  for (int i = 0; i < DINO_MAX_DUST; i++)
    dino_dust[i].active = false;
  dino_clouds[0].x = 30;
  dino_clouds[0].y = 6;
  dino_clouds[1].x = 95;
  dino_clouds[1].y = 11;
  dino_cactus_timer = dinoRandf(2.0f, 5.0f);
  dino_ground_phase = 0.0f;
  ptero_active = false;
  ptero_idle_timer = dinoRandf(8.0f, 18.0f);
  dino_num_changes = 0;
  dino_cur_change = 0;
  dino_triggered = false;
  last_minute_dino = -1;
  last_dino_update = 0;
  dino_init_done = true;
}

// ========== Update ==========
static void dinoRevealAndAdvance()
{
  // The drop already wrote the new value; move on to the next change.
  dino_cur_change++;
  if (dino_cur_change < dino_num_changes)
  {
    dino_phase = DINO_PTERO_ENTER;
    dino_phase_timer = 0.0f;
    ptero_active = true;
    ptero_x = SCREEN_WIDTH + 14;
    ptero_y = dinoTimeY() + 4;
  }
  else
  {
    dino_phase = DINO_IDLE;
    ptero_idle_timer = dinoRandf(8.0f, 18.0f);
  }
}

static void updateDinoAnimation(struct tm *timeinfo)
{
  unsigned long now = millis();
  updateDigitBounce();

  float dt = (now - last_dino_update) / 1000.0f;
  if (dt > 0.1f || last_dino_update == 0)
    dt = 0.025f;
  last_dino_update = now;

  float scroll = dinoScrollSpeed();

  // ----- Minute-change trigger (same scheme as Snake/Asteroids) -----
  int seconds = timeinfo->tm_sec;
  int minute = timeinfo->tm_min;
  if (minute != last_minute_dino)
  {
    last_minute_dino = minute;
    dino_triggered = false;
  }
  if (seconds >= DINO_TRIGGER_SECOND && !dino_triggered &&
      dino_phase == DINO_IDLE)
  {
    dino_triggered = true;
    time_overridden = true;
    time_override_start = millis();
    calculateTargetDigits(displayed_hour, displayed_min, displayed_is_pm);

    dino_num_changes = 0;
    for (int i = 0; i < num_targets; i++)
    {
      if (target_digit_index[i] != 2)
      { // skip the colon
        dino_change_idx[dino_num_changes] = target_digit_index[i];
        dino_change_val[dino_num_changes] = target_digit_values[i];
        dino_num_changes++;
      }
    }
    dino_cur_change = 0;
    if (dino_num_changes > 0)
    {
      dino_phase = DINO_PTERO_ENTER;
      dino_phase_timer = 0.0f;
      ptero_active = true; // repurpose any idle flyby as courier
      ptero_x = SCREEN_WIDTH + 14;
      ptero_y = dinoTimeY() + 4;
    }
    else
    {
      time_overridden = false; // only the colon changed
    }
  }

  // ----- Courier phases -----
  if (dino_phase != DINO_IDLE)
  {
    dino_phase_timer += dt;
    int didx = dino_change_idx[dino_cur_change];

    if (dino_phase == DINO_PTERO_ENTER)
    {
      ptero_x -= DINO_PTERO_SPEED * dt;
      ptero_y = dinoTimeY() + 8;                              // Offset slightly for 2x scale
      if (ptero_x <= DINO_DIGIT_X[didx] + DINO_DIGIT_W / 2 || // Changed DIGIT_X to DINO_DIGIT_X
          dino_phase_timer > DINO_PHASE_TIMEOUT)
      {
        dino_phase = DINO_PTERO_CARRY; // claws close on the old digit
        dino_phase_timer = 0.0f;
      }
    }
    else if (dino_phase == DINO_PTERO_CARRY)
    {
      ptero_x -= DINO_PTERO_SPEED * dt;
      if (ptero_x < -24 || dino_phase_timer > DINO_PHASE_TIMEOUT)
      {
        ptero_active = false;
        // Swap in the new value and let it fall from above the screen.
        updateDisplayedTimeDigit(didx, dino_change_val[dino_cur_change]);
        digit_offset_y[didx] = DINO_DROP_START;
        digit_velocity[didx] = 0.0f;
        dino_phase = DINO_DIGIT_DROP;
        dino_phase_timer = 0.0f;
      }
    }
    else if (dino_phase == DINO_DIGIT_DROP)
    {
      // updateDigitBounce() pulls the offset down to 0 with gravity.
      if (digit_offset_y[didx] >= 0.0f ||
          dino_phase_timer > DINO_PHASE_TIMEOUT)
      {
        digit_offset_y[didx] = 0.0f;
        digit_velocity[didx] = 0.0f;
        dinoSpawnDust(DIGIT_X[didx] + DINO_DIGIT_W / 2,
                      dinoTimeY() + 21);
        dinoRevealAndAdvance();
      }
    }
  }
  else
  {
    // Idle pterodactyl flyby across the midfield (right to left)
    if (ptero_active)
    {
      ptero_x -= 35.0f * dt;
      if (ptero_x < -16)
        ptero_active = false;
    }
    else
    {
      ptero_idle_timer -= dt;
      if (ptero_idle_timer <= 0)
      {
        ptero_idle_timer = dinoRandf(12.0f, 28.0f);
        ptero_active = true;
        ptero_x = SCREEN_WIDTH + 14;
        ptero_y = dinoRandf(42, 48);
      }
    }
  }

  // ----- World scroll: ground, cacti, clouds -----
  dino_ground_phase += scroll * dt;
  while (dino_ground_phase >= 16.0f)
    dino_ground_phase -= 16.0f;

  for (int i = 0; i < DINO_MAX_CACTI; i++)
  {
    if (!dino_cacti[i].active)
      continue;
    dino_cacti[i].x -= scroll * dt;
    if (dino_cacti[i].x < -8)
      dino_cacti[i].active = false;
  }
  dino_cactus_timer -= dt;
  if (dino_cactus_timer <= 0)
  {
    float lo, hi;
    dinoCactusGap(lo, hi);
    dino_cactus_timer = dinoRandf(lo, hi);
    for (int i = 0; i < DINO_MAX_CACTI; i++)
    {
      if (dino_cacti[i].active)
        continue;
      dino_cacti[i].active = true;
      dino_cacti[i].x = SCREEN_WIDTH + 6;
      dino_cacti[i].tall = random(0, 3) == 0; // 1 in 3 tall
      break;
    }
  }

  for (int i = 0; i < DINO_MAX_CLOUDS; i++)
  {
    dino_clouds[i].x -= (scroll * 0.18f) * dt; // parallax: clouds far away
    if (dino_clouds[i].x < -16)
    {
      dino_clouds[i].x = SCREEN_WIDTH + dinoRandf(4, 30);
      dino_clouds[i].y = (int)dinoRandf(3, 13);
    }
  }

  // ----- Dino: auto-jump approaching cacti -----
  if (!dino_airborne)
  {
    for (int i = 0; i < DINO_MAX_CACTI; i++)
    {
      if (!dino_cacti[i].active)
        continue;
      float dist = dino_cacti[i].x - DINO_X;
      if (dist > 0 && dist < scroll * 0.45f)
      {
        dino_airborne = true;
        dino_jump_vy = -110.0f; // Scaled jump power px/s up
        dinoSpawnDust(DINO_X + 4, DINO_GROUND_Y);
        break;
      }
    }
  }
  else
  {
    dino_jump_vy += 340.0f * dt; // Scaled gravity px/s^2
    dino_jump_y += dino_jump_vy * dt;
    if (dino_jump_y >= 0.0f)
    {
      dino_jump_y = 0.0f;
      dino_jump_vy = 0.0f;
      dino_airborne = false;
      dinoSpawnDust(DINO_X + 2, DINO_GROUND_Y);
    }
  }

  // Leg + wing animation clocks (run off wall time, scale with speed)
  unsigned long legMs = (unsigned long)(140.0f / (settings.dinoSpeed / 10.0f));
  if (now - last_leg_toggle > legMs)
  {
    last_leg_toggle = now;
    dino_leg_frame ^= 1;
  }
  if (now - last_wing_toggle > 160)
  {
    last_wing_toggle = now;
    ptero_wing_frame ^= 1;
  }

  // ----- Dust -----
  for (int i = 0; i < DINO_MAX_DUST; i++)
  {
    if (!dino_dust[i].active)
      continue;
    dino_dust[i].x += dino_dust[i].vx * dt;
    dino_dust[i].y += dino_dust[i].vy * dt;
    dino_dust[i].vy += 90.0f * dt;
    dino_dust[i].life -= dt;
    if (dino_dust[i].life <= 0)
      dino_dust[i].active = false;
  }
}

// ========== Drawing ==========
// ~12x12 procedural T-Rex, feet at (x, groundY), facing right.
// ========== Drawing ==========
static void drawDino(int x, int groundY)
{
  int s = DINO_SCALE;
  int top = groundY - (12 * s) + (int)dino_jump_y;

  // Head and eye
  display.fillRect(x + 6 * s, top, 6 * s, 4 * s, ST77XX_WHITE);
  display.fillRect(x + 8 * s, top + 1 * s, s, s, ST77XX_BLACK); // eye
  display.fillRect(x + 6 * s, top + 3 * s, 4 * s, s, ST77XX_WHITE);

  // Neck + body
  display.fillRect(x + 4 * s, top + 2 * s, 4 * s, 6 * s, ST77XX_WHITE);
  display.fillRect(x + 1 * s, top + 4 * s, 6 * s, 5 * s, ST77XX_WHITE);

  // Tail
  display.fillRect(x - 1 * s, top + 3 * s, 2 * s, 3 * s, ST77XX_WHITE);

  // Forearm
  display.fillRect(x + 7 * s, top + 5 * s, s, s, ST77XX_WHITE);

  // Legs
  if (dino_airborne)
  {
    display.fillRect(x + 2 * s, top + 9 * s, 2 * s, 2 * s, ST77XX_WHITE);
    display.fillRect(x + 5 * s, top + 9 * s, 2 * s, 2 * s, ST77XX_WHITE);
  }
  else if (dino_leg_frame == 0)
  {
    display.fillRect(x + 2 * s, top + 9 * s, 2 * s, 3 * s, ST77XX_WHITE);
    display.fillRect(x + 5 * s, top + 9 * s, 2 * s, 2 * s, ST77XX_WHITE);
  }
  else
  {
    display.fillRect(x + 2 * s, top + 9 * s, 2 * s, 2 * s, ST77XX_WHITE);
    display.fillRect(x + 5 * s, top + 9 * s, 2 * s, 3 * s, ST77XX_WHITE);
  }
}

static void drawCactus(const DinoCactus &c)
{
  int s = DINO_SCALE;
  int x = (int)c.x;
  if (c.tall)
  {
    display.fillRect(x + 2 * s, DINO_GROUND_Y - 12 * s, 2 * s, 12 * s, DINO_GREEN);
    display.fillRect(x, DINO_GROUND_Y - 9 * s, 2 * s, 4 * s, DINO_GREEN);
    display.fillRect(x + 1 * s, DINO_GROUND_Y - 10 * s, s, s, DINO_GREEN);
    display.fillRect(x + 4 * s, DINO_GROUND_Y - 7 * s, 2 * s, 3 * s, DINO_GREEN);
  }
  else
  {
    display.fillRect(x + 1 * s, DINO_GROUND_Y - 8 * s, 2 * s, 8 * s, DINO_GREEN);
    display.fillRect(x, DINO_GROUND_Y - 6 * s, s, s, DINO_GREEN);
    display.fillRect(x + 3 * s, DINO_GROUND_Y - 5 * s, s, s, DINO_GREEN);
  }
}

static void drawPtero(int x, int y)
{
  int s = DINO_SCALE;

  // Head, Beak, and Crest
  display.fillRect(x - 7 * s, y, 5 * s, 2 * s, DINO_GREY);         // Beak
  display.fillRect(x - 2 * s, y - 2 * s, 4 * s, 4 * s, DINO_GREY); // Head
  display.fillRect(x - 1 * s, y - 1 * s, s, s, DISPLAY_BLACK);     // Eye
  display.fillRect(x + 2 * s, y - 3 * s, 2 * s, 3 * s, DINO_GREY); // Crest

  // Body and Tail
  display.fillRect(x - 2 * s, y + 2 * s, 8 * s, 2 * s, DINO_GREY); // Body
  display.fillRect(x + 6 * s, y + 1 * s, 3 * s, 2 * s, DINO_GREY); // Tail

  // Flapping Wings
  if (ptero_wing_frame == 0)
  {
    // Wings UP
    display.fillRect(x + 1 * s, y - 4 * s, 3 * s, 2 * s, DINO_GREY);
    display.fillRect(x + 4 * s, y - 7 * s, 4 * s, 4 * s, DINO_GREY);
  }
  else
  {
    // Wings DOWN
    display.fillRect(x + 1 * s, y + 4 * s, 3 * s, 2 * s, DINO_GREY);
    display.fillRect(x + 4 * s, y + 6 * s, 4 * s, 4 * s, DINO_GREY);
  }
}

static void drawCloud(const DinoCloud &c)
{
  int s = DINO_SCALE;
  int x = (int)c.x;
  display.fillRect(x + 3 * s, c.y, 7 * s, s, ST77XX_WHITE);
  display.fillRect(x + 1 * s, c.y + 1 * s, 12 * s, s, ST77XX_WHITE);
  display.fillRect(x + 4 * s, c.y + 2 * s, 6 * s, s, ST77XX_WHITE);
}

void displayClockWithDino()
{
  if (!dino_init_done)
    resetDinoAnimation();

  struct tm timeinfo;
  if (!getTimeWithTimeout(&timeinfo))
  {
    display.setTextSize(1);
    display.setTextColor(DINO_GREY);
    display.setCursor(20, 28);
    display.print(ntpSynced ? "Time Error" : "Syncing time...");
    return;
  }

  updateDinoAnimation(&timeinfo);

  if (!time_overridden)
    syncDisplayedTime(&timeinfo);
  maintainTimeOverride(&timeinfo, dino_phase == DINO_IDLE);

  int gy = dinoTimeY();
  int s = DINO_SCALE;

  // Clouds
  if (settings.dinoShowClouds)
  {
    for (int i = 0; i < DINO_MAX_CLOUDS; i++)
      drawCloud(dino_clouds[i]);
  }

  // Scaled Ground
  display.fillRect(0, DINO_GROUND_Y, SCREEN_WIDTH, 2, DINO_GREY);
  for (int gx = -(int)dino_ground_phase; gx < SCREEN_WIDTH; gx += 16 * s)
  {
    display.fillRect(gx + 4 * s, DINO_GROUND_Y + 3 * s, 4 * s, 2, DINO_GREY);
    display.fillRect(gx + 11 * s, DINO_GROUND_Y + 2 * s, 2 * s, 2, DINO_GREY);
  }

  for (int i = 0; i < DINO_MAX_CACTI; i++)
  {
    if (dino_cacti[i].active)
      drawCactus(dino_cacti[i]);
  }

  drawDino(DINO_X, DINO_GROUND_Y);

  for (int i = 0; i < DINO_MAX_DUST; i++)
  {
    if (dino_dust[i].active)
    {
      display.fillRect((int)dino_dust[i].x, (int)dino_dust[i].y, 2, 2, DINO_GREY);
    }
  }

  display.setTextSize(5);
  display.setTextColor(DINO_TIME);
  char dch[5];
  dch[0] = '0' + displayed_hour / 10;
  dch[1] = '0' + displayed_hour % 10;
  dch[2] = shouldShowColon() ? ':' : ' ';
  dch[3] = '0' + displayed_min / 10;
  dch[4] = '0' + displayed_min % 10;

  int carryIdx = -1;
  if (dino_phase == DINO_PTERO_CARRY)
    carryIdx = dino_change_idx[dino_cur_change];

  for (int i = 0; i < 5; i++)
  {
    if (i == carryIdx)
      continue;
    int dy = gy + ((i == 2) ? 0 : (int)digit_offset_y[i]);
    display.setCursor(DINO_DIGIT_X[i], dy);
    display.print(dch[i]);
  }

  if (ptero_active)
  {
    drawPtero((int)ptero_x, (int)ptero_y);
    if (dino_phase == DINO_PTERO_CARRY && carryIdx >= 0)
    {
      display.setTextSize(5);
      display.setTextColor(DINO_TIME);
      display.setCursor((int)ptero_x - 12, (int)ptero_y + 8);
      display.print(dch[carryIdx]);
    }
  }

  if (settings.dinoShowDate)
  {
    display.setTextSize(1);
    display.setTextColor(ST77XX_WHITE);
    char dateStr[12];
    switch (settings.dateFormat)
    {
    case 0:
      sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      break;
    case 1:
      sprintf(dateStr, "%02d/%02d/%04d", timeinfo.tm_mon + 1, timeinfo.tm_mday, timeinfo.tm_year + 1900);
      break;
    case 2:
      sprintf(dateStr, "%04d-%02d-%02d", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday);
      break;
    case 3:
      sprintf(dateStr, "%02d.%02d.%04d", timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_year + 1900);
      break;
    }
    display.setCursor((SCREEN_WIDTH - 60) / 2, 4);
    display.print(dateStr);
  }

  drawMeridiemIndicator(140, 4, displayed_is_pm);

  if (!wifiConnected)
    drawNoWiFiIcon(0, 0);
}