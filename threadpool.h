/**
* @file       threadpool.c
* @brief      线程池
* @details    线程池的简单实现，其中任务队列和线程组都是用循环队列实现的
* @author     项斌
* @date       2018/08/05
* @version    1.0
*/

#ifndef __THREADPOOL_H__
#define __THREADPOOL_H__

/** 基本错误类型 */
#define ERR_BASE 8888

/** malloc申请内存错误 */
#define ERR_MALLOC ERR_BASE+1

/** 函数的传入参数错误 */
#define ERR_PARAMETER ERR_BASE+2

/** 初始化互斥琐、条件变量出错 */
#define ERR_MUTEX_COND ERR_BASE+3

/** 调试的时候使用，最后完成后会使用下面一个 */
//#define xb_printf(...) printf(__VA_ARGS__)
#define xb_printf(...)

/** 管理线程睡眠时间 */
#define DEFAULT_TIME_SPC 10

/** 默认添加线程、减少线程的数量 */
#define DEFAULT_THREAD_STEP 10


/** 任务队列中的任务相关信息 */
typedef struct threadpool_task
{
	void *(*function)(void *);         ///< 函数指针，回调函数
	void *arg;                         ///< 回调函数的参数
}threadpool_task_t;

/** 线程池相关信息 */
typedef struct threadpool
{
	pthread_mutex_t lock;				///< 本线程池的互斥锁，锁住的是整个结构体
	pthread_mutex_t busy_thr_lock;      ///< 变量busy_thr_num的锁
	pthread_cond_t queue_not_full;  	///< 任务队列不为满条件变量，如果满了，阻塞等待
	pthread_cond_t queue_not_empty;	    ///< 任务队列不为空

	/* 线程（数组）相关属性 */
	int min_thr_num;                ///< 最小线程数量
	int max_thr_num;                ///< 最大线程数量
	int live_thr_num;               ///< 活着的线程数量
	int busy_thr_num;               ///< 忙碌的线程数量，也就是正在干活的线程数量
	int wait_exit_thr_num;          ///< 等待销毁的线程

	pthread_t *threads;             ///< 线程数组（首地址）
	pthread_t manager_tid;          ///< 任务线程组管理线程，负责任务线程的增减
	threadpool_task_t *tasks;       ///< 任务队列数组

	/* 任务队列相关属性 */
	int queue_front;                ///< 任务队列的队首
	int queue_rear;					///< 任务队列的队尾
	int queue_size;					///< 当前任务队列的长度
	int queue_max_size;				///< 任务队列的最大长度

	int shutdown; 					///< 标志位，线程池使用状态  1:关闭，0：正在使用

}threadpool_t;


/////////////////////////////////  函数相关定义         //////////////////////////////////////

/**
 * @brief 线程池的创建
 * @param poll 线程池地址指针
 * @param min_thr_num 线程池中线程的最小数量
 * @param max_thr_num 线程池中线程的最大数量
 * @param queue_max_size 线程池中任务队列的最大长度
 * @return 成功，返回0；错误，返回错误代码
 */
int threadpool_create(threadpool_t **p,int min_thr_num,int max_thr_num,int queue_max_size);

/**
 * @brief 线程池活着的线程处理函数（干活的线程）
 * @param threadpool 线程池的范型指针
 * @return NULL值
 */
void *work_thread(void *threadpool);

/**
 * @brief 线程池管理线程处理函数
 * @param threadpool 线程池的范型指针
 * @return NULL值
 */
void *manage_thread(void *threadpool);

/**
 * @brief 添加任务到线程池
 * @param pool 线程池指针
 * @param function 任务回调函数
 * @param arg 任务回调函数的参数
 * @return 成功，返回0；失败，返回-1
 */
int threadpool_add_task(threadpool_t *pool, void*(*function)(void *arg), void *arg);

/**
 * @brief 线程池销毁
 * @param pool 线程池指针
 * @return 成功，返回0；失败，返回-1
 */
int threadpool_destroy(threadpool_t **p);

/**
 * @brief 释放malloc申请的内存
 * @param pool 线程池指针的指针
 * @return 成功，返回0；失败，返回-1
 */
int threadpool_free(threadpool_t **p);


/**
 * @brief 判断某一个线程是否活着
 * @param tid 被测试是否活着的线程
 * @return 返回1，表示或者；返回0，表示没有或者
 */
int is_thread_alive(pthread_t tid);



#endif
