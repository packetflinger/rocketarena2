typedef struct
{
	unsigned int	state[4];
	unsigned int	count[2];
	unsigned char	buffer[64];
} MD5_CTX;

void MD5Init (MD5_CTX *context);
void MD5Update (MD5_CTX *context, unsigned char *input, unsigned int inputLen);
void MD5Final (unsigned char digest[16], MD5_CTX *context);
void MD5Print (unsigned char digest[16], char *outbuf);
void MD5Digest (unsigned char *input, unsigned int inputLen, char *outbuf);
