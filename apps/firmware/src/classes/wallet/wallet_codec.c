/* Umsetzung der Schreibweisen aus wallet_codec.h. Selbst geschrieben. */

#include <string.h>
#include "wallet_codec.h"
#include "wallet_hash.h"
#include "osk_crypto.h"

/* --- Base58Check --------------------------------------------------------- */

static const char B58[59] = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

static int b58_value(char c)
{
	for (int i = 0; i < 58; i++) {
		if (B58[i] == c)
			return i;
	}
	return -1;
}

osk_result_t osk_base58check_encode(const uint8_t *in, size_t len, char *out, size_t cap,
				    size_t *len_out)
{
	uint8_t buf[OSK_BASE58_MAX_BIN + 4];
	uint8_t digits[OSK_BASE58_MAX_TEXT];
	uint8_t check[32];
	size_t total, zeros = 0, digit_count = 0, written = 0;

	if (in == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (len == 0 || len > OSK_BASE58_MAX_BIN)
		return OSK_ERR_ARG;

	memcpy(buf, in, len);
	osk_hash256(in, len, check);
	memcpy(buf + len, check, 4);
	total = len + 4u;

	while (zeros < total && buf[zeros] == 0)
		zeros++;

	/* Fortgesetzte Division durch 58, niederwertige Stelle zuerst. */
	for (size_t start = zeros; start < total;) {
		uint32_t remainder = 0;
		size_t first_nonzero = total;

		for (size_t i = start; i < total; i++) {
			uint32_t cur = (remainder << 8) | buf[i];

			buf[i] = (uint8_t)(cur / 58u);
			remainder = cur % 58u;
			if (buf[i] != 0 && first_nonzero == total)
				first_nonzero = i;
		}
		if (digit_count >= sizeof(digits)) {
			osk_wipe(buf, sizeof(buf));
			osk_wipe(digits, sizeof(digits));
			return OSK_ERR_ARG;
		}
		digits[digit_count++] = (uint8_t)remainder;
		start = first_nonzero;
	}

	if (zeros + digit_count + 1u > cap) {
		osk_wipe(buf, sizeof(buf));
		osk_wipe(digits, sizeof(digits));
		return OSK_ERR_ARG;
	}
	for (size_t i = 0; i < zeros; i++)
		out[written++] = '1';
	for (size_t i = digit_count; i > 0; i--)
		out[written++] = B58[digits[i - 1u]];
	out[written] = '\0';
	*len_out = written;
	osk_wipe(buf, sizeof(buf));
	osk_wipe(digits, sizeof(digits));
	return OSK_OK;
}

osk_result_t osk_base58check_decode(const char *in, uint8_t *out, size_t cap, size_t *len_out)
{
	uint8_t bytes[OSK_BASE58_MAX_BIN + 4];
	uint8_t check[32];
	size_t text_len = 0, zeros = 0, used = 0;

	if (in == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	while (in[text_len] != '\0') {
		if (text_len >= OSK_BASE58_MAX_TEXT)
			return OSK_ERR_FORMAT;
		text_len++;
	}
	if (text_len < 5u)
		return OSK_ERR_FORMAT;
	memset(bytes, 0, sizeof(bytes));

	while (zeros < text_len && in[zeros] == '1')
		zeros++;
	for (size_t i = zeros; i < text_len; i++) {
		int value = b58_value(in[i]);
		uint32_t carry;

		if (value < 0)
			return OSK_ERR_FORMAT;
		carry = (uint32_t)value;
		for (size_t j = 0; j < used; j++) {
			uint32_t cur = (uint32_t)bytes[sizeof(bytes) - 1u - j] * 58u + carry;

			bytes[sizeof(bytes) - 1u - j] = (uint8_t)(cur & 0xFFu);
			carry = cur >> 8;
		}
		while (carry > 0) {
			if (used >= sizeof(bytes))
				return OSK_ERR_FORMAT;
			bytes[sizeof(bytes) - 1u - used] = (uint8_t)(carry & 0xFFu);
			used++;
			carry >>= 8;
		}
	}
	{
		size_t total = zeros + used;
		uint8_t full[OSK_BASE58_MAX_BIN + 4];
		size_t payload;

		if (total < 5u || total > sizeof(full))
			return OSK_ERR_FORMAT;
		memset(full, 0, sizeof(full));
		memcpy(full + zeros, bytes + sizeof(bytes) - used, used);
		payload = total - 4u;
		osk_hash256(full, payload, check);
		if (!osk_memeq(check, full + payload, 4))
			return OSK_ERR_FORMAT;
		if (payload > cap)
			return OSK_ERR_ARG;
		memcpy(out, full, payload);
		*len_out = payload;
		osk_wipe(full, sizeof(full));
	}
	osk_wipe(bytes, sizeof(bytes));
	return OSK_OK;
}

/* --- Bech32 und Bech32m -------------------------------------------------- */

static const char BECH32_SET[33] = "qpzry9x8gf2tvdw0s3jn54khce6mua7l";

static uint32_t bech32_polymod(uint32_t chk, uint8_t value)
{
	static const uint32_t GEN[5] = {0x3b6a57b2u, 0x26508e6du, 0x1ea119fau, 0x3d4233ddu,
					0x2a1462b3u};
	uint8_t top = (uint8_t)(chk >> 25);

	chk = ((chk & 0x1ffffffu) << 5) ^ value;
	for (unsigned i = 0; i < 5; i++) {
		if ((((unsigned)top >> i) & 1u) != 0)
			chk ^= GEN[i];
	}
	return chk;
}

static uint32_t bech32_hrp_expand(const char *hrp, size_t hrp_len, uint32_t chk)
{
	for (size_t i = 0; i < hrp_len; i++)
		chk = bech32_polymod(chk, (uint8_t)((unsigned char)hrp[i] >> 5));
	chk = bech32_polymod(chk, 0);
	for (size_t i = 0; i < hrp_len; i++)
		chk = bech32_polymod(chk, (uint8_t)((unsigned char)hrp[i] & 0x1fu));
	return chk;
}

/* Bits umpacken, ohne Rest zu verlieren. `pad` fuellt den letzten Teil auf
 * (beim Kodieren), sonst muss der Rest null sein (beim Lesen). */
static bool convert_bits(const uint8_t *in, size_t in_len, unsigned from, unsigned to, bool pad,
			 uint8_t *out, size_t cap, size_t *out_len)
{
	uint32_t acc = 0;
	unsigned bits = 0;
	uint32_t max = (uint32_t)((1u << to) - 1u);
	size_t written = 0;

	for (size_t i = 0; i < in_len; i++) {
		if (((uint32_t)in[i] >> from) != 0)
			return false;
		acc = (acc << from) | in[i];
		bits += from;
		while (bits >= to) {
			bits -= to;
			if (written >= cap)
				return false;
			out[written++] = (uint8_t)((acc >> bits) & max);
		}
	}
	if (pad) {
		if (bits > 0) {
			if (written >= cap)
				return false;
			out[written++] = (uint8_t)((acc << (to - bits)) & max);
		}
	} else if (bits >= from || ((acc << (to - bits)) & max) != 0) {
		return false;
	}
	*out_len = written;
	return true;
}

osk_result_t osk_bech32_encode_address(const char *hrp, uint8_t witness_version,
				       const uint8_t *program, size_t program_len, char *out,
				       size_t cap, size_t *len_out)
{
	uint8_t data[1 + 64];
	size_t data_len = 0, hrp_len, written = 0;
	uint32_t chk;
	uint32_t constant;

	if (hrp == NULL || program == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (witness_version > 16u)
		return OSK_ERR_ARG;
	if (program_len < 2u || program_len > 40u)
		return OSK_ERR_ARG;
	if (witness_version == 0 && program_len != 20u && program_len != 32u)
		return OSK_ERR_ARG;
	hrp_len = strlen(hrp);
	if (hrp_len == 0 || hrp_len > 8u)
		return OSK_ERR_ARG;

	data[0] = witness_version;
	if (!convert_bits(program, program_len, 8, 5, true, data + 1, sizeof(data) - 1u, &data_len))
		return OSK_ERR_ARG;
	data_len += 1u;

	if (hrp_len + 1u + data_len + 6u + 1u > cap)
		return OSK_ERR_ARG;

	constant = (witness_version == 0) ? 1u : 0x2bc830a3u;
	chk = bech32_hrp_expand(hrp, hrp_len, 1);
	for (size_t i = 0; i < data_len; i++)
		chk = bech32_polymod(chk, data[i]);
	for (unsigned i = 0; i < 6; i++)
		chk = bech32_polymod(chk, 0);
	chk ^= constant;

	for (size_t i = 0; i < hrp_len; i++)
		out[written++] = hrp[i];
	out[written++] = '1';
	for (size_t i = 0; i < data_len; i++)
		out[written++] = BECH32_SET[data[i]];
	for (unsigned i = 0; i < 6; i++)
		out[written++] = BECH32_SET[(chk >> (5u * (5u - i))) & 0x1fu];
	out[written] = '\0';
	*len_out = written;
	return OSK_OK;
}

osk_result_t osk_bech32_decode_address(const char *in, const char *expect_hrp, uint8_t *version_out,
				       uint8_t *program_out, size_t cap, size_t *program_len_out)
{
	char hrp[9];
	uint8_t data[1 + 64 + 6];
	size_t len = 0, split = 0, data_len, program_len = 0;
	bool lower = false, upper = false;
	uint32_t chk, constant;

	if (in == NULL || version_out == NULL || program_out == NULL || program_len_out == NULL)
		return OSK_ERR_ARG;
	while (in[len] != '\0') {
		unsigned char c = (unsigned char)in[len];

		if (c < 0x21u || c > 0x7Eu)
			return OSK_ERR_FORMAT;
		if (c >= 'a' && c <= 'z')
			lower = true;
		if (c >= 'A' && c <= 'Z')
			upper = true;
		if (len >= 90u)
			return OSK_ERR_FORMAT;
		len++;
	}
	/* Gemischte Schreibweise ist nach BIP173 verboten - sonst waere die
	 * Pruefsumme nicht eindeutig. */
	if (lower && upper)
		return OSK_ERR_FORMAT;
	if (len < 8u)
		return OSK_ERR_FORMAT;

	for (size_t i = len; i > 0; i--) {
		if (in[i - 1u] == '1') {
			split = i - 1u;
			break;
		}
	}
	if (split == 0 || split > 8u || len - split - 1u < 6u)
		return OSK_ERR_FORMAT;
	for (size_t i = 0; i < split; i++) {
		char c = in[i];

		if (c >= 'A' && c <= 'Z')
			c = (char)(c + ('a' - 'A'));
		hrp[i] = c;
	}
	hrp[split] = '\0';
	if (expect_hrp != NULL && strcmp(hrp, expect_hrp) != 0)
		return OSK_ERR_FORMAT;

	data_len = len - split - 1u;
	/* Kleinste moegliche Adresse: 1 Stelle Version, 4 Stellen Programm
	 * (2 Byte), 6 Stellen Pruefsumme. Alles darunter ist keine Adresse -
	 * und ohne diese Pruefung wuerde die Rechnung data_len - 7 unterlaufen. */
	if (data_len < 11u || data_len > sizeof(data))
		return OSK_ERR_FORMAT;
	for (size_t i = 0; i < data_len; i++) {
		char c = in[split + 1u + i];
		const char *found;

		if (c >= 'A' && c <= 'Z')
			c = (char)(c + ('a' - 'A'));
		found = strchr(BECH32_SET, c);
		if (found == NULL || c == '\0')
			return OSK_ERR_FORMAT;
		data[i] = (uint8_t)(found - BECH32_SET);
	}

	chk = bech32_hrp_expand(hrp, split, 1);
	for (size_t i = 0; i < data_len; i++)
		chk = bech32_polymod(chk, data[i]);
	constant = (data[0] == 0) ? 1u : 0x2bc830a3u;
	if (chk != constant)
		return OSK_ERR_FORMAT;

	if (data[0] > 16u)
		return OSK_ERR_FORMAT;
	if (!convert_bits(data + 1, data_len - 7u, 5, 8, false, program_out, cap, &program_len))
		return OSK_ERR_FORMAT;
	if (program_len < 2u || program_len > 40u)
		return OSK_ERR_FORMAT;
	if (data[0] == 0 && program_len != 20u && program_len != 32u)
		return OSK_ERR_FORMAT;
	*version_out = data[0];
	*program_len_out = program_len;
	return OSK_OK;
}

/* --- Ethereum: Adresse mit Pruefsumme nach EIP-55 ------------------------ */

osk_result_t osk_eth_address_text(const uint8_t address[20], char *out, size_t cap)
{
	static const char HEX[17] = "0123456789abcdef";
	char plain[41];
	uint8_t digest[OSK_KECCAK256_LEN];

	if (address == NULL || out == NULL)
		return OSK_ERR_ARG;
	if (cap < 43u)
		return OSK_ERR_ARG;
	for (unsigned i = 0; i < 20; i++) {
		plain[2u * i] = HEX[(address[i] >> 4) & 0x0Fu];
		plain[2u * i + 1u] = HEX[address[i] & 0x0Fu];
	}
	plain[40] = '\0';
	/* Die Pruefsumme laeuft ueber die Kleinbuchstaben-Schreibweise OHNE
	 * "0x" - das ist die Stelle, an der EIP-55 am haeufigsten falsch
	 * umgesetzt wird. */
	osk_keccak256((const uint8_t *)plain, 40, digest);
	out[0] = '0';
	out[1] = 'x';
	for (unsigned i = 0; i < 40; i++) {
		char c = plain[i];
		unsigned nibble = (i % 2u == 0) ? (unsigned)(digest[i / 2u] >> 4)
						: (unsigned)(digest[i / 2u] & 0x0Fu);

		if (c >= 'a' && c <= 'f' && nibble >= 8u)
			c = (char)(c - ('a' - 'A'));
		out[2u + i] = c;
	}
	out[42] = '\0';
	return OSK_OK;
}

osk_result_t osk_hex_decode(const char *in, size_t in_len, uint8_t *out, size_t cap,
			    size_t *len_out)
{
	if (in == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if ((in_len % 2u) != 0)
		return OSK_ERR_FORMAT;
	if (in_len / 2u > cap)
		return OSK_ERR_ARG;
	for (size_t i = 0; i < in_len / 2u; i++) {
		unsigned value = 0;

		for (unsigned k = 0; k < 2; k++) {
			char c = in[2u * i + k];
			unsigned digit;

			if (c >= '0' && c <= '9')
				digit = (unsigned)(c - '0');
			else if (c >= 'a' && c <= 'f')
				digit = (unsigned)(c - 'a') + 10u;
			else if (c >= 'A' && c <= 'F')
				digit = (unsigned)(c - 'A') + 10u;
			else
				return OSK_ERR_FORMAT;
			value = (value << 4) | digit;
		}
		out[i] = (uint8_t)value;
	}
	*len_out = in_len / 2u;
	return OSK_OK;
}
