/* image-type.c
 *
 * Copyright (C) 1998, 2000, 2004, 2006, 2011 Free Software Foundation, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation; either version 3, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this software; see the file COPYING.LESSER.  If
 * not, write to the Free Software Foundation, Inc., 51 Franklin
 * Street, Fifth Floor, Boston, MA 02110-1301 USA
 */

#include <stdlib.h>
#include <libguile.h>
#include <pcre.h>

#if !defined(ARRAY_SIZE)
#define ARRAY_SIZE(x) (sizeof(x) / sizeof(x[0]))
#endif

static scm_t_bits pcre_tag;

struct guile_pcre
{
    pcre *regexp;
    SCM  pattern;
};

static SCM make_pcre(SCM pattern, SCM options)
{
    SCM smob;
    struct guile_pcre *regexp;
    int flags = 0;

    while (!scm_is_null(options)) {
	flags |= scm_to_int(scm_car(options));
	options = scm_cdr(options);
    }

    regexp = (struct guile_pcre *) scm_gc_malloc(sizeof(*regexp), "pcre");

    if (regexp) {
	int  error_code = 0;
	const char *error_ptr = NULL;
	int  error_offset = 0;

	regexp->pattern = pattern;
	regexp->regexp = pcre_compile2(scm_to_locale_string(pattern), flags,
				       &error_code, &error_ptr,
				       &error_offset, NULL);
	if (regexp->regexp == NULL) {
	    SCM args;

	    args = scm_cons(scm_from_latin1_string(error_ptr),
			    scm_cons(scm_from_int(error_offset), SCM_EOL));
	    scm_error(scm_from_latin1_symbol("make-pcre-error"),
		      "make-pcre", "~S: offset ~S", args, SCM_BOOL_F);
	}
    }

    SCM_NEWSMOB(smob, pcre_tag, regexp);
    return smob;
}

static SCM pcre_error_to_string(int rc)
{
    struct error_xlate
    {
	int error;
	char *name;
    };
    static struct error_xlate error_table[] = {
	{ PCRE_ERROR_NOMATCH, "PCRE_ERROR_NOMATCH" },
	{ PCRE_ERROR_NULL, "PCRE_ERROR_NULL" },
	{ PCRE_ERROR_BADOPTION, "PCRE_ERROR_BADOPTION" },
	{ PCRE_ERROR_BADMAGIC, "PCRE_ERROR_BADMAGIC" },
	{ PCRE_ERROR_UNKNOWN_OPCODE, "PCRE_ERROR_UNKNOWN_OPCODE" },
	{ PCRE_ERROR_UNKNOWN_NODE, "PCRE_ERROR_UNKNOWN_NODE" },
	{ PCRE_ERROR_NOMEMORY, "PCRE_ERROR_NOMEMORY" },
	{ PCRE_ERROR_NOSUBSTRING, "PCRE_ERROR_NOSUBSTRING" },
	{ PCRE_ERROR_MATCHLIMIT, "PCRE_ERROR_MATCHLIMIT" },
	{ PCRE_ERROR_CALLOUT, "PCRE_ERROR_CALLOUT" },
	{ PCRE_ERROR_BADUTF8, "PCRE_ERROR_BADUTF8" },
	{ PCRE_ERROR_BADUTF8_OFFSET, "PCRE_ERROR_BADUTF8_OFFSET" },
	{ PCRE_ERROR_PARTIAL, "PCRE_ERROR_PARTIAL" },
	{ PCRE_ERROR_BADPARTIAL, "PCRE_ERROR_BADPARTIAL" },
	{ PCRE_ERROR_INTERNAL, "PCRE_ERROR_INTERNAL" },
	{ PCRE_ERROR_BADCOUNT, "PCRE_ERROR_BADCOUNT" },
	{ PCRE_ERROR_DFA_UITEM, "PCRE_ERROR_DFA_UITEM" },
	{ PCRE_ERROR_DFA_UCOND, "PCRE_ERROR_DFA_UCOND" },
	{ PCRE_ERROR_DFA_UMLIMIT, "PCRE_ERROR_DFA_UMLIMIT" },
	{ PCRE_ERROR_DFA_WSSIZE, "PCRE_ERROR_DFA_WSSIZE" },
	{ PCRE_ERROR_DFA_RECURSE, "PCRE_ERROR_DFA_RECURSE" },
	{ PCRE_ERROR_RECURSIONLIMIT, "PCRE_ERROR_RECURSIONLIMIT" },
	{ PCRE_ERROR_NULLWSLIMIT, "PCRE_ERROR_NULLWSLIMIT" },
	{ PCRE_ERROR_BADNEWLINE, "PCRE_ERROR_BADNEWLINE" },
	{ PCRE_ERROR_BADOFFSET, "PCRE_ERROR_BADOFFSET" },
	{ PCRE_ERROR_SHORTUTF8, "PCRE_ERROR_SHORTUTF8" }
    };
    char error_buffer[64];
    size_t i;

    for (i = 0; i < ARRAY_SIZE(error_table); ++i)
	if (error_table[i].error == rc)
	    return scm_from_locale_string(error_table[i].name);
    snprintf(error_buffer, sizeof(error_buffer), "Unknown pcre error: %d",
	     rc);
    return scm_from_locale_string(error_buffer);
}

static SCM pcre_execute(SCM pcre_smob, SCM string)
{
    struct guile_pcre *regexp;
    SCM rv = SCM_BOOL_F;
    int rc;
    int capture_count = 0;
    int *captures = NULL;

    scm_assert_smob_type(pcre_tag, pcre_smob);

    regexp = (struct guile_pcre *) SCM_SMOB_DATA(pcre_smob);
    pcre_fullinfo(regexp->regexp, NULL, PCRE_INFO_CAPTURECOUNT, &capture_count);
    capture_count += 3;

    if (capture_count)
	captures = alloca(capture_count * 3 * sizeof(*captures));
    memset(captures, 0, 3 * sizeof(*captures));

    rc = pcre_exec(regexp->regexp, NULL, scm_to_locale_string(string),
		   scm_c_string_length(string), 0, 0, captures,
		   capture_count);
    if (rc < 0 && rc != PCRE_ERROR_NOMATCH) {
	scm_error_scm(scm_from_latin1_symbol("pcre-error"),
		      scm_from_latin1_string("pcre-exec"),
		      pcre_error_to_string(rc), SCM_EOL, SCM_BOOL_F);
    } else if (rc > 0) {
	int i;

	rv = scm_c_make_vector(rc + 1, SCM_UNSPECIFIED);
	scm_c_vector_set_x(rv, 0, string);
	for (i = 0; i < rc; ++i) {
	    SCM start = scm_from_signed_integer(captures[i * 2]);
	    SCM end = scm_from_signed_integer(captures[i * 2 + 1]);
	    scm_c_vector_set_x(rv, i + 1, scm_cons(start, end));
	}
    }

    scm_remember_upto_here_1(pcre_smob);

    return rv;
}

static int print_pcre(SCM pcre_smob, SCM port, scm_print_state *pstate)
{
    struct guile_pcre *regexp = (struct guile_pcre *) SCM_SMOB_DATA(pcre_smob);

    (void) pstate;
    scm_puts("#<pcre ", port);
    scm_display(regexp->pattern, port);
    scm_puts(">", port);

	    /* non-zero means success */
    return 1;
}

static SCM mark_pcre(SCM pcre_smob)
{
    struct guile_pcre *regexp = (struct guile_pcre *) SCM_SMOB_DATA(pcre_smob);

    scm_gc_mark(regexp->pattern);
    return SCM_BOOL_F;
}

static size_t free_pcre(SCM pcre_smob)
{
    struct guile_pcre *regexp = (struct guile_pcre *) SCM_SMOB_DATA(pcre_smob);

    pcre_free(regexp->regexp);
    regexp->regexp = NULL;
    regexp->pattern = NULL;
    scm_gc_free(regexp, sizeof(*regexp), "pcre");
    return 0;
}

void init_pcre(void)
{
    struct pcre_flags
    {
	char *name;
	int value;
    } flag_table [] = {
	{ "PCRE_CASELESS", PCRE_CASELESS },
	{ "PCRE_MULTILINE", PCRE_MULTILINE },
	{ "PCRE_DOTALL", PCRE_DOTALL },
	{ "PCRE_EXTENDED", PCRE_EXTENDED },
	{ "PCRE_ANCHORED", PCRE_ANCHORED },
	{ "PCRE_DOLLAR_ENDONLY", PCRE_DOLLAR_ENDONLY },
	{ "PCRE_EXTRA", PCRE_EXTRA },
	{ "PCRE_NOTBOL", PCRE_NOTBOL },
	{ "PCRE_NOTEOL", PCRE_NOTEOL },
	{ "PCRE_UNGREEDY", PCRE_UNGREEDY },
	{ "PCRE_NOTEMPTY", PCRE_NOTEMPTY },
	{ "PCRE_UTF8", PCRE_UTF8 },
	{ "PCRE_UTF16", PCRE_UTF16 },
	{ "PCRE_UTF32", PCRE_UTF32 },
	{ "PCRE_NO_AUTO_CAPTURE", PCRE_NO_AUTO_CAPTURE },
	{ "PCRE_NO_UTF8_CHECK", PCRE_NO_UTF8_CHECK },
	{ "PCRE_NO_UTF16_CHECK", PCRE_NO_UTF16_CHECK },
	{ "PCRE_NO_UTF32_CHECK", PCRE_NO_UTF32_CHECK },
	{ "PCRE_AUTO_CALLOUT", PCRE_AUTO_CALLOUT },
	{ "PCRE_PARTIAL_SOFT", PCRE_PARTIAL_SOFT },
	{ "PCRE_PARTIAL", PCRE_PARTIAL },
	{ "PCRE_DFA_SHORTEST", PCRE_DFA_SHORTEST },
	{ "PCRE_DFA_RESTART", PCRE_DFA_RESTART },
	{ "PCRE_FIRSTLINE", PCRE_FIRSTLINE },
	{ "PCRE_DUPNAMES", PCRE_DUPNAMES },
	{ "PCRE_NEWLINE_CR", PCRE_NEWLINE_CR },
	{ "PCRE_NEWLINE_LF", PCRE_NEWLINE_LF },
	{ "PCRE_NEWLINE_CRLF", PCRE_NEWLINE_CRLF },
	{ "PCRE_NEWLINE_ANY", PCRE_NEWLINE_ANY },
	{ "PCRE_NEWLINE_ANYCRLF", PCRE_NEWLINE_ANYCRLF },
	{ "PCRE_BSR_ANYCRLF", PCRE_BSR_ANYCRLF },
	{ "PCRE_BSR_UNICODE", PCRE_BSR_UNICODE },
	{ "PCRE_JAVASCRIPT_COMPAT", PCRE_JAVASCRIPT_COMPAT },
	{ "PCRE_NO_START_OPTIMIZE", PCRE_NO_START_OPTIMIZE },
	{ "PCRE_NO_START_OPTIMISE", PCRE_NO_START_OPTIMISE },
	{ "PCRE_PARTIAL_HARD", PCRE_PARTIAL_HARD },
	{ "PCRE_NOTEMPTY_ATSTART", PCRE_NOTEMPTY_ATSTART },
	{ "PCRE_UCP", PCRE_UCP },
    };
    size_t i;

    pcre_tag = scm_make_smob_type ("pcre", sizeof(struct guile_pcre));
    scm_set_smob_print(pcre_tag, print_pcre);
    scm_set_smob_mark(pcre_tag, mark_pcre);
    scm_set_smob_free(pcre_tag, free_pcre);
    scm_c_define_gsubr("make-pcre", 1, 0, 1, make_pcre);
    scm_c_define_gsubr("pcre-exec", 2, 0, 0, pcre_execute);
    for (i = 0; i < ARRAY_SIZE(flag_table); ++i) {
	scm_c_define(flag_table[i].name, scm_from_int(flag_table[i].value));
	scm_c_export(flag_table[i].name, NULL);
    }
    pcre_malloc = scm_malloc;		/* No corresponding scm_free(). */
}
