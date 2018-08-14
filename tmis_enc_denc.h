/**
* @file       tmis_enc_denc.c
* @brief      tmis涉及到的加密解密库
* @details    tmis涉及到的加密解密库，用于安全通信的，主要有对称加密AES、SHA1以及md5
* @author     项斌
* @date       2018/08/09
* @version    1.0
*/

#ifndef __TMIS_ENC_DEC_H__
#define __TMIS_ENC_DEC_H__


#ifdef  __cplusplus
extern "C" {
#endif



/**
 * @brief aes加密--aes128
 * @param in  传入参数，带加密的字符串
 * @param key 传入参数，密钥
 * @param out 传出参数，加密后的结果
 * @return    返回0，表示加密成功；否则，加密失败
 */
int aes_encrypt(const unsigned char* in, const unsigned char* key, unsigned char* out);

/**
 * @brief 将时间戳转化为字符串
 * @param t 时间戳
 * @param str_time 字符串数组
 * @param str_len 字符串数组长度
 * @return  成功返回0；失败返回-1
 */
int time2string(time_t t, char *str_time,int str_len);

/**
 * @brief 将字符串转化为时间戳
 * @param str_time 字符串数组
 * @param out 时间戳
 * @return  成功返回0；失败返回-1
 */

int string2time(char *str_time,time_t *out);


/**
 * @brief aes加密--aes128
 * @param in  传入参数，带解密的字符串
 * @param key 传入参数，密钥(128bits)
 * @param out 传出参数，解密后的结果
 * @return    返回0，表示解密成功；否则，解密失败
 */
int aes_decrypt(const unsigned char* in, const unsigned char* key, unsigned char* out);



/**
 * @brief md5生成报文（结果是128bits,也就是16bytes）
 * @param in  传入参数，生成报文摘要的字符串
 * @param out 传出参数，结果(传入之前先初始化好)
 * @return    返回0，表示成功；否则，失败
 */
int md5(const  unsigned char* in, unsigned char* out);



/**
 * @brief sha1生成报文（结果是128bits,也就是16bytes）
 * @param in  传入参数，生成报文摘要的字符串
 * @param out 传出参数，结果(传入之前先初始化好)
 * @return    返回0，表示成功；否则，失败
 */
int sha1(const  unsigned char* in, unsigned char* out);



/**
 * @brief 将字符数组转化成16进制输出
 * @param in  传入参数，字符数组
 * @param len 字符数组长度
 * @return    返回0，表示成功；否则，失败
 */
int printhex(const unsigned char* in, const int len);


/**
 * @brief 将字节流转化成16进制输出到数组中
 * @param in  传入参数，字符数组
 * @param out 传出的字符数组
 * @param len 输入字符数组长度
 * @return    返回0，表示成功；否则，失败
 */
int bytes2hex(const unsigned char* in, const int len, char *out);



/**
 * @brief 将16进制转化成字节流
 * @param in  传入参数，字符数组
 * @param out 传出的字符数组
 * @param len 输入字符数组长度
 * @return    返回0，表示成功；否则，失败
 */
int hex2bytes(const char* in, int len,unsigned char* out);

/**
 * @brief 获取p指向的数组的长度
 * @param p  传入参数，字符数组
 * @return    返回>=0，数组长度；-1，失败
 * @note   p指向的数组以\0\0\0结尾
 */
int get_length(const unsigned char *p);




#ifdef  __cplusplus
}
#endif



#endif










