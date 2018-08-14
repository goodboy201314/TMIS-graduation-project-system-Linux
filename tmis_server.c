/**
* @file       tmis_server.c
* @brief      tmis服务器端
* @details    tmis服务器端的实现，EPOLL + 线程池  + 密钥协商（安全通信）
* @author     项斌
* @date       2018/08/06
* @version    1.0
*/

#include <sys/epoll.h>
#include <string.h>
#include <time.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <stdio.h>
#include <sys/types.h>          /* See NOTES */
#include <sys/socket.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "log.h"
#include "threadpool.h"
#include "tmis_io.h"
#include "tmis_enc_denc.h"
#include "/usr/local/include/pbc/pbc.h"  //必须包含头文件pbc.h
#include "/usr/local/include/pbc/pbc_test.h"
#include "/usr/include/mysql/mysql.h"

/** 接受数据的数组大小 */
#define BUFLEN 2048

/** 并发数量 */
#define OPEN_MAX  1024

/** 服务器监听端口 */
#define SERV_PORT   8888

/** 发送的数据包相关信息 */
typedef struct tmis_packet
{
	unsigned int len;                   ///< 此次发送数据的长度
	char flag;                         ///< 此次发送数据的类型 ，协商密钥的，安全通信的
	char buf[BUFLEN];                  ///< 此次发送的数据
}tmis_packet_t;


struct epoll_event ep[OPEN_MAX];     ///< 全局变量, epoll_ctl的传出数组
FILE *fp;      						 ///< 全局变量，日志文件句柄
threadpool_t *tmispool; 			 ///< 全局变量，线程池
int efd;							///< 全局变量，红黑树树根

/** 公钥和私钥 */
char secret_key[1024] = "[1431701601476568613993916354570581999234296492200903722689435064403093647543786410908775082711468637043556899660242354405958838182001143332963964057164995, 2090155367049341967001403718508984758041491186470976194749112114432660174858545929700501744055058603817941671083836419094294404012587185432039026958345540]";
char public_key[1024] = "[8779246804865256595845410635551148521227644044548861627285453536743878386166265446937141101008408588690674901331738586548621281816777149434943936565852561, 5462710688655103662240594520449922001027729122965123487994410536107298056563228514615559984791707466524546778752823276552092115399599325705605166751646997]";

/** 分隔符 */
const char split_char_key_agreement[10]="我";    // "BB";//
const char split_char_communication[10]="AAAA";  ///< 每一条医疗记录之间的分割符号
const char split_char_communication_in[10]="AA"; ///< 一条医疗记录之间每个域的分割符号
#define len_split_char_key_agreement strlen(split_char_key_agreement)
#define len_split_char_communication strlen(split_char_communication)
#define len_split_char_communication_in strlen(split_char_communication_in)

/** 各个客户端的会话密钥 */
char session_keys[BUFLEN][20];

/////////////////////////////    函数实现     //////////////////////////////////

/**
 * @brief 初始化套接字
 * @param lfd 传出参数，监听套接字
 */
void initlistensocket(int *lfd)
{
	unsigned short port = SERV_PORT;		// 服务器端口号

	/* socket */
	int listenfd = socket(AF_INET, SOCK_STREAM, 0);
	if(listenfd==-1)
	{
		write_log(fp,"function socket is err:%s\n",strerror(errno));
		exit(-1);
	}

	fcntl(listenfd, F_SETFL, O_NONBLOCK);    //将socket设为非阻塞
	signal(SIGPIPE, SIG_IGN);   			//忽略管道破裂情况

	/* 地址复用 */
	int optval = 1;
	int ret = setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &optval,sizeof(optval));
	if (ret == -1)
	{
		write_log(fp,"function setsockopt is err:%s\n",strerror(errno));
		exit(-1);
	}

	/* bind */
	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(port);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ret = bind(listenfd, (struct sockaddr*) &addr, sizeof(addr));
	if (ret == -1)
	{
		write_log(fp,"function bind is err:%s\n",strerror(errno));
		exit(-1);
	}

	/* listen */
	ret = listen(listenfd, 128);
	if (ret == -1)
	{
		write_log(fp,"function listen is err:%s\n",strerror(errno));
		exit(-1);
	}

	write_log(fp,"TMIS服务器启动成功，这在%d端口监听客户端的连接.....\n",port);

	*lfd = listenfd;
	return;
}



/**
 * @brief 服务器处理客户端连接请求
 * @param lfd 服务器监听套接字
 */
void handle_connection(int lfd)
{
	struct sockaddr_in cliaddr;
	socklen_t len;
	int confd,ret;
	struct epoll_event tep;
	char str[30];

	/* lfd设置的是边沿触发，while循环是为了防止同时来n个客户端请求 */
	while(1)
	{
		len = sizeof(cliaddr);
		confd = accept(lfd,(struct sockaddr *)&cliaddr,&len);
		/* 被信号打断*/
		if(confd==-1)
		{
			if(errno==EINTR) continue;
			break;
		}

		//将confd也设置为非阻塞
		fcntl(confd, F_SETFL, O_NONBLOCK);
		tep.events = EPOLLIN | EPOLLET;  // 边沿触发模式
		tep.data.fd = confd;
		ret = epoll_ctl(efd,EPOLL_CTL_ADD,confd,&tep);
		if (ret == -1)
		{
			write_log(fp,"function epoll_ctl in while accept is err:%s\n",strerror(errno));
			exit(-1);
		}

		memset(str,0,sizeof(str));
		write_log(fp,"connection from %s at PORT %u\n",
                inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
                ntohs(cliaddr.sin_port));
	} // end for: while(1)

	return;
}


/**
 * @brief 获得医疗记录
 * @param user_id 客户端的用户名
 * @param fd  客户端socket
 * @param fp  输出日志句柄
 * @return 返回0，成功；否则，失败
 */
int handle_user_record_requset(char *user_id,int fd,FILE* fp)
{
	if(!user_id || !fp) return -1;
////////// 连接数据库，取得数据
//	printf("handle_user_record_requset\n");
	int ret = 0;
	// 1. init
	MYSQL mysql;
	if(mysql_init(&mysql) == NULL)
	{
		ret = mysql_errno(&mysql);
//		printf("func mysql_init error:%d\n",ret);
		write_log(fp,"func mysql_init error:%d\n",ret);
		return ret;
	}

	// 2. connect
	if(mysql_real_connect(&mysql,"localhost","root","123","db_tmis",0,NULL,0) == NULL)
	{
		ret = mysql_errno(&mysql);
//		printf("func mysql_real_connect error:%d\n",ret);
		write_log(fp,"func mysql_real_connect error:%d\n",ret);
		return ret;
	}

	// 3. select
	char sql[100] ={0};
	sprintf(sql,"select rtime,rdoctor,rsymptom,rfeedback from tmis_record where uid=\'%s\'",user_id);
//	printf("sql = %s\n",sql);
	mysql_query(&mysql,"set names utf8");
	ret = mysql_query(&mysql,sql);
	if(ret!=0)
	{
		ret = mysql_errno(&mysql);
//		printf("func mysql_query error:%d\n",ret);
		write_log(fp,"func mysql_query error:%d\n",ret);
		return ret;
	}

	MYSQL_RES *result = mysql_store_result(&mysql);
	if(result==NULL)
	{
		ret = mysql_errno(&mysql);
//		printf("func mysql_store_result error:%d\n",ret);
		write_log(fp,"func mysql_store_result error:%d\n",ret);
		return ret;
	}

	// 这里应该根据具体的记录条数来malloc内存，这里简单实现下
	MYSQL_ROW row = NULL;
	char str_record[4096*5]={0};
	char str_temp[1024]={0};
	while(row = mysql_fetch_row(result))
	{
//		printf("%s\t%s\t%s\t\n",row[0],row[1],row[2]);
		memset(str_temp,0,sizeof(str_temp));
		sprintf(str_temp,"%sAA%sAA%sAA%sAAAA",row[0],row[1],row[2],row[3]);
		strcat(str_record,str_temp);
	}
//	printf("str_record = %s\n",str_record);

	// 4. close
	mysql_close(&mysql);

////////// 加密所得的数据，然后返回给用户
//	int aes_encrypt(const unsigned char* in, const unsigned char* key, unsigned char* out);
	unsigned char bytes_record_back[4096*5]={0};
	aes_encrypt((unsigned char*)str_record,(unsigned char*)session_keys[fd],bytes_record_back);
//	int bytes2hex(const unsigned char* in, const int len, char *out);
	char str_record_back[4096*5]={0};
	int len1 = get_length(bytes_record_back);
	bytes2hex(bytes_record_back,len1,str_record_back);

////////// 返回给用户
	tmis_packet_t sendata;
	/* 3.回复客户端 */
	memset(&sendata,0,sizeof(sendata));
	sendata.flag = 2;
	int pktlen = strlen(str_record_back);
	sendata.len = htonl(pktlen);
	strcpy(sendata.buf,str_record_back);
	writen(fd,&sendata,pktlen+5);

	char str[30];
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	getpeername(fd, (struct sockaddr *)&cliaddr, &clilen);
	write_log(fp,"get the record request from the client %s\n",inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)));

	return 0;
}

/**
 * @brief 密钥协商过程中服务器端的步骤
 * @param constr 客户端发送的参数
 * @param fd  客户端
 * @param fp  输出日志句柄
 * @return 返回0，成功；否则，失败
 */
int key_agreement_server_do(char *constr,int fd,FILE* fp)
{
	if(!constr) return -1;

	//// 分割字符串
	char str_constr_Hi[2048]={0};
	char str_constr_Rc[2048]={0};
	char *split = strtok(constr,split_char_key_agreement);
	if(split) strcpy(str_constr_Hi,split);
	split = strtok(NULL,split_char_key_agreement);
	if(split) strcpy(str_constr_Rc,split);
	//// 转化成数组
	unsigned char bytes_Hi[4096]={0};
	unsigned char bytes_Rc[4096]={0};
//	int hex2bytes(const char* in, int len,unsigned char* out);
	hex2bytes(str_constr_Hi,strlen(str_constr_Hi),bytes_Hi);
	hex2bytes(str_constr_Rc,strlen(str_constr_Rc),bytes_Rc);

	pairing_t pairing;
	char s[16384];
	FILE *fp2 = stdin;
	fp2 = fopen("a.param", "r");
//	if (!fp) pbc_die("error opening a.param");
	if (!fp2) { write_log(fp,"error opening a.param\n"); return -1;}

	size_t count = fread(s, 1, 16384, fp2);
//	if (!count) pbc_die("input error");
	if (!count) { write_log(fp,"error opening a.param\n"); fclose(fp2);return -1;}

	fclose(fp2);
//	if (pairing_init_set_buf(pairing, s, count)) pbc_die("pairing init failed");
	if (pairing_init_set_buf(pairing, s, count)) { write_log(fp,"pairing init failed\n"); fclose(fp2); return -1;}
	// ======> pairing 初始化完成
	element_t element_P,elemetn_secret_key,element_public_key,element_rs;  // 这些要定义成全局的
	element_t element_Rs,element_Rc,element_k2,element_Ji;

	element_init_G1(element_P,pairing);
	element_init_G1(elemetn_secret_key,pairing);
	element_init_G1(element_public_key,pairing);
	element_init_G1(element_Rs,pairing);
	element_init_G1(element_Rc,pairing);
	element_init_G1(element_rs,pairing);
	element_init_G1(element_k2,pairing);
	element_init_G1(element_Ji,pairing);

	// 参数初始化
	char hash_str[30] = "xiangbin is a good boy!";
    element_from_hash(element_P, hash_str, strlen(hash_str));
//    element_printf("element_P = %B\n", element_P);  // 赋值：element_P

	element_random(element_rs);    // element_rs
	element_set_str(element_public_key,public_key,10); // element_public_key
	element_set_str(elemetn_secret_key,secret_key,10);// elemetn_secret_key

//// 1.计算k2
	element_from_bytes(element_Rc,bytes_Rc);
	element_mul(element_k2,elemetn_secret_key,element_Rc);

//// 2.解密Hi
	char str_k2[1024]={0};
	element_snprint(str_k2,sizeof(str_k2),element_k2);
	unsigned char bytes_md5_k2[50]={0};
	md5((unsigned char *)str_k2,bytes_md5_k2); // 得到解密密钥 bytes_k2

	char str_md5_k2[50]={0};
	char str_temp[50]={0};
//	int bytes2hex(const unsigned char* in, const int len, char *out);
	bytes2hex(bytes_md5_k2,strlen((char *)bytes_md5_k2),str_temp);
	strncpy(str_md5_k2,str_temp,16);

	char str_Hi[4096]={0};
//	int aes_decrypt(const unsigned char* in, const unsigned char* key, unsigned char* out);
	aes_decrypt(bytes_Hi,(unsigned char *)str_md5_k2,(unsigned char *)str_Hi);
//	printf("str_Hi = %s\n",str_Hi);


//// 3.分割字符串
	char str_IDi[1024]={0}; // 和注册的时候str_id一样
	char str_Ai[1024]={0};
	char str_t1[1024]={0};

	char *p = strtok(str_Hi,split_char_key_agreement);
	if(p) strcpy(str_IDi,p);
	p=strtok(NULL,split_char_key_agreement);
	if(p) strcpy(str_Ai,p);
	p=strtok(NULL,split_char_key_agreement);
	if(p) strcpy(str_t1,p);

//	printf("str_IDi = %s\n",str_IDi);
//	printf("str_Ai = %s\n",str_Ai);
//	printf("str_t1 = %s\n",str_t1);

///// 4.比较时间是否超时
	time_t t1;
	time_t t1_2 = time(NULL);
	string2time(str_t1, &t1);
	/*
	if(t1_2-t1>120)
	{
		printf("超时了。。。\n");
		return -1;
	}
	else printf("没有超时。。。\n");
	*/

///// 5.计算Ai2
	int len1 = strlen(str_IDi);
	int len2 = strlen(secret_key);
	char *CONSTR1 =(char *)malloc(sizeof(char) * (len1+len2)+1); // ID || KS
	memset(CONSTR1,0,len1+len2+1);
	strncpy(CONSTR1,str_IDi,len1);
	strncpy(CONSTR1+len1,secret_key,len2);
//	printf("CONSTR1 = %s\n",CONSTR1);
	unsigned char bytes_Ai[100]={0};
	sha1((unsigned char *)CONSTR1,bytes_Ai);

	mpz_t mpz_Ai2;
	char str_temp2[1024]={0};
	bytes2hex(bytes_Ai,strlen((char *)bytes_Ai),str_temp2);
	mpz_init_set_str(mpz_Ai2,str_temp2,16);	// mpz_Ai2
	char str_Ai2[1024]={0};
// char * mpz_get_str (char *str, int base, mpz_t op)
	mpz_get_str (str_Ai2, 10, mpz_Ai2);
	if(strcmp(str_Ai,str_Ai2)==0)
	{
//		printf("该用户为注册用户。。。\n");
//		write_log(fp,"the user is not registered!!\n");
	}
	else
	{
//		printf("该用户不是注册用户。。。\n");
		write_log(fp,"the user is not registered!!\n");
		return -1;
	}

//// 6.	生成rs
	element_random(element_rs);    // element_rs

//// 7. 计算Rs
	element_mul(element_Rs,element_rs,element_P);

//// 8.计算Ji
	element_mul(element_Ji,element_rs,element_Rc);

//// 9.计算 IDi || Rs || Ji || t2
	// str_t2
	time_t t2 = time(NULL);
//	printf("t2: %ld\n",t2);
	char str_t2[30]={0};
	time2string(t2, str_t2, sizeof(str_t2)/sizeof(char));
//	printf("str_time = %s\n",str_t2);
	// str_Rs
// int element_snprint(char *s, size_t n, element_t e)
	char str_Rs[1024]={0};
	element_snprint(str_Rs,sizeof(str_Rs),element_Rs);
	// str_Ji
	char str_Ji[1024]={0};
	element_snprint(str_Ji,sizeof(str_Rs),element_Ji);

	// 连接
	len1 = strlen(str_IDi);
	len2 = strlen(str_Rs);
	int len3 = strlen(str_Ji);
	int len4 = strlen(str_t2);
	char *CONSTR2=(char *)malloc(sizeof(char)*(len1+len2+len3+len4+1+4*len_split_char_key_agreement));
	memset(CONSTR2,0,len1+len2+len3+len4+1+4*len_split_char_key_agreement);
	strncpy(CONSTR2,str_IDi,len1);
	strncpy(CONSTR2+len1,split_char_key_agreement,len_split_char_key_agreement);
	strncpy(CONSTR2+len1+len_split_char_key_agreement,str_Rs,len2);
	strncpy(CONSTR2+len1+len_split_char_key_agreement+len2,split_char_key_agreement,len_split_char_key_agreement);
	strncpy(CONSTR2+len1+2*len_split_char_key_agreement+len2,str_Ji,len3);
	strncpy(CONSTR2+len1+2*len_split_char_key_agreement+len2+len3,split_char_key_agreement,len_split_char_key_agreement);
	strncpy(CONSTR2+len1+3*len_split_char_key_agreement+len2+len3,str_t2,len4);
	strncpy(CONSTR2+len1+3*len_split_char_key_agreement+len2+len3+len4,split_char_key_agreement,len_split_char_key_agreement);
//	printf("str_Li = %s\n",CONSTR2);

//// 10.计算Li
	unsigned char bytes_Li[4096]={0};
	aes_encrypt((unsigned char *)CONSTR2,(unsigned char *)str_md5_k2,bytes_Li);
//	printf("key_agreement：服务器端计算成功。。。。\n");
	char str_Li[4096]={0};
	len1 = get_length(bytes_Li);
//	int bytes2hex(const unsigned char* in, const int len, char *out);
	bytes2hex(bytes_Li,len1,str_Li);

///// 将数据发送给客户端
	tmis_packet_t sendata;
	/* 3.回复客户端 */
	memset(&sendata,0,sizeof(sendata));
	sendata.flag = 1;
	int pktlen = strlen(str_Li);
	sendata.len = htonl(pktlen);
	strcpy(sendata.buf,str_Li);
	writen(fd,&sendata,pktlen+5);
//	write_log(fp,"write data to %s at PORT %u\n",str,port);
//	printf("发送数据到客户端：%s\n",sendata.buf);


///// 4.计算sk
	len1=strlen(str_Ji);
	len2=strlen(str_t1);
	len3=strlen(str_t2);
	char *CONSTR4 = (char *)malloc(sizeof(char)*(len1+len2+len3+1));
	memset(CONSTR4,0,len1+len2+len3+1);
	strncpy(CONSTR4,str_Ji,len1);
	strncpy(CONSTR4+len1,str_t1,len2);
	strncpy(CONSTR4+len1+len2,str_t2,len3);

	unsigned char bytes_sk[50] = {0};
	md5((unsigned char*)CONSTR4, bytes_sk);
//	printhex(bytes_sk,16);
	char str_sk[50]={0};
	len1 = get_length(bytes_sk);
	bytes2hex(bytes_sk, len1, str_sk);
	char session_key[20] = {0};
	strncpy(session_key,str_sk,16);
//	printf("%s\n",session_key);
	memset(session_keys[fd],0,sizeof(session_keys[fd]));
	strcpy(session_keys[fd],session_key);
//	printf("%s\n",session_keys[fd]);

	char str[30];
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	getpeername(fd, (struct sockaddr *)&cliaddr, &clilen);
	write_log(fp,"share the session key %s with %s\n",session_key,inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)));


//////// 释放内存
	element_clear(element_P);
	element_clear(elemetn_secret_key);
	element_clear(element_public_key);
	element_clear(element_Rs);
	element_clear(element_Rc);
	element_clear(element_rs);
	element_clear(element_k2);
	element_clear(element_Ji);
	free(CONSTR1);
	free(CONSTR2);
	free(CONSTR4);
	mpz_clear(mpz_Ai2);

	return 0;
}

/**
 * @brief 处理数据的线程
 * @param arg 客户端socket
 */
void *handle_data(void *arg)
{
	int fd = (int)arg;

	char str[30];
	struct sockaddr_in cliaddr;
	socklen_t clilen = sizeof(cliaddr);
	unsigned short port = SERV_PORT;


	tmis_packet_t recvdata;
//	write_log(fp,"===在handle_data里面  fd = %d\n",fd);

	/* 获取对方的地址和端口号，出于性能考虑，可以省略 */
//	int getpeername(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
	getpeername(fd, (struct sockaddr *)&cliaddr, &clilen);
	write_log(fp,"receive data from %s at PORT %u\n",
					inet_ntop(AF_INET, &cliaddr.sin_addr, str, sizeof(str)),
					ntohs(cliaddr.sin_port));

	/* 1.读取数据，并且解析 */
	memset(&recvdata,0,sizeof(recvdata));
	size_t ret = readn(fd,&recvdata,5);
	if (ret == -1)
	{
		write_log(fp,"ERROR: receive data from %s at PORT %u\n",str,
				ntohs(cliaddr.sin_port));
		close(fd);
		fclose(fp);
//		exit(-1);  				//这样整个进程都结束了，我们不要这样的结果
//		pthread_exit(NULL);    //这样这个线程就结束了，我们应该把这个线程再次放到线程池中
		return NULL;
	}
	else if (ret < 5)
	{
		write_log(fp,"client %s is closed\n",str);
		close(fd);
		return NULL;
	}

	// 读取此次客户端发送数据的长度
	size_t n = ntohl(recvdata.len);
	// 读取此次客户端发送数据的标记
	char flag = recvdata.flag;
//	printf("flag = %d\n",flag);

	ret = readn(fd,recvdata.buf,n);
	if (ret == -1)
	{
		write_log(fp,"ERROR: receive data from %s at PORT %u\n",str,
				ntohs(cliaddr.sin_port));
		close(fd);
		fclose(fp);
		return NULL;
	}
	else if (ret < n)
	{
		write_log(fp,"client %s is closed\n",str);
		close(fd);
		return NULL;
	}
	write_log(fp,"the data:%s\n",recvdata.buf);

	/* 2.服务器根据flag解析数据 */
	if(flag==1)  // 密钥协商
	{
		//int key_agreement_server_do(char *constr,int fd)
		key_agreement_server_do(recvdata.buf,fd,fp);
	}
	else if(flag==2)
	{
		handle_user_record_requset(recvdata.buf,fd,fp);
	}

	return NULL;

//	tmis_packet_t sendata;
//	/* 3.回复客户端 */
//	memset(&sendata,0,sizeof(sendata));
//	sendata.flag = flag;
//	sendata.len = htonl(n);
//	strncpy(sendata.buf,recvdata.buf,n);
//	writen(fd,&sendata,n+5);
//	write_log(fp,"write data to %s at PORT %u\n",str,port);
//
//	return NULL;
}

/**
 * @brief 服务器处理客户端发送来的数据
 * @param fd 客户端
 */
void handle_clientdata(int fd)
{
	// 将任务添加到线程池中
//	int *p = (int *)malloc(sizeof(int));
//	*p = fd;
	threadpool_add_task(tmispool, handle_data, (void *)fd);
//	write_log(fp,"====== 添加任务到队列\n");

	return ;
}

/**
 * @brief 服务器端epoll的实现
 */
void do_service()
{
	struct epoll_event tep;
	int ret,nready,i,fd;


	/* socket,bind,listen */
	int lfd = 0;
	initlistensocket(&lfd);

	/* epoll_create */
	efd = epoll_create(OPEN_MAX);
	if (efd == -1)
	{
		write_log(fp,"function epoll_create is err:%s\n",strerror(errno));
		exit(-1);
	}

//	int epoll_ctl(int epfd, int op, int fd, struct epoll_event *event);
	tep.events = EPOLLIN | EPOLLET;  // 边沿触发模式
	tep.data.fd = lfd;
	ret = epoll_ctl(efd,EPOLL_CTL_ADD,lfd,&tep);
	if (ret == -1)
	{
		write_log(fp,"function epoll_ctl is err:%s\n",strerror(errno));
		exit(-1);
	}


	while(1)
	{
// int epoll_wait(int epfd, struct epoll_event *events,int maxevents, int timeout);
		nready = epoll_wait(efd,ep,OPEN_MAX,-1);
		if(nready == -1)
		{
			if(errno==EINTR) continue;
			write_log(fp,"function epoll_wait is err:%s\n",strerror(errno));
			exit(-1);
		}

		// 处理epoll_wait传出的事件
		for(i=0;i<nready;i++)
		{
			/* 如果不是"读"事件, 继续循环 */
			if (!(ep[i].events & EPOLLIN))   continue;

			fd = ep[i].data.fd;
			/* 处理客户端连接请求 */
			if(fd==lfd) handle_connection(lfd);
			else handle_clientdata(fd);
		}
	}// end for: while(1)

	return;
}

#if 1

// 以守护进程的方式
int main()
{
	/* 1.打开输入输出的日志文件 */
	fp=open_log("./tmis.log");

	if(fp==NULL)
	{
		printf("日志输出文件打开失败！\n");
		exit(-1);
	}

	/* 2.fork子进程，将子进程设置为守护进程 */
	pid_t pid = fork();
	if(pid==-1)
	{
		write_log(fp,"TMIS服务器启动失败：fork出错了！\n");
		exit(-1);
	}

	/* 父进程退出 */
	if(pid>0) exit(0);

	/* 子进程相关代码 */
	pid_t sid = setsid();
	if(sid==-1)
	{
		write_log(fp,"TMIS服务器启动失败：setsid出错了！\n");
		exit(-1);
	}

	umask(0);
	//close(STDIN_FILENO);
	//close(STDOUT_FILENO);
	//close(STDERR_FILENO);


	/* 3.创建线程池*/
	int ret = threadpool_create(&tmispool,10,100,100);
	if(ret != 0)
	{
		write_log(fp,"the threadpool is create failed!\n");
		exit(-1);
	}


	/* 4. 服务器端接受连接 ，处理数据 */
	write_log(fp,"TMIS服务器启动开始！\n");
	do_service();

	threadpool_destroy(&tmispool);; // 销毁线程池
	return 0;
}

#endif



