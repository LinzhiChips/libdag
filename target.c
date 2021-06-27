#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifdef USE_GCRYPT
#include <gcrypt.h>
#endif

#include "common.h"
#include "mine.h"


/* ----- Target check ------------------------------------------------------ */

/*
 * "target" is 32 bytes
 *
 * https://www.gnupg.org/documentation/manuals/gcrypt/MPI-library.html
 *
 * Difficulty had a local peak on 2018-03-27 at 3'339'796'333'912'015, which
 * is roughly 2^51.57. So we're only a factor of about 5000 from needing more
 * than 64 bits.
 */

#ifdef USE_GCRYPT

void get_target(uint8_t *target, const uint64_t difficulty[4])
{
	gcry_mpi_t a, b, q;
	unsigned i;

	a = gcry_mpi_new(257);
	b = gcry_mpi_new(257);
	q = gcry_mpi_new(257);

	/* a = 1 << 256 */
	gcry_mpi_set_ui(a, 1);
	gcry_mpi_mul_2exp(a, a, 256);

	/* b = difficulty */
	gcry_mpi_set_ui(b, 0);
	for (i = 0; i != 4; i++) {
		gcry_mpi_mul_2exp(b, b, 32);
		gcry_mpi_add_ui(b, b, difficulty[3 - i] >> 32);
		gcry_mpi_mul_2exp(b, b, 32);
		gcry_mpi_add_ui(b, b, (uint32_t) difficulty[3 - i]);
	}

	/* q = a / b */
	gcry_mpi_div(q, NULL, a, b, 0);

	/* @@@ is bitwise extraction REALLY the best we can do ?!? */
	memset(target, 0, TARGET_BYTES);
	for (i = 0; i != TARGET_BYTES * 8; i++)
		if (gcry_mpi_test_bit(q, i))
			target[TARGET_BYTES - 1 - (i >> 3)] |= 1 << (i & 7);

	gcry_mpi_release(a);
	gcry_mpi_release(b);
	gcry_mpi_release(q);
}

#else /* USE_GCRYPT */


/*
 * Bit vectors are LSb first, i.e. vec[0] is the LSb, vec[n - 1] is the MSb.
 */

static bool vec_le(const bool *a, unsigned a_n, const bool *b, unsigned b_n)
{
	unsigned i = a_n > b_n ? a_n : b_n;

	while (i--) {
		if ((i < a_n && a[i]) == (i < b_n && b[i]))
			continue;
		return i < b_n && b[i];
	}
	return 1;
}


static bool vec_sub(bool *a, unsigned a_n, const bool *b, unsigned b_n)
{
	bool borrow = 0;
	unsigned i;

	for (i = 0; i != a_n && i != b_n; i++) {
		int sum = a[i] - borrow - b[i];

		a[i] = sum & 1;
		borrow = sum < 0;
	}
	while (i != b_n)
		if (b[i++])
			return 1;
	while (i != a_n && borrow) {
		int sum = a[i] - borrow;

		a[i] = sum & 1;
		borrow = sum < 0;
		i++;
	}
	return borrow;
}


void get_target(uint8_t *target, const uint64_t difficulty[4])
{
	bool a[257] = { 0, };
	bool b[256];
	bool q[256];
	unsigned i;

	// a = 1 << 256
	a[256] = 1;

	// b = difficulty
	for (i = 0; i != 256; i++)
		b[i] = (difficulty[i >> 6] >> i) & 1;

	for (i = 0; i != 256; i++)
		q[i] = 0;

	for (i = 256; i--;) {
		if (vec_le(b, 256, a + i, 257 - i)) {
			q[i] = 1;
			vec_sub(a + i, 257 - i, b, 256);
		}
	}
	for (i = 0; i != TARGET_BYTES * 8; i++)
		target[TARGET_BYTES - 1 - (i >> 3)] |= q[i] << (i & 7);
}

#endif /* !USE_GCRYPT */

/*
 * The comparison in the Ethash reference (between a Python dictionary and a
 * string) does not make sense and appears to depend on largely unspecified
 * behaviour. ethminer provides a more useful reference comparing the 256-bit
 * value of "result" with the target (called "boundary").
 *
 * The comparison in ethminer is "result < boundary", unlike the <= one would
 * derive from "dictionary > target" in ethash.py. We therefore use
 * "result < target" as well.
 */

bool below_target(const uint8_t *result, const uint8_t *target)
{
	return memcmp(result, target, TARGET_BYTES) < 0;
}
