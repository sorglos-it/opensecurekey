/* BIP39, selbst geschrieben gegen die Spezifikation.
 *
 * Reihenfolge der Bits, weil genau dort die Fehler sitzen: die Entropie
 * wird als Bitstrom gelesen, hoechstwertiges Bit zuerst; dahinter kommen
 * ent_len/4 Bits aus dem ersten Byte von SHA-256(Entropie); je 11 Bits sind
 * eine Wortnummer. */

#include <string.h>
#include "wallet_bip39.h"
#include "wallet_hash.h"
#include "osk_crypto.h"
#include "osk_se050.h"

int osk_bip39_word_index(const char *word, size_t len)
{
	size_t lo = 0, hi = OSK_BIP39_WORDS;

	if (word == NULL || len == 0)
		return -1;
	while (lo < hi) {
		size_t mid = lo + (hi - lo) / 2u;
		const char *candidate = osk_bip39_english[mid];
		size_t clen = strlen(candidate);
		size_t n = len < clen ? len : clen;
		int cmp = memcmp(candidate, word, n);

		if (cmp == 0 && clen != len)
			cmp = (clen < len) ? -1 : 1;
		if (cmp == 0)
			return (int)mid;
		if (cmp < 0)
			lo = mid + 1u;
		else
			hi = mid;
	}
	return -1;
}

static bool entropy_len_ok(size_t ent_len)
{
	return ent_len >= OSK_BIP39_MIN_ENTROPY && ent_len <= OSK_BIP39_MAX_ENTROPY &&
	       (ent_len % 4u) == 0;
}

osk_result_t osk_bip39_from_entropy(const uint8_t *ent, size_t ent_len, char *out, size_t cap,
				    size_t *len_out)
{
	uint8_t bits[OSK_BIP39_MAX_ENTROPY + 1];
	uint8_t digest[OSK_SHA256_LEN];
	size_t words, written = 0;

	if (ent == NULL || out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	if (!entropy_len_ok(ent_len))
		return OSK_ERR_ARG;

	memcpy(bits, ent, ent_len);
	osk_sha256(ent, ent_len, digest);
	bits[ent_len] = digest[0]; /* hoechstens 8 Pruefbits, also ein Byte */
	words = (ent_len * 8u + ent_len / 4u) / 11u;

	for (size_t w = 0; w < words; w++) {
		size_t index = 0;
		const char *word;
		size_t wlen;

		for (unsigned b = 0; b < 11; b++) {
			size_t bit = w * 11u + b;
			unsigned value = ((unsigned)bits[bit / 8u] >> (7u - (bit % 8u))) & 1u;

			index = (index << 1) | value;
		}
		word = osk_bip39_english[index];
		wlen = strlen(word);
		if (written + wlen + (w > 0 ? 1u : 0u) + 1u > cap) {
			osk_wipe(out, cap);
			osk_wipe(bits, sizeof(bits));
			return OSK_ERR_ARG;
		}
		if (w > 0)
			out[written++] = ' ';
		memcpy(out + written, word, wlen);
		written += wlen;
	}
	out[written] = '\0';
	*len_out = written;
	osk_wipe(bits, sizeof(bits));
	osk_wipe(digest, sizeof(digest));
	return OSK_OK;
}

osk_result_t osk_bip39_generate(unsigned words, char *out, size_t cap, size_t *len_out)
{
	uint8_t entropy[OSK_BIP39_MAX_ENTROPY];
	size_t ent_len;
	osk_result_t res;

	switch (words) {
	case 12:
		ent_len = 16;
		break;
	case 15:
		ent_len = 20;
		break;
	case 18:
		ent_len = 24;
		break;
	case 21:
		ent_len = 28;
		break;
	case 24:
		ent_len = 32;
		break;
	default:
		return OSK_ERR_ARG;
	}
	res = osk_se050_random(entropy, ent_len);
	if (res != OSK_OK) {
		osk_wipe(entropy, sizeof(entropy));
		return res;
	}
	res = osk_bip39_from_entropy(entropy, ent_len, out, cap, len_out);
	osk_wipe(entropy, sizeof(entropy));
	return res;
}

/* Zerlegt die Mnemonik in Wortnummern. Erlaubt sind Kleinbuchstaben a-z und
 * einzelne Leerzeichen; alles andere ist ein Formatfehler. Fuehrende und
 * abschliessende Leerzeichen werden uebergangen, damit ein abgetipptes Wort
 * nicht an einem Leerzeichen scheitert. */
static osk_result_t split_words(const char *mnemonic, uint16_t *index_out, size_t *count_out)
{
	size_t count = 0, i = 0;

	if (mnemonic == NULL)
		return OSK_ERR_ARG;
	while (mnemonic[i] == ' ')
		i++;
	while (mnemonic[i] != '\0') {
		size_t start = i;
		size_t len;
		int index;

		while (mnemonic[i] >= 'a' && mnemonic[i] <= 'z')
			i++;
		len = i - start;
		if (len == 0)
			return OSK_ERR_FORMAT; /* fremdes Zeichen */
		if (count >= OSK_BIP39_MAX_WORDS)
			return OSK_ERR_FORMAT;
		index = osk_bip39_word_index(mnemonic + start, len);
		if (index < 0)
			return OSK_ERR_FORMAT;
		index_out[count++] = (uint16_t)index;
		if (mnemonic[i] == '\0')
			break;
		if (mnemonic[i] != ' ')
			return OSK_ERR_FORMAT;
		while (mnemonic[i] == ' ')
			i++;
	}
	if (count != 12u && count != 15u && count != 18u && count != 21u && count != 24u)
		return OSK_ERR_FORMAT;
	*count_out = count;
	return OSK_OK;
}

static osk_result_t to_entropy(const char *mnemonic, uint8_t *ent_out, size_t cap, size_t *len_out,
			       bool check_sum)
{
	uint16_t index[OSK_BIP39_MAX_WORDS];
	uint8_t bits[OSK_BIP39_MAX_ENTROPY + 1];
	uint8_t digest[OSK_SHA256_LEN];
	size_t count = 0, ent_len, check_bits;
	osk_result_t res;

	res = split_words(mnemonic, index, &count);
	if (res != OSK_OK)
		return res;
	ent_len = count * 11u / 33u * 4u; /* 12 -> 16, 24 -> 32 */
	check_bits = ent_len / 4u;
	if (ent_out != NULL && cap < ent_len)
		return OSK_ERR_ARG;

	memset(bits, 0, sizeof(bits));
	for (size_t w = 0; w < count; w++) {
		for (unsigned b = 0; b < 11; b++) {
			size_t bit = w * 11u + b;
			unsigned value = ((unsigned)index[w] >> (10u - b)) & 1u;

			bits[bit / 8u] = (uint8_t)(bits[bit / 8u] | (value << (7u - (bit % 8u))));
		}
	}
	if (check_sum) {
		uint8_t mask = (uint8_t)(0xFFu << (8u - check_bits));

		osk_sha256(bits, ent_len, digest);
		if (((bits[ent_len] ^ digest[0]) & mask) != 0) {
			osk_wipe(bits, sizeof(bits));
			osk_wipe(digest, sizeof(digest));
			return OSK_ERR_FORMAT;
		}
	}
	if (ent_out != NULL)
		memcpy(ent_out, bits, ent_len);
	if (len_out != NULL)
		*len_out = ent_len;
	osk_wipe(bits, sizeof(bits));
	osk_wipe(digest, sizeof(digest));
	return OSK_OK;
}

osk_result_t osk_bip39_check(const char *mnemonic)
{
	return to_entropy(mnemonic, NULL, 0, NULL, true);
}

osk_result_t osk_bip39_to_entropy(const char *mnemonic, uint8_t *out, size_t cap, size_t *len_out)
{
	if (out == NULL || len_out == NULL)
		return OSK_ERR_ARG;
	return to_entropy(mnemonic, out, cap, len_out, true);
}

static bool ascii_only(const char *text, size_t *len_out)
{
	size_t len = 0;

	while (text[len] != '\0') {
		unsigned char c = (unsigned char)text[len];

		if (c < 0x20u || c > 0x7Eu)
			return false;
		if (len > 256u)
			return false;
		len++;
	}
	*len_out = len;
	return true;
}

osk_result_t osk_bip39_to_seed(const char *mnemonic, const char *passphrase,
			       uint8_t seed[OSK_BIP39_SEED_LEN])
{
	char salt[8 + 257];
	size_t mnemonic_len = 0, pass_len = 0;
	osk_result_t res;

	if (mnemonic == NULL || seed == NULL)
		return OSK_ERR_ARG;
	if (!ascii_only(mnemonic, &mnemonic_len) || mnemonic_len == 0)
		return OSK_ERR_FORMAT;
	if (passphrase != NULL && !ascii_only(passphrase, &pass_len))
		return OSK_ERR_FORMAT;

	memcpy(salt, "mnemonic", 8);
	if (pass_len > 0)
		memcpy(salt + 8, passphrase, pass_len);
	res = osk_pbkdf2_sha512((const uint8_t *)mnemonic, mnemonic_len, (const uint8_t *)salt,
				8u + pass_len, 2048, seed, OSK_BIP39_SEED_LEN);
	osk_wipe(salt, sizeof(salt));
	return res;
}
