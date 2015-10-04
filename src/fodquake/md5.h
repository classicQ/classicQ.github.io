
/* Data structure for MD5 (Message-Digest) computation */
struct md5
{
	unsigned long long length;         /* number of bits handled */
	unsigned int state[4];       /* state buffer */
	unsigned int curlen;         /* current input buffer length */
	unsigned char buf[64];        /* input buffer */
};


void md5_init(struct md5 *md);
void md5_process(struct md5 *md, const unsigned char *in, unsigned long inlen);
void md5_done(struct md5 *md, unsigned char *out);

