// Stub header to satisfy PNGdec's s3_simd_rgb565.S on standard ESP32 (non-S3).
// Defines the ESP32-S3 AES3 SIMD feature flag as disabled so the S3-specific
// assembly block compiles to nothing on plain ESP32.
#pragma once
#define dsps_fft2r_sc16_aes3_enabled 0
