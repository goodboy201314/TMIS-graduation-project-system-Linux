/**
* @file       log.c
* @brief      打印输出
* @details    将结果打印输出到文件或者标准设备
* @author     项斌
* @date       2018/08/05
* @version    1.0
*/

#include <stdio.h>
#include <time.h>
#include <stdarg.h>
#include <pthread.h>
#include "log.h"


pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

/**
 * @brief 输出结果到标准设备
 * @param _Format 类似printf的格式字符串
 */
void std_print(const char *  _Format, ...)
{
	printf(_Format);
	//printf("%s\n",__func__);
}

/**
 * @brief 打开指定的文件
 * @param filename 文件的路基以及文件的名字
 * @return    成功，返回文件的句柄；失败，返回NULL
 */
FILE* open_log(const char *filename)
{
	FILE *fp = NULL;

	fp = fopen(filename,"a+");
	if(NULL==fp) return NULL;
	return fp;
}

/**
 * @brief 输出结果到文件句柄
 * @param fp 文件的句柄
 * @return 成功返回0，失败返回-1
 */
int write_log(FILE *fp,const char *fmt,...)
{
	/* 参数判断 */
	if(NULL==fp) return -1;
	int ret;

	/* 获取当前的时间并且转化为字符串 */
	char nowtime[20]={0};
	time_t rawtime;
	struct tm* ltime;
	time(&rawtime);
	ltime = localtime(&rawtime);
	strftime(nowtime, 20, "%Y-%m-%d %H:%M:%S", ltime);

	pthread_mutex_lock(&mutex);
	do
	{
		/* 输出当前的时间 */
		ret = fprintf(fp,"%s: ",nowtime);
		if(ret<0) break;

		/* 输出要打印的内容*/
		va_list args;  //va_list是一个字符串指针，用于获取不确定个数的参数
		va_start(args,fmt); //读取可变参数的过程其实就是在堆栈中，使用指针，遍历堆栈段中
		//的参数列表，从低地址到高地址一个一个的把参数内容读出来的过程

		//该函数会根据参数fmt字符串来转换格式并格式化数据，然后将结果输出到参数Stream指定的文件中
		//直到出现字符串结束的\0为止。
		ret = vfprintf(fp, fmt, args);
		//获取完所有参数之后，为了避免发生程序瘫痪，需要将 ap指针关闭，其实这个函数相当于将args设置为NULL
		va_end(args);
		fflush(fp);
	}while(0);
	pthread_mutex_unlock(&mutex);

	return (ret>=0 ? 0 : -1);
}

/**
 * @brief 输出结果到文件句柄
 * @param fp 文件的句柄
 * @return 成功返回0，失败返回-1
 */
int write_log1(FILE *fp,const char *s,...)
{
	/* 参数判断 */
	if(NULL==fp) return -1;
	int ret;

	/* 获取当前的时间并且转化为字符串 */
	char nowtime[20]={0};
	time_t rawtime;
	struct tm* ltime;
	time(&rawtime);
	ltime = localtime(&rawtime);
	strftime(nowtime, 20, "%Y-%m-%d %H:%M:%S", ltime);

	do
	{
		/* 输出当前的时间 */
		ret = fprintf(fp,"%s: ",nowtime);
		if(ret<0) break;

		/* 输出要打印的内容*/
		ret = fprintf(fp,s);
		fflush(fp);
	}while(0);

	return (ret>=0 ? 0 : -1);
}

/**
 * @brief 关闭文件句柄
 * @param fp 文件的句柄
 * @return 成功返回0，失败返回EOF(-1)
 */
int close_log(FILE *fp)
{
	/* 参数判断 */
	if(NULL==fp) return -1;

	return fclose(fp);
}

#if 0
int main()
{
	//printf("%d\n",printf(""));
	FILE *fp = open_log("./a.txt");

	write_log(fp,"xiangbin is a boy, and he is %d years old! %d\n",25,100);

	close_log(fp);

	return 0;
}

#endif
