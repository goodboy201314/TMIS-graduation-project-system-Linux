/**
* @file       threadpool.c
* @brief      线程池
* @details    线程池的简单实现，其中任务队列和线程组都是用循环队列实现的
* @author     项斌
* @date       2018/08/05
* @version    1.0
*/

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <errno.h>
#include "threadpool.h"

/////////////////////////////////    函数实现     ///////////////////////////////
/**
 * @brief 线程池的创建
 * @param poll 线程池地址指针
 * @param min_thr_num 线程池中线程的最小数量
 * @param max_thr_num 线程池中线程的最大数量
 * @param queue_max_size 线程池中任务队列的最大长度
 * @return 成功，返回0；错误，返回错误代码
 */
int threadpool_create(threadpool_t **p,int min_thr_num,int max_thr_num,int queue_max_size)
{
	threadpool_t *pool = NULL;
	int ret = 0;
	int i = 0;

	do
	{
		/** 动态申请内存，并且初始化 */
		pool = (threadpool_t *)malloc(sizeof(threadpool_t));
		if(NULL==pool)
		{
			ret = ERR_MALLOC;
			break;
		}

		/* 初始化参数 */
		pool->min_thr_num = min_thr_num;
		pool->max_thr_num = max_thr_num;
		/* 活着的线程数 初值=最小线程数 */
		pool->live_thr_num = min_thr_num;
		pool->busy_thr_num = 0;
		pool->wait_exit_thr_num = 0;

		pool->queue_front = 0;
		pool->queue_rear = 0;
		pool->queue_size = 0;
		pool->queue_max_size = queue_max_size;
		pool->shutdown = 0;   /* 不关闭线程池 */

		/* 根据最大线程上限数， 给工作线程数组开辟空间, 并清零 */
		pool->threads = (pthread_t *)malloc(sizeof(pthread_t) * max_thr_num);
		if(NULL==pool->threads)
		{
			ret = ERR_MALLOC;
			break;
		}
		memset(pool->threads,0,sizeof(pthread_t) * max_thr_num);

		/* 申请队列的空间 */
		pool->tasks = (threadpool_task_t *)malloc(sizeof(threadpool_task_t) * queue_max_size);
		if (NULL == pool->tasks) {
			ret = ERR_MALLOC;
			break;
		}

		/* 初始化互斥琐、条件变量 */
		if (pthread_mutex_init( &(pool->lock), NULL ) != 0
				|| pthread_mutex_init( &(pool->busy_thr_lock), NULL ) != 0
				|| pthread_cond_init( &(pool->queue_not_empty), NULL ) != 0
				|| pthread_cond_init( &(pool->queue_not_full), NULL ) != 0)
		{
			ret = ERR_MUTEX_COND;
			break;
		}

		/* 初始化工作线程 */
		for(i=0;i<min_thr_num;i++)
		{
            pthread_create(&(pool->threads[i]), NULL, work_thread, (void *)pool);/*pool指向当前线程池*/
            pthread_detach(pool->threads[i]);  /* 分离线程，这样避免僵尸线程 */
            xb_printf("start thread 0x%x...\n", (unsigned int)pool->threads[i]);
        }
		/* 初始化管理线程 */
        pthread_create(&(pool->manager_tid), NULL, manage_thread, (void *)pool);/* 启动管理者线程 */
        pthread_detach(pool->manager_tid);/* 分离线程，这样避免僵尸线程 */

		*p = pool;
	}while(0); //代替goto

	if(ret!=0) threadpool_free(&pool);

	return ret;
}

/**
 * @brief 线程池活着的线程处理函数（干活的线程）
 * @param threadpool 线程池的范型指针
 * @return NULL值
 */
void *work_thread(void *threadpool)
{
	threadpool_t *pool = (threadpool_t *)threadpool;
	threadpool_task_t task;

	while(1)
	{
		/*刚创建出线程，等待任务队列里有任务，否则阻塞等待任务队列里有任务后再唤醒接收任务*/
		pthread_mutex_lock(&(pool->lock));
		while( pool->queue_size==0 && !pool->shutdown )
		{
			/* 阻塞等待条件满足 */
			pthread_cond_wait( &(pool->queue_not_empty),&(pool->lock) );

			/*裁员（员工自杀方式）：   清除指定数目的空闲线程，如果要结束的线程个数大于0，结束线程*/
			if(pool->wait_exit_thr_num>0)
			{
				pool->wait_exit_thr_num--; //这时候拿着pool->lock锁呢

				/*如果线程池里活着的线程个数大于最小值时可以结束当前空闲的线程*/
				if(pool->live_thr_num > pool->min_thr_num)
				{
					pool->live_thr_num--;  // 杀死该进程
					pthread_mutex_unlock(&(pool->lock));
					xb_printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());

					pthread_exit(NULL);
				}
			}
		}// end for: while(pool->queue_size==0 && !pool->shutdown)

		/* 销毁的时候：关闭整个线程池 */
		if (pool->shutdown) {
//			pool->live_thr_num--;  // 杀死该进程
			pthread_mutex_unlock(&(pool->lock));
			xb_printf("thread 0x%x is exiting\n", (unsigned int)pthread_self());
			pthread_exit(NULL);     /* 线程自行结束 */
		}

		/*干活：从任务队列里获取任务, 是一个出队操作*/
		//task = pool->tasks[pool->queue_front];
		task.function = pool->tasks[pool->queue_front].function;
		task.arg = pool->tasks[pool->queue_front].arg;
		pool->queue_front = (pool->queue_front + 1) % pool->queue_max_size;
		pool->queue_size--;
		/*通知可以有新的任务添加进来*/
		pthread_cond_broadcast(&(pool->queue_not_full));
		/*任务取出后，立即将 线程池琐 释放*/
		pthread_mutex_unlock(&(pool->lock));


		/*执行任务*/
		xb_printf("thread 0x%x start working\n", (unsigned int)pthread_self());
		pthread_mutex_lock(&(pool->busy_thr_lock));
		pool->busy_thr_num++;
		pthread_mutex_unlock(&(pool->busy_thr_lock));

		(*(task.function))(task.arg);
		//task.function(task.arg);

		/*任务结束处理*/
		xb_printf("thread 0x%x end working\n", (unsigned int)pthread_self());
		pthread_mutex_lock(&(pool->busy_thr_lock));
		pool->busy_thr_num--;
		pthread_mutex_unlock(&(pool->busy_thr_lock));

	} // end for: while(1)

	return NULL;
}

/**
 * @brief 线程池管理线程处理函数
 * @param threadpool 线程池的范型指针
 * @return NULL值
 */
void *manage_thread(void *threadpool)
{

	threadpool_t *pool = (threadpool_t *)threadpool;
	int i = 0;
	while(!pool->shutdown)
	{
		/* 先睡眠一段时间，然后醒来处理线程的数量，人少->招人，人多->裁人 */
		sleep(DEFAULT_TIME_SPC);
		if(pool->shutdown) break;     // 在睡着的时候，被调用了destroy

		/*拿到相关的变量*/
		pthread_mutex_lock(&(pool->lock));
//		int queue_size = pool->queue_size;
		int live_thr_num = pool->live_thr_num;
		pthread_mutex_unlock(&(pool->lock));

		pthread_mutex_lock(&(pool->busy_thr_lock));
		int busy_thr_num = pool->busy_thr_num;
		pthread_mutex_unlock(&(pool->busy_thr_lock));

		/* 增加线程：如果工作的线程占到总线程的80%（并且还没有达到最大的线程数量），那么增加线程数量 */
		if( (busy_thr_num * 100 / live_thr_num > 80) && (live_thr_num < pool->max_thr_num) )
		{
			pthread_mutex_lock(&(pool->lock));  // 加锁
			int add = 0;
			// 查找一个空闲的数组地址（这个数组元素或者没有使用，或者线程被释放掉了）
			for(i=0;i<pool->max_thr_num && add < DEFAULT_THREAD_STEP
					&& live_thr_num < pool->max_thr_num ;i++)
			{
				if(pool->threads[i]==0 || !is_thread_alive(pool->threads[i]))
				{
					pthread_create(&(pool->threads[i]), NULL, work_thread, (void *)pool);/*pool指向当前线程池*/
					pthread_detach(pool->threads[i]);  /* 分离线程，这样避免僵尸线程 */
					add++;
					pool->live_thr_num++;
				}
			} // end for:  for(i=0;i<pool->max_thr_num &&...

			pthread_mutex_unlock(&(pool->lock)); // 解锁
		}

		/* 减少线程：如果有50%的线程干活，那么减少线程 */
		if( busy_thr_num * 2 < live_thr_num && live_thr_num > pool->min_thr_num)
		{
			/* 设置要释放的线程数量 */
			pthread_mutex_lock(&(pool->lock));
			pool->wait_exit_thr_num = DEFAULT_THREAD_STEP;      /* 要销毁的线程数 设置为10 */
			pthread_mutex_unlock(&(pool->lock));

			for(i=0;i<DEFAULT_THREAD_STEP;i++)
			{
				pthread_cond_signal(&(pool->queue_not_empty));
			}
		}

	} // end for： while(1)

	return NULL;
}

/**
 * @brief 添加任务到线程池
 * @param pool 线程池指针
 * @param function 任务回调函数
 * @param arg 任务回调函数的参数
 * @return 成功，返回0；失败，返回-1
 */
int threadpool_add_task(threadpool_t *pool, void*(*function)(void *arg), void *arg)
{
	pthread_mutex_lock(&(pool->lock)); // 拿到锁

	/* 队列已经满， 调wait阻塞 */
	while(pool->queue_size==pool->queue_max_size && !pool->shutdown)
	{
		pthread_cond_wait(&(pool->queue_not_full),&(pool->lock));
	}
	/* 线程池被关闭了 */
	if (pool->shutdown)
	{
		pthread_mutex_unlock(&(pool->lock));
		return -1;
	}

	/* 添加任务到队列 */
	// 首先清空
/////////////////// 具体问题，具体分析，这里传入的是int类型的fd，不是malloc的内存，不用释放
	if (pool->tasks[pool->queue_rear].arg != NULL)
	{
//		free(pool->tasks[pool->queue_rear].arg);
		pool->tasks[pool->queue_rear].arg = NULL;
	}
	pool->tasks[pool->queue_rear].function = function;
	pool->tasks[pool->queue_rear].arg = arg;
	pool->queue_rear = (pool->queue_rear + 1) % pool->queue_max_size;       /* 队尾指针移动, 模拟环形 */
	pool->queue_size++;

	/*添加完任务后，队列不为空，唤醒线程池中 等待处理任务的线程*/
	pthread_cond_signal(&(pool->queue_not_empty));
	pthread_mutex_unlock(&(pool->lock));

	return 0;

}

/**
 * @brief 线程池销毁
 * @param pool 线程池指针
 * @return 成功，返回0；失败，返回-1
 */
int threadpool_destroy(threadpool_t **p)
{
	threadpool_t *pool = NULL;
	int i;

	if(NULL==p) return -1;

	pool = *p;

	/* 设置之后，管理线程醒来后可以退出了 */
	pool->shutdown =1;
	/*先销毁管理线程*/
//	pthread_join(pool->manager_tid, NULL);


	/* 给每一个工作的线程发信号，让他们醒来，然后自杀*/
	for (i = 0; i < pool->live_thr_num; i++)
	{
		pthread_cond_broadcast(&(pool->queue_not_empty)); /*通知所有的空闲线程*/
	}
//	for (i = 0; i < pool->live_thr_num; i++)
//	{
//		pthread_join(pool->threads[i], NULL);
//	}

	/* 释放malloc申请的变量*/
	return threadpool_free(p);
}

/**
 * @brief 释放malloc申请的内存
 * @param pool 线程池指针的指针
 * @return 成功，返回0；失败，返回-1
 */
int threadpool_free(threadpool_t **p)
{
	threadpool_t *pool = NULL;
	if(NULL==p) return -1;

	pool = *p;
	if(NULL==pool) return -1;

	/* 首先,释放销毁锁和子线程 */
	if(pool->threads)
	{
		free(pool->threads);
		pthread_mutex_unlock(&(pool->lock));
		pthread_mutex_destroy(&(pool->lock));
		pthread_mutex_unlock(&(pool->busy_thr_lock));
		pthread_mutex_destroy(&(pool->busy_thr_lock));
		pthread_cond_destroy(&(pool->queue_not_empty));
		pthread_cond_destroy(&(pool->queue_not_full));
	}

	/* 接着,释放销毁锁 */
	if(pool->tasks) free(pool->tasks);

	/* 最后，销毁线程池 */
	free(pool);
	*p= NULL;

	return 0;
}


/**
 * @brief 判断某一个线程是否活着
 * @param tid 被测试是否活着的线程
 * @return 返回1，表示或者；返回0，表示没有或者
 */
int is_thread_alive(pthread_t tid)
{
	int ret = pthread_kill(tid, 0);     //发0号信号，测试线程是否存活
//    ESRCH:  No thread with the ID thread could be found.
	return (ret == ESRCH) ? 0: 1;

//	if (kill_rc == ESRCH) return 0;
//	return 1;
}


#if 0
void *hander_data(void *arg)
{
	int *p = (int *)arg;

	xb_printf("正在处理%d的数据\n",*p);
	sleep(1);
	xb_printf("==%d的数据处理成功\n",*p);

	return NULL;
}
int main()
{
// int threadpool_create(threadpool_t **p,int min_thr_num,int max_thr_num,int queue_max_size)
	threadpool_t *pool = NULL;
	int ret = threadpool_create(&pool,10,100,500);
	if(ret != 0)
	{
		xb_printf("the threadpool is create failed!\n");
		exit(-1);
	}

	int i=0;
	int *num = (int *)malloc(sizeof(int) * 10);
	for(i=0;i<10;i++) num[i]=i+1;

	// 添加任务
	for(i=0;i<10;i++)
	{
//		int threadpool_add_task(threadpool_t *pool, void*(*function)(void *arg), void *arg)
		threadpool_add_task(pool,hander_data,(void *)&num[i]);
	}


	char c = getchar();                                        /* 等子线程完成任务 */
	threadpool_destroy(&pool);

	return 0;

}

#endif


