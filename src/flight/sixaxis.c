#include "flight/sixaxis.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "core/debug.h"
#include "core/flash.h"
#include "core/profile.h"
#include "core/project.h"
#include "core/tasks.h"
#include "driver/gyro/gyro.h"
#include "driver/serial.h"
#include "driver/time.h"
#include "flight/control.h"
#include "flight/filter.h"
#include "flight/sdft.h"
#include "flight/sixaxis.h"
#include "io/blackbox.h"
#include "io/led.h"
#include "util/util.h"

#define CAL_INTERVAL 2000           // time between measurements in us
#define CAL_COUNT 500               // count of samples
#define CAL_TRIES (CAL_COUNT * 100) // number of attempts

#define GYRO_BIAS_LIMIT 800
#define ACCEL_BIAS_LIMIT 800

// gyro has +-2000 divided over 16bit.
#define GYRO_RANGE (1.f / (65536.f / 4000.f))
#define ACCEL_RANGE (1.f / 2048.0f)

#ifdef USE_GYRO

static float gyrocal[3];
static float rot_mat[3][3] = {
    {1.0f, 0.0f, 0.0f},
    {0.0f, 1.0f, 0.0f},
    {0.0f, 0.0f, 1.0f},
};

static filter_t filter[FILTER_MAX_SLOTS];
static filter_state_t filter_state[FILTER_MAX_SLOTS][3];

static sdft_t gyro_sdft[SDFT_AXES];
static filter_biquad_notch_t notch_filter[SDFT_AXES][SDFT_PEAKS];
static filter_biquad_state_t notch_filter_state[SDFT_AXES][SDFT_PEAKS];

void sixaxis_init() {
  target_info.gyro_id = gyro_init();
  if (target_info.gyro_id == GYRO_TYPE_INVALID) {
    failloop(FAILLOOP_GYRO);
  }

  for (uint8_t i = 0; i < FILTER_MAX_SLOTS; i++) {
    filter_init(profile.filter.gyro[i].type, &filter[i], filter_state[i], 3, profile.filter.gyro[i].cutoff_freq, task_get_period_us(TASK_GYRO));
  }

  for (uint8_t i = 0; i < SDFT_AXES; i++) {
    sdft_init(&gyro_sdft[i]);
    for (uint8_t j = 0; j < SDFT_PEAKS; j++) {
      filter_biquad_notch_init(&notch_filter[i][j], &notch_filter_state[i][j], 1, 0, task_get_period_us(TASK_GYRO));
    }
  }
}

static void sixaxis_compute_matrix() {
  static uint8_t last_gyro_orientation = GYRO_ROTATE_NONE;
  if (last_gyro_orientation == profile.motor.gyro_orientation) {
    return;
  }

  vec3_t rot = {.roll = 0, .pitch = 0, .yaw = 0};

  if (profile.motor.gyro_orientation & GYRO_ROTATE_90_CW) {
    rot.yaw += 90.0f * DEGTORAD;
  }
  if (profile.motor.gyro_orientation & GYRO_ROTATE_90_CCW) {
    rot.yaw -= 90.0f * DEGTORAD;
  }
  if (profile.motor.gyro_orientation & GYRO_ROTATE_45_CW) {
    rot.yaw += 45.0f * DEGTORAD;
  }
  if (profile.motor.gyro_orientation & GYRO_ROTATE_45_CCW) {
    rot.yaw -= 45.0f * DEGTORAD;
  }
  if (profile.motor.gyro_orientation & GYRO_ROTATE_180) {
    rot.yaw += 180.0f * DEGTORAD;
  }
  if (profile.motor.gyro_orientation & GYRO_FLIP_180) {
    rot.roll += 180.0f * DEGTORAD;
    rot.yaw += 180.0f * DEGTORAD;
  }

  const float cosx = fastcos(rot.roll);
  const float sinx = fastsin(rot.roll);
  const float cosy = fastcos(rot.pitch);
  const float siny = fastsin(rot.pitch);
  const float cosz = fastcos(rot.yaw);
  const float sinz = fastsin(rot.yaw);

  const float coszcosx = cosz * cosx;
  const float sinzcosx = sinz * cosx;
  const float coszsinx = sinx * cosz;
  const float sinzsinx = sinx * sinz;

  rot_mat[0][0] = cosz * cosy;
  rot_mat[0][1] = -cosy * sinz;
  rot_mat[0][2] = siny;
  rot_mat[1][0] = sinzcosx + (coszsinx * siny);
  rot_mat[1][1] = coszcosx - (sinzsinx * siny);
  rot_mat[1][2] = -sinx * cosy;
  rot_mat[2][0] = (sinzsinx) - (coszcosx * siny);
  rot_mat[2][1] = (coszsinx) + (sinzcosx * siny);
  rot_mat[2][2] = cosy * cosx;

  last_gyro_orientation = profile.motor.gyro_orientation;
}

static vec3_t sixaxis_apply_matrix(vec3_t v) {
  return (vec3_t){
      .roll = (rot_mat[0][0] * v.roll + rot_mat[1][0] * v.pitch + rot_mat[2][0] * v.yaw),
      .pitch = (rot_mat[0][1] * v.roll + rot_mat[1][1] * v.pitch + rot_mat[2][1] * v.yaw),
      .yaw = (rot_mat[0][2] * v.roll + rot_mat[1][2] * v.pitch + rot_mat[2][2] * v.yaw),
  };
}

void sixaxis_read() {
  sixaxis_compute_matrix();

  filter_coeff(profile.filter.gyro[0].type, &filter[0], profile.filter.gyro[0].cutoff_freq, task_get_period_us(TASK_GYRO));
  filter_coeff(profile.filter.gyro[1].type, &filter[1], profile.filter.gyro[1].cutoff_freq, task_get_period_us(TASK_GYRO));

  const gyro_data_t data = gyro_read();

  const vec3_t accel = sixaxis_apply_matrix(data.accel);
  // swap pitch and roll to match gyro
  state.accel_raw.roll = (accel.pitch - flash_storage.accelcal[1]) * ACCEL_RANGE;
  state.accel_raw.pitch = (accel.roll - flash_storage.accelcal[0]) * ACCEL_RANGE;
  state.accel_raw.yaw = (accel.yaw - flash_storage.accelcal[2]) * ACCEL_RANGE;

  state.gyro_raw.roll = data.gyro.roll - gyrocal[0];
  state.gyro_raw.pitch = data.gyro.pitch - gyrocal[1];
  state.gyro_raw.yaw = data.gyro.yaw - gyrocal[2];
  state.gyro_raw = sixaxis_apply_matrix(state.gyro_raw);
  state.gyro.roll = state.gyro_raw.roll = state.gyro_raw.roll * GYRO_RANGE * DEGTORAD;
  state.gyro.pitch = state.gyro_raw.pitch = -state.gyro_raw.pitch * GYRO_RANGE * DEGTORAD;
  state.gyro.yaw = state.gyro_raw.yaw = -state.gyro_raw.yaw * GYRO_RANGE * DEGTORAD;

  state.gyro_temp = data.temp;

  if (profile.filter.gyro_dynamic_notch_enable) {
    // we are updating the sdft state per axis per loop
    // eg. axis 0 step 0, axis 0 step 1 ... axis 2, step 3

    // 3 == idle state
    static uint8_t current_axis = 3;

    for (uint32_t i = 0; i < 3; i++) {
      // push new samples every loop
      if (sdft_push(&gyro_sdft[i], state.gyro.axis[i]) && current_axis == 3) {
        // a batch just finished and we are in idle state
        // kick off a new round of axis updates
        current_axis = 0;
      }
    }

    // if we have a batch ready, start walking the sdft steps for a given axis
    if (current_axis < 3 && sdft_update(&gyro_sdft[current_axis])) {
      // once all sdft update steps are done, we update the filters and continue to the next axis
      for (uint32_t p = 0; p < SDFT_PEAKS; p++) {
        filter_biquad_notch_coeff(&notch_filter[current_axis][p], gyro_sdft[current_axis].notch_hz[p], task_get_period_us(TASK_GYRO));
      }
      // on the last axis we increment this to the idle state 3
      current_axis++;
    }
  }

  for (uint32_t i = 0; i < 3; i++) {
    state.gyro.axis[i] = filter_step(profile.filter.gyro[0].type, &filter[0], &filter_state[0][i], state.gyro.axis[i]);
    state.gyro.axis[i] = filter_step(profile.filter.gyro[1].type, &filter[1], &filter_state[1][i], state.gyro.axis[i]);

    if (profile.filter.gyro_dynamic_notch_enable) {
      for (uint32_t p = 0; p < SDFT_PEAKS; p++) {
        blackbox_set_debug(BBOX_DEBUG_DYN_NOTCH, i * SDFT_PEAKS + p, gyro_sdft[i].notch_hz[p]);
        state.gyro.axis[i] = filter_biquad_notch_step(&notch_filter[i][p], &notch_filter_state[i][p], state.gyro.axis[i]);
      }
    }
  }

  state.gyro_delta_angle.roll = state.gyro.roll * state.looptime;
  state.gyro_delta_angle.pitch = state.gyro.pitch * state.looptime;
  state.gyro_delta_angle.yaw = state.gyro.yaw * state.looptime;
}

static bool test_gyro_move(const gyro_data_t *last_data, const gyro_data_t *data) {
  bool did_move = false;
  for (uint8_t i = 0; i < 3; i++) {
    const float delta = fabsf(fabsf(last_data->gyro.axis[i] * GYRO_RANGE) - fabsf(data->gyro.axis[i] * GYRO_RANGE));
    if (delta > 0.5f) {
      did_move = true;
      break;
    }
  }
  return did_move;
}

// returns true if it's already still, i.e. no move since the first loops
static bool sixaxis_wait_for_still(uint32_t timeout) {
  uint8_t move_counter = 15;
  uint32_t loop_counter = 0;

  gyro_data_t last_data;
  memset(&last_data, 0, sizeof(gyro_data_t));

  const uint32_t start = time_micros();
  uint32_t now = start;
  while (now - start < timeout && move_counter > 0) {
    const gyro_data_t data = gyro_read();

    const bool did_move = test_gyro_move(&last_data, &data);
    if (did_move) {
      move_counter = 15;
    } else {
      move_counter--;
    }

    led_pwm(move_counter / 15.f, 1000);

    while ((time_micros() - now) < 1000)
      ;

    now = time_micros();
    last_data = data;
    ++loop_counter;
  }
  return loop_counter < 20;
}

void sixaxis_gyro_cal() {
  for (uint8_t retry = 0; retry < 15; ++retry) {
    if (sixaxis_wait_for_still(CAL_INTERVAL)) {
      // break only if it's already still, otherwise, wait and try again
      break;
    }
    time_delay_ms(100);
  }
  gyro_calibrate();

  gyro_data_t last_data = gyro_read();
  for (uint32_t tries = 0, cal_counter = 0; tries < CAL_TRIES; tries++) {
    const gyro_data_t data = gyro_read();

    led_pwm(((float)(cal_counter) / (float)(CAL_COUNT)), CAL_INTERVAL);

    bool did_move = test_gyro_move(&last_data, &data);
    if (!did_move) { // only cali gyro when it's still
      for (uint8_t i = 0; i < 3; i++) {
        lpf(&gyrocal[i], data.gyro.axis[i], lpfcalc(CAL_INTERVAL, 0.5 * 1e6));
      }
      if (cal_counter++ == CAL_COUNT) {
        break;
      }
    }

    time_delay_us(CAL_INTERVAL);
    last_data = data;
  }
}

void sixaxis_acc_cal() {
  for (uint8_t retry = 0; retry < 15; ++retry) {
    if (sixaxis_wait_for_still(CAL_INTERVAL)) {
      // break only if it's already still, otherwise, wait and try again
      break;
    }
    time_delay_ms(100);
  }

  sixaxis_compute_matrix();

  flash_storage.accelcal[0] = 0;
  flash_storage.accelcal[1] = 0;
  flash_storage.accelcal[2] = 2048;

  gyro_data_t last_data = gyro_read();
  for (uint32_t tries = 0, cal_counter = 0; tries < CAL_TRIES; tries++) {
    const gyro_data_t data = gyro_read();

    led_pwm(((float)(cal_counter) / (float)(CAL_COUNT)), CAL_INTERVAL);

    // Skip samples if gyro shows movement
    if (!test_gyro_move(&last_data, &data)) {
      const vec3_t accel = sixaxis_apply_matrix(data.accel);
      for (uint8_t i = 0; i < 3; i++) {
        lpf(&flash_storage.accelcal[i], accel.axis[i], lpfcalc(CAL_INTERVAL, 0.5 * 1e6));
      }

      if (cal_counter++ == CAL_COUNT) {
        break;
      }
    }

    time_delay_us(CAL_INTERVAL);
    last_data = data;
  }

  for (uint8_t i = 0; i < 3; i++) {
    flash_storage.accelcal[i] = constrain(flash_storage.accelcal[i], -ACCEL_BIAS_LIMIT, ACCEL_BIAS_LIMIT);
  }
}

#else

void sixaxis_init() {}
void sixaxis_read() {}

void sixaxis_gyro_cal() {
  time_delay_ms(1500);
}
void sixaxis_acc_cal() {
  time_delay_ms(1500);
}

#endif