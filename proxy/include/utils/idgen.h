#ifndef UVA_PROXY_IDGEN_H
#define UVA_PROXY_IDGEN_H

/* Nanoid-style ID lengths matching UvA frontend format */
#define NANOID_THREAD_LEN 36
#define NANOID_MSG_LEN    16

/*
 * Generate a nanoid-style alphanumeric ID.
 * Fills buf with 'len' random characters from [a-zA-Z0-9] plus NUL.
 * buf must be at least len+1 bytes.
 */
void gen_nanoid(char *buf, int len);

#endif /* UVA_PROXY_IDGEN_H */
