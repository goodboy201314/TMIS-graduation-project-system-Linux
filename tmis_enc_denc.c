/**
* @file       tmis_enc_denc.c
* @brief      tmis涉及到的加密解密库
* @details    tmis涉及到的加密解密库，用于安全通信的，主要有对称加密AES、SHA1以及md5
* @author     项斌
* @date       2018/08/09
* @version    1.0
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <openssl/aes.h>
#include <openssl/md5.h>
#include <openssl/sha.h>
#include <time.h>
#include "tmis_enc_denc.h"

/**
 * @brief aes加密--aes128
 * @param in  传入参数，带加密的字符串
 * @param key 传入参数，密钥
 * @param out 传出参数，加密后的结果
 * @return    返回0，表示加密成功；否则，加密失败
 */
int aes_encrypt(const unsigned char* in, const unsigned char* key, unsigned char* out)
{
	if (!in || !key || !out) return -1;  

	int i;
	unsigned char iv[AES_BLOCK_SIZE]; //加密的初始化向量
	for (i = 0; i < AES_BLOCK_SIZE; ++i) //iv一般设置为全0,可以设置其他，但是加密解密要一样就行
		iv[i] = 0;

	AES_KEY aes;
	if (AES_set_encrypt_key(key, 128, &aes) < 0)
	{
		return 0;
	}
	int len = strlen((char *)in); //这里的长度是char*in的长度，但是如果in中间包含'\0'字符的话

	//那么就只会加密前面'\0'前面的一段，所以，这个len可以作为参数传进来，记录in的长度

	//至于解密也是一个道理，光以'\0'来判断字符串长度，确有不妥，后面都是一个道理。
	AES_cbc_encrypt(in, out, len, &aes, iv,AES_ENCRYPT);
	//AES_ecb_encrypt(in,out,&aes,AES_ENCRYPT);

			
	return 0;
}


/**
 * @brief 获取p指向的数组的长度
 * @param p  传入参数，字符数组
 * @return    返回>=0，数组长度；-1，失败
 * @note   p指向的数组以\0\0\0结尾
 */
int get_length(const unsigned char *p)
{
	if(!p) return -1;
	int len=0;
	while(1)
	{
		if(p[len]==0 && p[len+1]==0 && p[len+2]==0)  return len;

		len++;
	}
	
	return -1;
}

/**
 * @brief 将时间戳转化为字符串
 * @param t 时间戳
 * @param str_time 字符串数组
 * @param str_len 字符串数组长度
 * @return  成功返回0；失败返回-1
 */
int time2string(time_t t, char *str_time,int str_len)
{
	if(!str_time) return -1;
	
    struct tm *tm_t;
    tm_t = localtime(&t);
    strftime(str_time,str_len,"%Y%m%d%H%M%S",tm_t);

    return 0;
}

/**
 * @brief 将字符串转化为时间戳
 * @param str_time 字符串数组
 * @param out 时间戳
 * @return  成功返回0；失败返回-1
 */

int string2time(char *str_time,time_t *out)
{
	if(!str_time || !out) return -1;

	struct tm stm;  
  	strptime(str_time, "%Y%m%d%H%M%S",&stm); 
	*out= mktime(&stm);

    return 0;
}


/**
 * @brief aes加密--aes128
 * @param in  传入参数，带解密的字符串
 * @param key 传入参数，密钥(128bits)
 * @param out 传出参数，解密后的结果
 * @return    返回0，表示解密成功；否则，解密失败
 */
int aes_decrypt(const unsigned char* in, const unsigned char* key, unsigned char* out)
{
	if (!in || !key || !out) return -1;

	int i;
	unsigned char iv[AES_BLOCK_SIZE]; //加密的初始化向量
	for (i = 0; i < AES_BLOCK_SIZE; ++i) //iv一般设置为全0,可以设置其他，但是加密解密要一样就行
		iv[i] = 0;

	
	AES_KEY aes;
	if (AES_set_decrypt_key(key, 128, &aes) < 0)
	{
		return 0;
	}
	int len = get_length(in);//strlen((char *)in);
	//void AES_ecb_encrypt(const unsigned char *in, unsigned char *out,
//								const AES_KEY *key, const int enc);
//void AES_cbc_encrypt(const unsigned char *in, unsigned char *out,
//size_t length, const AES_KEY *key,unsigned char *ivec, const int enc); //参数相对复杂

	AES_cbc_encrypt(in, out, len, &aes, iv,AES_DECRYPT);
	//AES_ecb_encrypt(in,out,&aes,AES_DECRYPT);
	
	return 0;
}


/*******************************************************************************
打开/usr/include/openssl/md5.h这个文件我们可以看到一些函数:

1.初始化 MD5 Contex, 成功返回1,失败返回0
 int MD5_Init(MD5_CTX *c);

2.循环调用此函数,可以将不同的数据加在一起计算MD5,成功返回1,失败返回0
  int MD5_Update(MD5_CTX *c, const void *data, size_t len);

3.输出MD5结果数据,成功返回1,失败返回0
  int MD5_Final(unsigned char *md, MD5_CTX *c);
 
 =====> MD5_Init,MD5_Update,MD5_Final三个函数的组合,直接计算出MD5的值

unsigned char *MD5(const unsigned char *d, size_t n, unsigned char *md);

内部函数,不需要调用
 void MD5_Transform(MD5_CTX *c, const unsigned char *b);
******************************************************************************/

/**
 * @brief md5生成报文（结果是128bits,也就是16bytes）
 * @param in  传入参数，生成报文摘要的字符串
 * @param out 传出参数，结果(传入之前先初始化好)
 * @return    返回0，表示成功；否则，失败
 */
int md5(const  unsigned char* in, unsigned char* out)
{
	if (!in || !out) return -1;  // 参数检查

	MD5_CTX ctx;
//    unsigned char outmd[16];
//    memset(outmd,0,sizeof(outmd));
//	  memset(out,0,outlen);  
	int len = strlen((char *)in);

    MD5_Init(&ctx);	
    MD5_Update(&ctx,in,len);
    MD5_Final(out,&ctx);

	return 0;
}


/*******************************************************************************
SHA1算法是对MD5算法的升级,计算结果为20字节(160位)，使用方法如下：
打开/usr/include/openssl/sha.h这个文件我们可以看到一些函数

1.初始化 SHA Contex, 成功返回1,失败返回0
 int SHA_Init(SHA_CTX *c);

2.循环调用此函数,可以将不同的数据加在一起计算SHA1,成功返回1,失败返回0
  int SHA_Update(SHA_CTX *c, const void *data, size_t len);

3.输出SHA1结果数据,成功返回1,失败返回0
 int SHA_Final(unsigned char *md, SHA_CTX *c);

 ====> SHA_Init,SHA_Update,SHA_Final三个函数的组合,直接计算出SHA1的值

 unsigned char *SHA(const unsigned char *d, size_t n, unsigned char *md);

内部函数,不需要调用
  void SHA_Transform(SHA_CTX *c, const unsigned char *data);
 
另外：上面的SHA可以改为SHA1，SHA224，SHA256，SHA384，SHA512就可以实现多种加密了
*********************************************************************************/

/**
 * @brief sha1生成报文（结果是128bits,也就是16bytes）
 * @param in  传入参数，生成报文摘要的字符串
 * @param out 传出参数，结果(传入之前先初始化好)
 * @return    返回0，表示成功；否则，失败
 */
int sha1(const  unsigned char* in, unsigned char* out)
{
	if (!in || !out) return -1;	// 参数检查

	SHA_CTX stx;
    //unsigned char outmd[20];//注意这里的字符个数为20
       
    int len=strlen((char *)in);
   // memset(out,0,outlen));
	
    SHA1_Init(&stx);
	SHA1_Update(&stx,in,len);
    SHA1_Final(out,&stx);


	return 0;
}

/**
 * @brief 将字符数组转化成16进制输出
 * @param in  传入参数，字符数组
 * @param len 字符数组长度
 * @return    返回0，表示成功；否则，失败
 */
int printhex(const unsigned char* in, const int len)
{
	if (!in) return -1;	// 参数检查

	int i;
	for(i=0;i<len;i++)
    {
        printf("%02X",in[i]);
    }
	printf("\n");

	return 0;
}

/**
 * @brief 将字节流转化成16进制输出到数组中
 * @param in  传入参数，字符数组
 * @param out 传出的字符数组
 * @param len 输入字符数组长度
 * @return    返回0，表示成功；否则，失败
 */
int bytes2hex(const unsigned char* in, const int len, char *out)
{
	if(!in || !out) return -1;

	int i;
    unsigned char highByte, lowByte;
    for (i = 0; i < len; i++)
    {
        highByte = in[i] >> 4;
        lowByte = in[i] & 0x0f ;


        highByte += 0x30;


        if (highByte > 0x39)
                out[i * 2] = highByte + 0x07;
        else
                out[i * 2] = highByte;


        lowByte += 0x30;
        if (lowByte > 0x39)
            out[i * 2 + 1] = lowByte + 0x07;
        else
            out[i * 2 + 1] = lowByte;
    }


	return 0;
	
}


/**
 * @brief 将16进制转化成字节流
 * @param in  传入参数，字符数组
 * @param out 传出的字符数组
 * @param len 输入字符数组长度
 * @return    返回0，表示成功；否则，失败
 */
int hex2bytes(const char* in, int len,unsigned char* out)
{
	if(!in || !out) return -1;

	int i;
    unsigned char highByte, lowByte;
	//memset(out,0,outlen);
    
    for (i = 0; i < len; i += 2)
    {
        highByte = toupper(in[i]);
        lowByte  = toupper(in[i + 1]);


        if (highByte > 0x39)
            highByte -= 0x37;
        else
            highByte -= 0x30;


        if (lowByte > 0x39)
            lowByte -= 0x37;
        else
            lowByte -= 0x30;


        out[i / 2] = (highByte << 4) | lowByte;
    }
	
    return 0;
}



// 测试aes
#if 0
int main()
{
	unsigned char in[30] = "xiangbin is a good boy!";
	unsigned char key[17] = "0123456789123456";
	unsigned char out[100];
	unsigned char out2[100];
	unsigned char out3[100];
	char outStr[100]={0};

//int aes_decrypt(const char* in, const char* key, char* out)
//int aes_decrypt(const char* in, const char* key, char* out)
	printf("测试aes....\n");
	memset(out,0,sizeof(out));
	memset(out2,0,sizeof(out2));
	aes_encrypt(in,key,out);
// int bytes2hex(const unsigned char* in, const int len, char *out);
	bytes2hex(out, strlen((char *)out), outStr);	
	printf("aes加密输出1：%s\n",outStr);
	printf("aes加密输出2：");
	printhex(out,strlen((char *)out));
	
// int hex2bytes(const char* in, int len,unsigned char* out);
	memset(out3,0,sizeof(out3));
	hex2bytes(outStr,strlen(outStr),out3);

	aes_decrypt(out3,key,out2);
	printf("aes解密输出：%s\n",out2);


	return 0;
}



#endif


// 综合测试aes,sha1,md5
#if 0
int main()
{
	unsigned char in[30] = "xiangbin is a good boy!";
	unsigned char key[17] = "0123456789123456";
	unsigned char out[100];
	unsigned char out2[100];

//int aes_decrypt(const char* in, const char* key, char* out)
//int aes_decrypt(const char* in, const char* key, char* out)
	printf("测试aes....\n");
	memset(out,0,sizeof(out));
	memset(out2,0,sizeof(out2));
	aes_encrypt(in,key,out);
	
	printf("aes加密输出：");
	printhex(out,strlen((char *)out));

	aes_decrypt(out,key,out2);
	printf("aes解密输出：%s\n",out2);
	

//int md5(const unsigned char* in, unsigned char* out)
	printf("\n测试md5....\n");
	memset(out,0,sizeof(out));
	md5(in, out);

	printf("md5加密输出：");
	printhex(out,strlen((char *)out));


//int sha1(const unsigned char* in, unsigned char* out);
	printf("\n测试sha1...\n");
	memset(out,0,sizeof(out));
	sha1(in, out);

	printf("sha1加密输出：");
	printhex(out,strlen((char *)out));


	return 0;
}

/*********************************** 结果显示 ***********************************
测试aes....
aes加密输出：8334AE2794E35317F1FFBF562337A3D751F3D41394940647F8F7EF70884BB549
aes解密输出：xiangbin is a good boy!

测试md5....
md5加密输出：943995A8D4E4DD931747FE6E772D15D3

测试sha1...
sha1加密输出：922E80C855F1C0E10E3AE582AB90A123ECB18976

**********************************************************************************/

#endif









