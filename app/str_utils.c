/******************************************************************************
 * @file str_utils.c
 *
 * @par dependencies
 *      - str_utils.h (this module's interface)
 *
 * @author Yuna-Celisse
 *
 * @brief  Minimal string utilities for embedded use — no stdio dependency.
 *
 * All functions are hand-rolled to avoid pulling in the C standard
 * library (printf / sprintf families cost ~5-10 KB on Cortex-M0+).
 *
 * @version V1.0 2026-6-22
 *
 * @note 1 tab == 4 spaces!
 *****************************************************************************/

#include "str_utils.h"

/* ========== ftoa_fixed ========== */

/**
 * @brief  Convert float to a fixed-point decimal string (e.g. "-12.34").
 *
 * Writes up to 7 characters + null terminator into buf. The output
 * always includes one decimal digit. Assumes value fits in [-999, 9999].
 *
 * @param[out] buf      Output buffer (at least 12 bytes).
 * @param[in]  value    Floating-point value to convert.
 * @param[in]  decimals Number of decimal places (1 .. 4).
 * @return Pointer to buf (null-terminated).
 */
char *ftoa_fixed(char *buf, float value, uint8_t decimals)
{
    uint8_t pos = 0;
    int32_t whole;
    uint32_t frac;

    /* Handle negative values */
    if (value < 0.0f) {
        buf[pos++] = '-';
        value = -value;
    }

    whole = (int32_t)value;

    /* Compute fractional part (scaled by 10^decimals) */
    {
        float   scale = 1.0f;
        uint8_t i;
        for (i = 0; i < decimals; i++) {
            scale *= 10.0f;
        }
        frac = (uint32_t)((value - (float)whole) * scale + 0.5f);
        /* Handle rounding overflow (e.g. 0.999 → 1.000) */
        if (frac >= (uint32_t)scale) {
            whole++;
            frac = 0;
        }
    }

    /* Print integer part (simple itoa) */
    {
        char   tmp[6];
        uint8_t len = 0;
        if (whole == 0) {
            tmp[len++] = '0';
        } else {
            int32_t w = whole;
            while (w > 0) {
                tmp[len++] = '0' + (uint8_t)(w % 10);
                w /= 10;
            }
        }
        /* tmp is reversed; write forward into buf */
        while (len > 0) {
            buf[pos++] = tmp[--len];
        }
    }

    /* Decimal point + fractional part */
    buf[pos++] = '.';
    {
        uint32_t divisor = 1;
        uint8_t  i;
        for (i = 0; i < decimals - 1; i++) {
            divisor *= 10;
        }
        for (i = 0; i < decimals; i++) {
            buf[pos++] = '0' + (uint8_t)((frac / divisor) % 10);
            divisor /= 10;
        }
    }

    buf[pos] = '\0';

    /* Defensive: ensure the output never starts with "." or "-.".
     * Some float corner cases (e.g. subnormals, compiler-specific
     * optimisations) can cause the integer-part extraction above to
     * produce zero digits.  Shift the entire string right one place
     * and insert the missing '0' at position 0 or 1. */
    if (buf[0] == '.') {
        int8_t i;
        for (i = (int8_t)pos; i >= 0; i--) {
            buf[i + 1] = buf[i];
        }
        buf[0] = '0';
    } else if (buf[0] == '-' && buf[1] == '.') {
        int8_t i;
        for (i = (int8_t)pos; i >= 1; i--) {
            buf[i + 1] = buf[i];
        }
        buf[1] = '0';
    }

    return buf;
}

/* ========== str_len ========== */

uint16_t str_len(const char *s)
{
    uint16_t n = 0;
    while (*s++) {
        n++;
    }
    return n;
}

/* ========== str_find ========== */

uint16_t str_find(const char *haystack, const char *needle)
{
    uint16_t i;
    uint16_t j;
    uint16_t hayLen;
    uint16_t ndlLen;

    if (!haystack || !needle) {
        return 0xFFFF;
    }

    hayLen = str_len(haystack);
    ndlLen = str_len(needle);

    if (ndlLen == 0) {
        return 0;
    }
    if (ndlLen > hayLen) {
        return 0xFFFF;
    }

    for (i = 0; i <= hayLen - ndlLen; i++) {
        for (j = 0; j < ndlLen; j++) {
            if (haystack[i + j] != needle[j]) {
                break;
            }
        }
        if (j == ndlLen) {
            return i;
        }
    }

    return 0xFFFF;
}

/* ========== str_parse_uint32 ========== */

const char *str_parse_uint32(const char *s, uint32_t *out)
{
    uint32_t val = 0;

    while (*s >= '0' && *s <= '9') {
        val = val * 10 + (uint32_t)(*s - '0');
        s++;
    }

    *out = val;
    return s;
}
