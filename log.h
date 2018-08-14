/**
* @file       log.h
* @brief      打印输出
* @details    将结果打印输出到文件或者标准设备
* @author     项斌
* @date       2018/08/05
* @version    1.0
*/
#ifndef __LOG_H__
#define __LOG_H__

/**
 * @brief 输出结果到标准设备
 * @param _Format 类似printf的格式字符串
 */
void std_print(const char *  _Format, ...);

/**
 * @brief 打开指定的文件
 * @param filename 文件的路基以及文件的名字
 * @return    成功，返回文件的句柄；失败，返回0
 */
FILE* open_log(const char *filename);

/**
 * @brief 输出结果到文件句柄
 * @param fp 文件的句柄
 * @return 成功返回0，失败返回-1
 */
int write_log(FILE *fp,const char *fmt,...);

/**
 * @brief 输出结果到文件句柄
 * @param fp 文件的句柄
 * @return 成功返回0，失败返回-1
 */
int write_log1(FILE *fp,const char *s,...);


/**
 * @brief 关闭文件句柄
 * @param fp 文件的句柄
 * @return 成功返回0，失败返回EOF(-1)
 */
int close_log(FILE *fp);


#endif
