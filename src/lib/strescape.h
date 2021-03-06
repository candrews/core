#ifndef STRESCAPE_H
#define STRESCAPE_H

#define IS_ESCAPED_CHAR(c) ((c) == '"' || (c) == '\\' || (c) == '\'')

/* escape all '\', '"' and "'" characters */
const char *str_escape(const char *str);

/* remove all '\' characters, append to given string */
void str_append_unescaped(string_t *dest, const void *src, size_t src_size);

/* remove all '\' characters */
char *str_unescape(char *str);

/* Remove all '\' chars from str until '"' is reached and return the unescaped
   string. *str is updated to point to the character after the '"'. Returns 0
   if ok, -1 if '"' wasn't found. */
int str_unescape_next(const char **str, const char **unescaped_r);

/* For Dovecot's internal protocols: Escape \001, \t, \r and \n characters
   using \001. */
const char *str_tabescape(const char *str);
void str_append_tabescaped(string_t *dest, const char *src);
void str_append_tabunescaped(string_t *dest, const void *src, size_t src_size);
char *str_tabunescape(char *str);
const char *t_str_tabunescape(const char *str);

char **p_strsplit_tabescaped(pool_t pool, const char *str);
const char *const *t_strsplit_tabescaped(const char *str);

#endif
