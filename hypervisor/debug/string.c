/*
 * Copyright (C) 2018 Intel Corporation. All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <types.h>
#include <rtl.h>
#include <logmsg.h>
#include "lib.h"

/*
 * Convert a string to a long integer - decimal support only.
 */
int64_t strtol_deci(const char *nptr)
{
	const char *s = nptr;
	char c;
	uint64_t acc, cutoff, cutlim;
	int32_t neg = 0, any;
	uint64_t base = 10UL;

	/*
	 * Skip white space and pick up leading +/- sign if any.
	 */
	do {
		c = *s;
		s++;
	} while (is_space(c));

	if (c == '-') {
		neg = 1;
		c = *s;
		s++;
	} else if (c == '+') {
		c = *s;
		s++;
	} else {
		/* No sign character. */
	}

	/*
	 * Compute the cutoff value between legal numbers and illegal
	 * numbers.  That is the largest legal value, divided by the
	 * base.  An input number that is greater than this value, if
	 * followed by a legal input character, is too big.  One that
	 * is equal to this value may be valid or not; the limit
	 * between valid and invalid numbers is then based on the last
	 * digit.  For instance, if the range for longs is
	 * [-2147483648..2147483647] and the input base is 10,
	 * cutoff will be set to 214748364 and cutlim to either
	 * 7 (neg==0) or 8 (neg==1), meaning that if we have accumulated
	 * a value > 214748364, or equal but the next digit is > 7 (or 8),
	 * the number is too big, and we will return a range error.
	 *
	 * Set any if any `digits' consumed; make it negative to indicate
	 * overflow.
	 */
	cutoff = (neg != 0) ? LONG_MIN : LONG_MAX;
	cutlim = cutoff % base;
	cutoff /= base;
	acc = 0UL;
	any = 0;

	while ((c >= '0') && (c <= '9')) {
		c -= '0';
		if ((acc > cutoff) ||
			((acc == cutoff) && ((uint64_t)c > cutlim))) {
			any = -1;
			break;
		} else {
			acc *= base;
			acc += (uint64_t)c;
		}

		c = *s;
		s++;
	}

	if (any < 0) {
		acc = (neg != 0) ? LONG_MIN : LONG_MAX;
	} else if (neg != 0) {
		acc = ~acc + 1UL;
	} else {
		/* There is no overflow and no leading '-' exists. In such case
		 * acc already holds the right number. No action required. */
	}
	return (long)acc;
}

static inline char hex_digit_value(char ch)
{
	char c;
	if (('0' <= ch) && (ch <= '9')) {
		c = ch - '0';
	} else if (('a' <= ch) && (ch <= 'f')) {
		c = ch - 'a' + 10;
	} else if (('A' <= ch) && (ch <= 'F')) {
		c = ch - 'A' + 10;
	} else {
		c = -1;
	}
	return c;
}

/*
 * Convert a string to an uint64_t integer - hexadecimal support only.
 */
uint64_t strtoul_hex(const char *nptr)
{
	const char *s = nptr;
	char c, digit;
	uint64_t acc, cutoff, cutlim;
	uint64_t base = 16UL;
	int32_t any;

	/*
	 * See strtol for comments as to the logic used.
	 */
	do {
		c = *s;
		s++;
	} while (is_space(c));

	if ((c == '0') && ((*s == 'x') || (*s == 'X'))) {
		c = s[1];
		s += 2;
	}

	cutoff = ULONG_MAX / base;
	cutlim = ULONG_MAX % base;
	acc = 0UL;
	any = 0;
	digit = hex_digit_value(c);
	while (digit >= 0) {
		if ((acc > cutoff) || ((acc == cutoff) && ((uint64_t)digit > cutlim))) {
			any = -1;
			break;
		} else {
			acc *= base;
			acc += (uint64_t)digit;
		}

		c = *s;
		s++;
		digit = hex_digit_value(c);
	}

	if (any < 0) {
		acc = ULONG_MAX;
	}
	return acc;
}

int32_t strcmp(const char *s1_arg, const char *s2_arg)
{
	const char *str1 = s1_arg;
	const char *str2 = s2_arg;

	while (((*str1) != '\0') && ((*str2) != '\0') && ((*str1) == (*str2))) {
		str1++;
		str2++;
	}

	return *str1 - *str2;
}

char *strncpy_s(char *d_arg, size_t dmax, const char *s_arg, size_t slen_arg)
{
	const char *s = s_arg;
	char *d = d_arg;
	char *pret;
	size_t dest_avail;
	uint64_t overlap_guard;
	size_t slen = slen_arg;

	if ((d == NULL) || (s == NULL)) {
		pr_err("%s: invlaid src or dest buffer", __func__);
		pret = NULL;
	} else {
		pret = d_arg;
	}

	if (pret != NULL) {
		if ((dmax == 0U) || (slen == 0U)) {
			pr_err("%s: invlaid length of src or dest buffer", __func__);
			pret =  NULL;
		}
	}

	/* if d equal to s, just return d; else execute the below code */
	if ((pret != NULL) && (d != s)) {
		overlap_guard = (uint64_t)((d > s) ? (d - s - 1) : (s - d - 1));
		dest_avail = dmax;

		while (dest_avail > 0U) {
			bool complete = false;

			if (overlap_guard == 0U) {
				pr_err("%s: overlap happened.", __func__);
				d--;
				*d = '\0';
				pret = NULL;
				/* copy complete */
				complete = true;
			} else {
				if (slen == 0U) {
					*d = '\0';
					/* copy complete */
					complete = true;
				} else {
					*d = *s;
					if (*d == '\0') {
						/* copy complete */
						complete = true;
					} else {
						d++;
						s++;
						slen--;
						dest_avail--;
						overlap_guard--;
					}
				}
			}

			if (complete) {
				break;
			}
		}

		if (dest_avail == 0U) {
			pr_err("%s: dest buffer has no enough space.", __func__);

			/* to avoid a string that is not null-terminated in dest buffer */
			pret[dmax - 1] = '\0';
		}
	}

	return pret;
}

char *strchr(char *s_arg, char ch)
{
	char *s = s_arg;
	while ((*s != '\0') && (*s != ch)) {
		++s;
	}

	return ((*s) != '\0') ? s : NULL;
}
