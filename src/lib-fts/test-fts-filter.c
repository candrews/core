/* Copyright (c) 2014-2016 Dovecot authors, see the included COPYING file */

#include "lib.h"
#include "sha2.h"
#include "str.h"
#include "unichar.h"
#include "test-common.h"
#include "fts-language.h"
#include "fts-filter.h"

#include <stdio.h>

static const char *const stopword_settings[] = {"stopwords_dir", TEST_STOPWORDS_DIR, NULL};
static struct fts_language english_language = { .name = "en" };
static struct fts_language french_language = { .name = "fr" };
static struct fts_language norwegian_language = { .name = "no" };
#if defined(HAVE_LIBICU) && defined(HAVE_FTS_STEMMER)
static struct fts_language swedish_language = { .name = "sv" };
#endif

static void test_fts_filter_find(void)
{
	test_begin("fts filter find");
	test_assert(fts_filter_find("stopwords") == fts_filter_stopwords);
	test_assert(fts_filter_find("snowball") == fts_filter_stemmer_snowball);
	test_assert(fts_filter_find("normalizer-icu") == fts_filter_normalizer_icu);
	test_assert(fts_filter_find("lowercase") == fts_filter_lowercase);
	test_assert(fts_filter_find("contractions") == fts_filter_contractions);
	test_end();
}


static void test_fts_filter_contractions_fail(void)
{

	struct fts_filter *filter;
	const char *error;

	test_begin("fts filter contractions, unsupported language");
	test_assert(fts_filter_create(fts_filter_contractions, NULL, &english_language, NULL, &filter, &error) != 0);
	test_assert(error != NULL);
	test_end();
}

static void test_fts_filter_contractions_fr(void)
{
	struct {
		const char *input;
		const char *output;
	} tests[] = {
		{ "foo", "foo" },
		{ "you're", "you're" },
		{ "l'homme", "homme" },
		{ "l\xE2\x80\x99homme", "homme" },
		{ "aujourd'hui", "aujourd'hui" },
		{ "qu\xE2\x80\x99il", "il" },
		{ "qu'il", "il" },
		{ "du'il", "du'il" },
		{ "que", "que" },
		{ "'foobar'", "'foobar'" },
		{ "foo'bar", "foo'bar" },
		{ "a'foo", "a'foo" },
		{ "cu'", "cu'" },
		{ "qu", "qu" },
		{ "d", "d" },
		{ "qu'", NULL },
		{ "j'adore", "adore" },
		{ "quelqu'un", "quelqu'un" },
		{ "l'esprit", "esprit" }
	};
	struct fts_filter *filter;
	const char *error;
	const char *token;
	unsigned int i;
	int ret;

	test_begin("fts filter contractions, French");
	test_assert(fts_filter_create(fts_filter_contractions, NULL, &french_language, NULL, &filter, &error) == 0);

	for (i = 0; i < N_ELEMENTS(tests); i++) {
		token = tests[i].input;
		ret = fts_filter_filter(filter, &token, &error);
		test_assert(ret >= 0);
		if (ret > 0)
			test_assert_idx(strcmp(token, tests[i].output) == 0, i);
		else if (ret == 0)
			test_assert_idx(token == NULL && tests[i].output == NULL, i);
	}
	fts_filter_unref(&filter);
	test_end();
}

static void test_fts_filter_lowercase(void)
{
	struct {
		const char *input;
		const char *output;
	} tests[] = {
		{ "foo", "foo" },
		{ "FOO", "foo" },
		{ "fOo", "foo" }
	};
	struct fts_filter *filter;
	const char *error;
	const char *token;
	unsigned int i;

	test_begin("fts filter lowercase");
	test_assert(fts_filter_create(fts_filter_lowercase, NULL, &english_language, NULL, &filter, &error) == 0);

	for (i = 0; i < N_ELEMENTS(tests); i++) {
		token = tests[i].input;
		test_assert_idx(fts_filter_filter(filter, &token, &error) > 0 &&
				strcmp(token, tests[i].output) == 0, 0);
	}
	fts_filter_unref(&filter);
	test_end();
}

#ifdef HAVE_LIBICU
static void test_fts_filter_lowercase_utf8(void)
{
	struct {
		const char *input;
		const char *output;
	} tests[] = {
		{ "f\xC3\x85\xC3\x85", "f\xC3\xA5\xC3\xA5" },
		{ "F\xC3\x85\xC3\x85", "f\xC3\xA5\xC3\xA5" },
		{ "F\xC3\x85\xC3\xA5", "f\xC3\xA5\xC3\xA5" }
	};
	struct fts_filter *filter;
	const char *error;
	const char *token;
	unsigned int i;

	test_begin("fts filter lowercase, UTF8");
	test_assert(fts_filter_create(fts_filter_lowercase, NULL, &english_language, NULL, &filter, &error) == 0);

	for (i = 0; i < N_ELEMENTS(tests); i++) {
		token = tests[i].input;
		test_assert_idx(fts_filter_filter(filter, &token, &error) > 0 &&
				strcmp(token, tests[i].output) == 0, 0);
	}
	fts_filter_unref(&filter);
	test_end();
}
#endif

static void test_fts_filter_stopwords_eng(void)
{
	struct fts_filter *filter;
	const char *error;
	int ret;
	const char *input[] = {"an", "elephant", "and", "a", "bear",
	                       "drive", "by", "for", "no", "reason",
	                       "they", "will", "not",  "sing", NULL};
	const char *output[] = {NULL, "elephant", NULL, NULL, "bear",
	                       "drive", NULL, NULL, NULL, "reason",
	                       NULL, NULL, NULL,  "sing"};
	const char **ip, **op;
	const char *token;

	test_begin("fts filter stopwords, English");
	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &english_language, stopword_settings, &filter, &error) == 0);

	ip = input;
	op = output;
	while (*ip != NULL) {
		token = *ip;
		ret = fts_filter_filter(filter, &token, &error);
		if (ret <= 0) {
			test_assert(ret == 0);
			test_assert(*op == NULL);
		} else {
			test_assert(*op != NULL);
			test_assert(strcmp(*ip, token)  == 0);
		}
		op++;
		ip++;
	}

	fts_filter_unref(&filter);
	test_assert(filter == NULL);
	test_end();
}

static void test_fts_filter_stopwords_fin(void)
{
	const struct fts_language finnish = { .name = "fi" };
	struct fts_filter *filter;
	const char *error;
	int ret;
	const char *input[] = {"olla", "vaiko", "eik\xC3\xB6", "olla",
	                       "kenest\xC3\xA4", "ja", "joista", "jonka",
	                       "testi", NULL};
	const char *output[] = {NULL, "vaiko", "eik\xC3\xB6", NULL, NULL,
	                        NULL, NULL, NULL, "testi"};
	const char *input2[] =
		{"kuka", "kenet", "keneen", "testi", "eiv\xC3\xA4t", NULL};
	const char *output2[] = {NULL, NULL, NULL, "testi", NULL};
	const char **ip, **op;
	const char *token;

	test_begin("fts filter stopwords, Finnish");
	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &finnish, stopword_settings, &filter, &error) == 0);

	ip = input;
	op = output;
	while (*ip != NULL) {
		token = *ip;
		ret = fts_filter_filter(filter, &token, &error);
		if (ret <= 0) {
			test_assert(ret == 0);
			test_assert(*op == NULL);
		} else {
			test_assert(*op != NULL);
			test_assert(strcmp(*ip, token)  == 0);
		}
		op++;
		ip++;
	}

	fts_filter_unref(&filter);
	test_assert(filter == NULL);

	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &finnish, stopword_settings, &filter, &error) == 0);
	ip = input2;
	op = output2;
	while (*ip != NULL) {
		token = *ip;
		ret = fts_filter_filter(filter, &token, &error);
		if (ret <= 0) {
			test_assert(ret == 0);
			test_assert(*op == NULL);
		} else {
			test_assert(*op != NULL);
			test_assert(strcmp(*ip, token)  == 0);
		}
		op++;
		ip++;
	}

	fts_filter_unref(&filter);
	test_assert(filter == NULL);
	test_end();
}

static void test_fts_filter_stopwords_fra(void)
{
	struct fts_filter *filter;
	const char *error;
	int ret;

	const char *input[] = {"e\xC3\xBBt", "soyez", "soi", "peut", "que",
	                       "quelconque", "\xC3\xA9t\xC3\xA9",
	                       "l\xE2\x80\x99""av\xC3\xA8nement",
	                       NULL};
	const char *output[] = {NULL, NULL, NULL, "peut", NULL,
	                        "quelconque", NULL, 
	                        "l\xE2\x80\x99""av\xC3\xA8nement",};
	const char **ip, **op;
	const char *token;

	test_begin("fts filter stopwords, French");
	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &french_language, stopword_settings, &filter, &error) == 0);

	ip = input;
	op = output;
	while (*ip != NULL) {
		token = *ip;
		ret = fts_filter_filter(filter, &token, &error);
		if (ret <= 0) {
			test_assert(ret == 0);
			test_assert(*op == NULL);
		} else {
			test_assert(*op != NULL);
			test_assert(strcmp(*ip, token)  == 0);
		}
		op++;
		ip++;
	}

	fts_filter_unref(&filter);
	test_assert(filter == NULL);
	test_end();
}

static void test_fts_filter_stopwords_no(void)
{
	struct fts_filter *filter;
	const char *error;
	int ret;

	const char *input[] = {"og", "d\xC3\xA5", "medlemsstatane", "har",
	                       "bunde", "seg", "til", "\xC3\xA5", "fremje",
	                       "allmenn", "v\xC3\xB8rdnad", "for", "pakta",
	                       "og", "halde", "seg", "etter", "menneskerettane",
	                       "og", "den", "grunnleggjande", "fridomen", "i",
	                       "samarbeid", "med", "Dei", "Sameinte",
	                       "Nasjonane", NULL};

	const char *output[] = {NULL, NULL, "medlemsstatane", NULL,
	                       "bunde", NULL, NULL, NULL, "fremje",
	                       "allmenn", "v\xC3\xB8rdnad", NULL, "pakta",
	                       NULL, "halde", NULL, NULL, "menneskerettane",
	                       NULL, NULL, "grunnleggjande", "fridomen", NULL,
	                       "samarbeid", NULL, "Dei", "Sameinte",
	                       "Nasjonane"};
	const char **ip, **op;
	const char *token;

	test_begin("fts filter stopwords, Norwegian");
	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &norwegian_language, stopword_settings, &filter, &error) == 0);

	ip = input;
	op = output;
	while (*ip != NULL) {
		token = *ip;
		ret = fts_filter_filter(filter, &token, &error);
		if (ret <= 0) {
			test_assert(ret == 0);
			test_assert(*op == NULL);
		} else {
			test_assert(*op != NULL);
			test_assert(strcmp(*ip, token)  == 0);
		}
		op++;
		ip++;
	}

	fts_filter_unref(&filter);
	test_assert(filter == NULL);
	test_end();
}

static void test_fts_filter_stopwords_fail_lazy_init(void)
{
	const struct fts_language unknown = { .name = "bebobidoop" };
	struct fts_filter *filter = NULL;
	const char *error = NULL, *token = "foobar";

	test_begin("fts filter stopwords, fail filter() (lazy init)");
	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &unknown, stopword_settings, &filter, &error) == 0);
	test_assert(filter != NULL && error == NULL);
	test_assert(fts_filter_filter(filter, &token, &error) < 0 && error != NULL);
	fts_filter_unref(&filter);
	test_end();

}

static void test_fts_filter_stopwords_malformed(void)
{
	const struct fts_language malformed = { .name = "malformed" };
	struct fts_filter *filter = NULL;
	const char *error = NULL, *token = "foobar";

	test_begin("fts filter stopwords, malformed list");
	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &malformed, stopword_settings, &filter, &error) == 0);
	test_expect_error_string("seems empty. Is the file correctly formatted?");
	test_assert(fts_filter_filter(filter, &token, &error) > 0);
	test_expect_no_more_errors();
	fts_filter_unref(&filter);
	test_end();

}

#ifdef HAVE_FTS_STEMMER
static void test_fts_filter_stemmer_snowball_stem_english(void)
{
	struct fts_filter *stemmer;
	const char *error;
	const char *token = NULL;
	const char * const tokens[] = {
		"dries" ,"friendlies", "All", "human", "beings", "are",
		 "born", "free", "and", "equal", "in", "dignity", "and",
		 "rights", "They", "are", "endowed", "with", "reason", "and",
		 "conscience", "and", "should", "act", "towards", "one",
		 "another", "in", "a", "spirit", "of", "brotherhood", NULL};
	const char * const bases[] = {
		"dri" ,"friend", "All", "human", "be", "are", "born", "free",
		"and", "equal", "in", "digniti", "and", "right", "They", "are",
		"endow", "with", "reason", "and", "conscienc", "and", "should",
		"act", "toward", "one", "anoth", "in", "a", "spirit", "of",
		"brotherhood", NULL};
	const char * const *tpp;
	const char * const *bpp;

	test_begin("fts filter stem English");
	test_assert(fts_filter_create(fts_filter_stemmer_snowball, NULL, &english_language, NULL, &stemmer, &error) == 0);
	bpp = bases;
	for (tpp=tokens; *tpp != NULL; tpp++) {
		token = *tpp;
		test_assert(fts_filter_filter(stemmer, &token, &error) > 0);
		test_assert(token != NULL);
		test_assert(null_strcmp(token, *bpp) == 0);
		bpp++;
	}
	fts_filter_unref(&stemmer);
	test_assert(stemmer == NULL);
	test_end();
}

static void test_fts_filter_stemmer_snowball_stem_french(void)
{
	struct fts_filter *stemmer;
	const char *error;
	const char *token = NULL;
	const char * const tokens[] = {
		"Tous", "les", "\xC3\xAAtres", "humains", "naissent",
		"libres", "et",	"\xC3\xA9gaux", "en", "dignit\xC3\xA9",
		"et", "en", "droits", NULL};
	const char * const bases[] = {
		"Tous" ,"le", "\xC3\xAAtre", "humain", "naissent", "libr", "et",
		"\xC3\xA9gal", "en", "dignit", "et", "en", "droit", NULL};
	const char * const *tpp;
	const char * const *bpp;

	test_begin("fts filter stem French");
	test_assert(fts_filter_create(fts_filter_stemmer_snowball, NULL, &french_language, NULL, &stemmer, &error) == 0);
	bpp = bases;
	for (tpp=tokens; *tpp != NULL; tpp++) {
		token = *tpp;
		test_assert(fts_filter_filter(stemmer, &token, &error) > 0);
		test_assert(token != NULL);
		test_assert(null_strcmp(token, *bpp) == 0);
		bpp++;
	}
	fts_filter_unref(&stemmer);
	test_assert(stemmer == NULL);
	test_end();
}

static void test_fts_filter_stopwords_stemmer_eng(void)
{
	int ret;
	struct fts_filter *stemmer;
	struct fts_filter *filter;
	const char *error;
	const char *token = NULL;
	const char * const tokens[] = {
		"dries" ,"friendlies", "All", "human", "beings", "are",
		 "born", "free", "and", "equal", "in", "dignity", "and",
		 "rights", "They", "are", "endowed", "with", "reason", "and",
		 "conscience", "and", "should", "act", "towards", "one",
		 "another", "in", "a", "spirit", "of", "brotherhood", NULL};
	const char * const bases[] = {
		"dri" ,"friend", "All", "human", "be", NULL, "born", "free",
		NULL, "equal", NULL, "digniti", NULL, "right", "They", NULL,
		"endow", NULL, "reason", NULL, "conscienc", NULL, "should",
		"act", "toward", "one", "anoth", NULL, NULL, "spirit", NULL,
		"brotherhood", NULL};
	const char * const *tpp;
	const char * const *bpp;

	test_begin("fts filters stopwords and stemming chained, English");

	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &english_language, stopword_settings, &filter, &error) == 0);
	test_assert(fts_filter_create(fts_filter_stemmer_snowball, filter, &english_language, NULL, &stemmer, &error) == 0);

	bpp = bases;
	for (tpp=tokens; *tpp != NULL; tpp++) {
		token = *tpp;
		ret = fts_filter_filter(stemmer, &token, &error);
		test_assert(ret >= 0);
		if (ret == 0)
			test_assert(*bpp == NULL);
		else {
			test_assert(*bpp != NULL);
			test_assert(null_strcmp(*bpp, token)  == 0);
		}
		bpp++;
	}
	fts_filter_unref(&stemmer);
	fts_filter_unref(&filter);
	test_assert(stemmer == NULL);
	test_assert(filter == NULL);
	test_end();
}
#endif

#ifdef HAVE_LIBICU
static void test_fts_filter_normalizer_swedish_short(void)
{
	struct fts_filter *norm = NULL;
	const char *input[] = {
		"Vem",
		"\xC3\x85",
		"\xC3\x85\xC3\x84\xC3\x96",
		"Vem kan segla f\xC3\xB6rutan vind?\n"
		"\xC3\x85\xC3\x84\xC3\x96\xC3\xB6\xC3\xA4\xC3\xA5"
	};
	const char *expected_output[] = {
		"vem",
		"a",
		"aao",
		"vem kan segla forutan vind?\naaooaa"
	};
	const char * const settings[] =
		{"id", "Any-Lower; NFKD; [: Nonspacing Mark :] Remove; NFC", NULL};
	const char *error = NULL;
	const char *token = NULL;
	unsigned int i;

	test_begin("fts filter normalizer Swedish short text");

	test_assert(fts_filter_create(fts_filter_normalizer_icu, NULL, NULL, settings, &norm, &error) == 0);
	for (i = 0; i < N_ELEMENTS(input); i++) {
		token = input[i];
		test_assert_idx(fts_filter_filter(norm, &token, &error) == 1, i);
		test_assert_idx(null_strcmp(token, expected_output[i]) == 0, i);
	}
	fts_filter_unref(&norm);
	test_assert(norm == NULL);
	test_end();
}

static void test_fts_filter_normalizer_swedish_short_default_id(void)
{
	struct fts_filter *norm = NULL;
	const char *input[] = {
		"Vem",
		"\xC3\x85",
		"\xC3\x85\xC3\x84\xC3\x96",
		"Vem kan segla f\xC3\xB6rutan vind?\n"
		"\xC3\x85\xC3\x84\xC3\x96\xC3\xB6\xC3\xA4\xC3\xA5"
	};
	const char *expected_output[] = {
		"vem",
		"a",
		"aao",
		"vemkanseglaforutanvind?\naaooaa"
	};
	const char *error = NULL;
	const char *token = NULL;
	unsigned int i;

	test_begin("fts filter normalizer Swedish short text using default ID");

	test_assert(fts_filter_create(fts_filter_normalizer_icu, NULL, NULL, NULL, &norm, &error) == 0);
	for (i = 0; i < N_ELEMENTS(input); i++) {
		token = input[i];
		test_assert_idx(fts_filter_filter(norm, &token, &error) == 1, i);
		test_assert_idx(null_strcmp(token, expected_output[i]) == 0, i);
	}
	fts_filter_unref(&norm);
	test_assert(norm == NULL);
	test_end();
}

/* UDHRDIR comes from Automake AM_CPPFLAGS */
#define UDHR_FRA_NAME "/udhr_fra.txt"
static void test_fts_filter_normalizer_french(void)
{
	struct fts_filter *norm = NULL;
	FILE *input;
	const char * const settings[] =
		{"id", "Any-Lower; NFKD; [: Nonspacing Mark :] Remove", NULL};
	char buf[4096] = {0};
	const char *error = NULL;
	const char *tokens;
	unsigned char sha512_digest[SHA512_RESULTLEN];
	struct sha512_ctx ctx;
	const unsigned char correct_digest[] = {
		0x78, 0x1e, 0xb9, 0x04, 0xa4, 0x92, 0xca, 0x88,
		0x1e, 0xef, 0x7b, 0xc8, 0x3e, 0x4a, 0xa8, 0xdb,
		0x9c, 0xd4, 0x42, 0x5c, 0x64, 0x81, 0x06, 0xd5,
		0x72, 0x93, 0x38, 0x0c, 0x09, 0xce, 0xbe, 0xdf,
		0x65, 0xff, 0x36, 0x35, 0x05, 0x77, 0xcc, 0xc6,
		0xff, 0x44, 0x2c, 0x31, 0x10, 0x00, 0xf6, 0x8d,
		0x15, 0x25, 0x1e, 0x54, 0x67, 0x2a, 0x5b, 0xc1,
		0xdb, 0x84, 0xc5, 0x0d, 0x43, 0x7e, 0x8c, 0x70};
	const char *udhr_path;

	test_begin("fts filter normalizer French UDHR");

	udhr_path = t_strconcat(UDHRDIR, UDHR_FRA_NAME, NULL);
	test_assert(fts_filter_create(fts_filter_normalizer_icu, NULL, NULL, settings, &norm, &error) == 0);
	input = fopen(udhr_path, "r");
	test_assert(input != NULL);
	sha512_init(&ctx);
	while (NULL != fgets(buf, sizeof(buf), input)) {
		tokens = buf;
		if (fts_filter_filter(norm, &tokens, &error) != 1){
			break;
		}
		sha512_loop(&ctx, tokens, strlen(tokens));
	}
	fclose(input);
	sha512_result(&ctx, sha512_digest);
	test_assert(memcmp(sha512_digest, correct_digest,
			   sizeof(sha512_digest)) == 0);
	fts_filter_unref(&norm);
	test_assert(norm == NULL);
	test_end();
}

static void test_fts_filter_normalizer_empty(void)
{
	/* test just a couple of these */
	static const char *empty_tokens[] = {
		"\xC2\xAF", /* U+00AF */
		"\xCC\x80", /* U+0300 */
		"\xF3\xA0\x87\xAF", /* U+E01EF */
		"\xCC\x80\xF3\xA0\x87\xAF" /* U+0300 U+E01EF */
	};
	const char * const settings[] =
		{"id", "Any-Lower; NFKD; [: Nonspacing Mark :] Remove; [\\x20] Remove", NULL};
	struct fts_filter *norm;
	const char *error;
	unsigned int i;

	test_begin("fts filter normalizer empty tokens");
	test_assert(fts_filter_create(fts_filter_normalizer_icu, NULL, NULL, settings, &norm, &error) == 0);
	for (i = 0; i < N_ELEMENTS(empty_tokens); i++) {
		const char *token = empty_tokens[i];
		test_assert_idx(fts_filter_filter(norm, &token, &error) == 0, i);
	}
	fts_filter_unref(&norm);
	test_end();
}

static void test_fts_filter_normalizer_baddata(void)
{
	const char * const settings[] =
		{"id", "Any-Lower; NFKD; [: Nonspacing Mark :] Remove", NULL};
	struct fts_filter *norm;
	const char *token, *error;
	string_t *str;
	unsigned int i;

	test_begin("fts filter normalizer bad data");

	test_assert(fts_filter_create(fts_filter_normalizer_icu, NULL, NULL, settings, &norm, &error) == 0);
	str = t_str_new(128);
	for (i = 1; i < 0x1ffff; i++) {
		str_truncate(str, 0);
		uni_ucs4_to_utf8_c(i, str);
		token = str_c(str);
		T_BEGIN {
			test_assert_idx(fts_filter_filter(norm, &token, &error) >= 0, i);
		} T_END;
	}

	str_truncate(str, 0);
	uni_ucs4_to_utf8_c(0x7fffffff, str);
	token = str_c(str);
	test_assert(fts_filter_filter(norm, &token, &error) >= 0);

	fts_filter_unref(&norm);
	test_end();
}

static void test_fts_filter_normalizer_invalid_id(void)
{
	struct fts_filter *norm = NULL;
	const char *settings[] =
		{"id", "Any-One-Out-There; DKFN; [: Nonspacing Mark :] Remove",
		 NULL};
	const char *error = NULL, *token = "foo";

	test_begin("fts filter normalizer invalid id");
	test_assert(fts_filter_create(fts_filter_normalizer_icu, NULL, NULL, settings, &norm, &error) == 0);
	test_assert(error == NULL);
	test_assert(fts_filter_filter(norm, &token, &error) < 0 && error != NULL);
	fts_filter_unref(&norm);
	test_end();
}

#ifdef HAVE_FTS_STEMMER
static void test_fts_filter_normalizer_stopwords_stemmer_eng(void)
{
	int ret;
	struct fts_filter *normalizer;
	struct fts_filter *stemmer;
	struct fts_filter *filter;
	const char *error;
	const char * const id_settings[] =
		//{"id", "Any-Lower; NFKD; [: Nonspacing Mark :] Remove; NFC", NULL};
		{"id", "Lower", NULL};
	const char *token = NULL;
	const char * const tokens[] = {
		"dries" ,"friendlies", "All", "human", "beings", "are",
		"born", "free", "and", "equal", "in", "dignity", "and",
		"rights", "They", "are", "endowed", "with", "reason", "and",
		"conscience", "and", "should", "act", "towards", "one",
		"another", "in", "a", "spirit", "of", "brotherhood", "ABCFoo",
		NULL};
	const char * const bases[] = {
		"dri" ,"friend", "all", "human", "be", NULL, "born", "free",
		NULL, "equal", NULL, "digniti", NULL, "right", NULL, NULL,
		"endow", NULL, "reason", NULL, "conscienc", NULL, "should",
		"act", "toward", "one", "anoth", NULL, NULL, "spirit", NULL,
		"brotherhood", "abcfoo", NULL};
	const char * const *tpp;
	const char * const *bpp;

	test_begin("fts filters normalizer, stopwords and stemming chained, English");

	test_assert(fts_filter_create(fts_filter_normalizer_icu, NULL, NULL, id_settings, &normalizer, &error) == 0);
	test_assert(fts_filter_create(fts_filter_stopwords, normalizer, &english_language, stopword_settings, &filter, &error) == 0);
	test_assert(fts_filter_create(fts_filter_stemmer_snowball, filter, &english_language, NULL, &stemmer, &error) == 0);

	bpp = bases;
	for (tpp = tokens; *tpp != NULL; tpp++) {
		token = *tpp;
		ret = fts_filter_filter(stemmer, &token, &error);
		if (ret <= 0) {
			test_assert(ret == 0);
			test_assert(*bpp == NULL);
		} else {
			test_assert(*bpp != NULL);
			test_assert(strcmp(*bpp, token)  == 0);
		}
		bpp++;
	}
	fts_filter_unref(&stemmer);
	fts_filter_unref(&filter);
	fts_filter_unref(&normalizer);
	test_assert(stemmer == NULL);
	test_assert(filter == NULL);
	test_assert(normalizer == NULL);
	test_end();
}

static void test_fts_filter_stopwords_normalizer_stemmer_no(void)
{
	int ret;
	struct fts_filter *normalizer;
	struct fts_filter *stemmer;
	struct fts_filter *filter;
	const char *error;
	const char *token = NULL;
	const char * const tokens[] = {
		/* Nynorsk*/
		"Alle", "har", "plikter", "andsynes", "samfunnet", "d\xC3\xA5",
		"personlegdomen", "til", "den", "einskilde", "einast", "der",
		"kan", "f\xC3\xA5", "frie", "og", "fullgode",
		"voksterk\xC3\xA5r",
		/* Bokmal */
		"Alle", "mennesker", "er", "f\xC3\xB8""dt", "frie", "og", "med",
		"samme", "menneskeverd", "og", "menneskerettigheter", "De",
		"er", "utstyrt", "med", "fornuft", "og", "samvittighet",
		"og", "b\xC3\xB8r", "handle", "mot", "hverandre", "i",
		"brorskapets", "\xC3\xA5nd", NULL};

	const char * const bases[] = {
		/* Nynorsk*/
		"all", NULL, "plikt", "andsyn", "samfunn", NULL,
		"personlegdom", NULL, NULL, "einskild", "ein", NULL, NULL,
		"fa", "frie", NULL, "fullgod", "voksterk",
		/* Bokmal */
		"all", "mennesk", NULL, "f\xC3\xB8""dt", "frie", NULL, NULL,
		NULL, "menneskeverd", NULL, "menneskerett", "de", NULL,
		"utstyrt", NULL, "fornuft", NULL, "samvitt", NULL, "b\xC3\xB8r",
		"handl", NULL, "hverandr", NULL, "brorskap", "and", NULL};
	const char * const *tpp;
	const char * const *bpp;

	test_begin("fts filters with stopwords, default normalizer and stemming chained, Norwegian");

	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &norwegian_language, stopword_settings, &filter, &error) == 0);
	test_assert(fts_filter_create(fts_filter_normalizer_icu, filter, NULL, NULL, &normalizer, &error) == 0);
	test_assert(fts_filter_create(fts_filter_stemmer_snowball, normalizer, &norwegian_language, NULL, &stemmer, &error) == 0);

	bpp = bases;
	for (tpp = tokens; *tpp != NULL; tpp++) {
		token = *tpp;
		ret = fts_filter_filter(stemmer, &token, &error);
		if (ret <= 0) {
			test_assert(ret == 0);
			test_assert(*bpp == NULL);
		} else {
			test_assert(*bpp != NULL);
			test_assert(null_strcmp(*bpp, token) == 0);
		}
		bpp++;
	}
	fts_filter_unref(&stemmer);
	fts_filter_unref(&normalizer);
	fts_filter_unref(&filter);
	test_assert(stemmer == NULL);
	test_assert(filter == NULL);
	test_assert(normalizer == NULL);
	test_end();
}

static void test_fts_filter_stopwords_normalizer_stemmer_sv(void)
{
	int ret;
	struct fts_filter *normalizer;
	struct fts_filter *stemmer;
	struct fts_filter *filter;
	const char *error;
	const char *token = NULL;
	const char * const tokens[] = {
		"Enär", "erkännandet", "av", "det", "inneboende", "värdet",
		"hos", "alla", "medlemmar", "av", "människosläktet", "och",
		"av", "deras", "lika", "och", "oförytterliga", "rättigheter",
		"är", "grundvalen", "för", "frihet", "rättvisa", "och", "fred",
		"i", "världen",	NULL};
	const char * const bases[] = {
		"enar", "erkan", NULL, NULL, "inneboend", "vardet", "hos", NULL,
		"medlemm", NULL, "manniskoslaktet", NULL, NULL, NULL, "lik",
		NULL, "oforytter", "ratt", NULL, "grundval", NULL, "frihet",
		"rattvis", NULL, "fred", NULL, "varld", NULL};
	const char * const *tpp;
	const char * const *bpp;

	test_begin("fts filters with stopwords, default normalizer and stemming chained, Swedish");


	test_assert(fts_filter_create(fts_filter_stopwords, NULL, &swedish_language, stopword_settings, &filter, &error) == 0);
	test_assert(fts_filter_create(fts_filter_normalizer_icu, filter, NULL, NULL, &normalizer, &error) == 0);
	test_assert(fts_filter_create(fts_filter_stemmer_snowball, normalizer, &swedish_language, NULL, &stemmer, &error) == 0);

	bpp = bases;
	for (tpp = tokens; *tpp != NULL; tpp++) {
		token = *tpp;
		ret = fts_filter_filter(stemmer, &token, &error);
		if (ret <= 0) {
			test_assert(ret == 0);
			test_assert(*bpp == NULL);
		} else {
			test_assert(*bpp != NULL);
			test_assert(null_strcmp(*bpp, token) == 0);
		}
		bpp++;
	}
	fts_filter_unref(&stemmer);
	fts_filter_unref(&normalizer);
	fts_filter_unref(&filter);
	test_assert(stemmer == NULL);
	test_assert(filter == NULL);
	test_assert(normalizer == NULL);
	test_end();
}
#endif
#endif

static void test_fts_filter_english_possessive(void)
{
	struct fts_filter *norm = NULL;
	const char *input[] = {
		"foo'",

		"foo's",
		"foo\xC3\xA4's",
		"foo'S",
		"foos'S",
		"foo's's",
		"foo'ss",

		"foo\xE2\x80\x99s",
		"foo\xC3\xA4\xE2\x80\x99s",
		"foo\xE2\x80\x99S",
		"foos\xE2\x80\x99S",
		"foo\xE2\x80\x99s\xE2\x80\x99s",
		"foo\xE2\x80\x99ss"
	};
	const char *expected_output[] = {
		"foo'",

		"foo",
		"foo\xC3\xA4",
		"foo",
		"foos",
		"foo's",
		"foo'ss",

		"foo",
		"foo\xC3\xA4",
		"foo",
		"foos",
		"foo\xE2\x80\x99s",
		"foo\xE2\x80\x99ss"
	};
	const char *error = NULL;
	const char *token = NULL;
	unsigned int i;

	test_begin("fts filter english possessive");

	test_assert(fts_filter_create(fts_filter_english_possessive, NULL, NULL, NULL, &norm, &error) == 0);
	for (i = 0; i < N_ELEMENTS(input); i++) {
		token = input[i];
		test_assert_idx(fts_filter_filter(norm, &token, &error) == 1, i);
		test_assert_idx(null_strcmp(token, expected_output[i]) == 0, i);
	}
	fts_filter_unref(&norm);
	test_assert(norm == NULL);
	test_end();
}

/* TODO: Functions to test 1. ref-unref pairs 2. multiple registers +
  an unregister + find */

int main(void)
{
	static void (*test_functions[])(void) = {
		test_fts_filter_find,
		test_fts_filter_contractions_fail,
		test_fts_filter_contractions_fr,
		test_fts_filter_lowercase,
#ifdef HAVE_LIBICU
		test_fts_filter_lowercase_utf8,
#endif
		test_fts_filter_stopwords_eng,
		test_fts_filter_stopwords_fin,
		test_fts_filter_stopwords_fra,
		test_fts_filter_stopwords_no,
		test_fts_filter_stopwords_fail_lazy_init,
		test_fts_filter_stopwords_malformed,
#ifdef HAVE_FTS_STEMMER
		test_fts_filter_stemmer_snowball_stem_english,
		test_fts_filter_stemmer_snowball_stem_french,
		test_fts_filter_stopwords_stemmer_eng,
#endif
#ifdef HAVE_LIBICU
		test_fts_filter_normalizer_swedish_short,
		test_fts_filter_normalizer_swedish_short_default_id,
		test_fts_filter_normalizer_french,
		test_fts_filter_normalizer_empty,
		test_fts_filter_normalizer_baddata,
		test_fts_filter_normalizer_invalid_id,
#ifdef HAVE_FTS_STEMMER
		test_fts_filter_normalizer_stopwords_stemmer_eng,
		test_fts_filter_stopwords_normalizer_stemmer_no,
		test_fts_filter_stopwords_normalizer_stemmer_sv,
#endif
#endif
		test_fts_filter_english_possessive,
		NULL
	};
	int ret;

	fts_filters_init();
	ret = test_run(test_functions);
	fts_filters_deinit();
	return ret;
}
