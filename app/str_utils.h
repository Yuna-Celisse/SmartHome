/******************************************************************************
 * @file str_utils.h
 *
 * @par dependencies
 *      - stdint.h
 *      - stdbool.h
 *
 * @author Yuna-Celisse
 *
 * @brief  Minimal string utilities for embedded use — no stdio dependency.
 *
 * Provides manual implementations of common string operations (length,
 * substring search, number parsing, float-to-string) suitable for a
 * Cortex-M0+ bare-metal environment where linking printf/scanf would
 * pull in ~5-10 KB of library code.
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#ifndef STR_UTILS_H
#define STR_UTILS_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief  Convert a float to a fixed-point decimal string.
 *
 * Writes up to 11 characters + null into buf. Handles negative
 * values and rounding overflow (e.g. 0.999 → 1.000). Assumes
 * |value| < 10000 and decimals ≤ 4.
 *
 * @param[out] buf       Output buffer (at least 12 bytes recommended).
 * @param[in]  value     Floating-point value to convert.
 * @param[in]  decimals  Number of decimal places (1 .. 4).
 * @return Pointer to buf (null-terminated).
 */
char *ftoa_fixed(char *buf, float value, uint8_t decimals);

/**
 * @brief  Compute the length of a null-terminated string.
 *
 * @param[in] s  Null-terminated string (must not be NULL).
 * @return Number of characters before the null terminator.
 */
uint16_t str_len(const char *s);

/**
 * @brief  Search for a substring within a larger string.
 *
 * Brute-force search — suitable for short command strings (< 256 bytes).
 *
 * @param[in] haystack  String to search within (null-terminated).
 * @param[in] needle    Substring to search for (null-terminated).
 * @return Zero-based offset of the first match, or 0xFFFF if not found.
 */
uint16_t str_find(const char *haystack, const char *needle);

/**
 * @brief  Parse an unsigned 32-bit decimal integer from a string.
 *
 * Reads consecutive ASCII digits starting at s. Stops at the first
 * non-digit character. Does NOT skip leading whitespace.
 *
 * @param[in]  s    Pointer to the start of the digit sequence.
 * @param[out] out  Parsed value (only valid if at least one digit was read).
 * @return Pointer to the first character after the last digit consumed.
 */
const char *str_parse_uint32(const char *s, uint32_t *out);

#endif /* STR_UTILS_H */
