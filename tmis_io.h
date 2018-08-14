/**
* @file       tmis_io.c
* @brief      tmis服务的read，write
* @details    tmis服务器端接收（read）和发送实现（write）的实现，考虑粘包问题 ==>writen,readn
* @author     项斌
* @date       2018/08/07
* @version    1.0
*/


#ifndef __TMIS_IO_H__
#define __TMIS_IO_H__



/**
 * @brief 从套接字fd中读取count字节大小数据，放入buf数组中
 * @param fd 套接字
 * @param buf 缓存数组
 * @param count 此次读取数据的大小
 * @return 读取出错，返回-1；否则，返回读取的字节大小
 */
ssize_t readn(int fd, void *buf, size_t count);


/**
 * @brief 往套接字fd中写count字节大小数据，数据来源于buf数组
 * @param fd 套接字
 * @param buf 数据来源数组
 * @param count 此次写入数据的大小
 * @return 读取出错，返回-1；否则，返回读取的字节大小
 */
ssize_t writen(int fd, const void *buf, size_t count);


#endif
