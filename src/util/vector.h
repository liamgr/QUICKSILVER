#pragma once

#include <cbor.h>

typedef union {
  struct {
    float roll;
    float pitch;
    float yaw;
  };
  float axis[3];
} vec3_t;

cbor_result_t cbor_encode_vec3_t(cbor_value_t *enc, const vec3_t *vec);
cbor_result_t cbor_decode_vec3_t(cbor_value_t *dec, vec3_t *vec);

float vec3_magnitude(vec3_t *v);

typedef union {
  struct {
    int16_t roll;
    int16_t pitch;
    int16_t yaw;
  };
  int16_t axis[3];
} compact_vec3_t;

cbor_result_t cbor_encode_compact_vec3_t(cbor_value_t *enc, const compact_vec3_t *vec);
cbor_result_t cbor_decode_compact_vec3_t(cbor_value_t *dec, compact_vec3_t *vec);

void vec3_from_array(vec3_t *out, float *in);
void vec3_compress(compact_vec3_t *out, vec3_t *in, float scale);

// Helper functions for vec3_rotate
static inline vec3_t vec3_add(vec3_t a, vec3_t b) {
  return (vec3_t){{a.axis[0] + b.axis[0],
                   a.axis[1] + b.axis[1],
                   a.axis[2] + b.axis[2]}};
}

static inline vec3_t vec3_sub(vec3_t a, vec3_t b) {
  return (vec3_t){{a.axis[0] - b.axis[0],
                   a.axis[1] - b.axis[1],
                   a.axis[2] - b.axis[2]}};
}

static inline vec3_t vec3_mul(vec3_t v, float s) {
  return (vec3_t){{v.axis[0] * s,
                   v.axis[1] * s,
                   v.axis[2] * s}};
}

static inline vec3_t vec3_mul_elem(vec3_t a, vec3_t b) {
  return (vec3_t){{a.axis[0] * b.axis[0],
                   a.axis[1] * b.axis[1],
                   a.axis[2] * b.axis[2]}};
}

static inline float vec3_dot(vec3_t a, vec3_t b) {
  return a.axis[0] * b.axis[0] + a.axis[1] * b.axis[1] + a.axis[2] * b.axis[2];
}

static inline vec3_t vec3_cross(vec3_t a, vec3_t b) {
  return (vec3_t){{a.axis[1] * b.axis[2] - a.axis[2] * b.axis[1],
                   a.axis[2] * b.axis[0] - a.axis[0] * b.axis[2],
                   a.axis[0] * b.axis[1] - a.axis[1] * b.axis[0]}};
}

static inline vec3_t vec3_rotate(const vec3_t vec, const vec3_t rot) {
  return (vec3_t){{
      vec.axis[0] - vec.axis[1] * rot.axis[2] + vec.axis[2] * rot.axis[1],
      vec.axis[0] * rot.axis[2] + vec.axis[1] - vec.axis[2] * rot.axis[0],
      -vec.axis[0] * rot.axis[1] + vec.axis[1] * rot.axis[0] + vec.axis[2],
  }};
}

typedef union {
  struct {
    float roll;
    float pitch;
    float yaw;
    float throttle;
  };
  float axis[4];
} vec4_t;

cbor_result_t cbor_encode_vec4_t(cbor_value_t *enc, const vec4_t *vec);
cbor_result_t cbor_decode_vec4_t(cbor_value_t *dec, vec4_t *vec);

typedef union {
  struct {
    int16_t roll;
    int16_t pitch;
    int16_t yaw;
    int16_t throttle;
  };
  int16_t axis[4];
} compact_vec4_t;

cbor_result_t cbor_encode_compact_vec4_t(cbor_value_t *enc, const compact_vec4_t *vec);
cbor_result_t cbor_decode_compact_vec4_t(cbor_value_t *dec, compact_vec4_t *vec);

void vec4_from_array(vec4_t *out, float *in);
void vec4_compress(compact_vec4_t *out, vec4_t *in, float scale);