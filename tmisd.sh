#!/bin/bash

#######################################################
# tmis服务器的脚本代码
# 使用方法：
#		./tmisd.sh [参数]
# 参数说明：
#		start,	开启tmis服务器 ok
#		stop, 	关闭tmis服务器 ok
#		state,	查看tmis服务器状态 ok
#		log,	查看tmis服务器生成的日志  ok
#		clear,	清除tmis服务器生成的日志 ok
#       help,	查看帮助 ok
#
#######################################################

# 以下是各个功能的实现 ok
#### 检查参数   ####
if [ "$#" != "1" ]; then
	echo -e "\033[32m使用方法：$0 start|stop|state|log|clear|help\033[0m"
	exit -1
fi


#### 查看tmis服务器生成的日志  ok
if [ "$1" = "log" ]; then
	echo -e "\033[32m===========================================================================\033[1m"
	cat tmis.log 
	echo -e "\033[32m===========================================================================\033[0m"
	exit 1
fi


#### 查看tmis服务器生成的日志  ok
if [ "$1" = "help" ]; then
	echo -e "\033[32m===========================================================================\033[1m"
	echo -e "start\t开启tmis服务器" 
	echo -e "stop\t关闭tmis服务器" 
	echo -e "state\t查看tmis服务器状态" 
	echo -e "log\t查看tmis服务器生成的日志 " 
	echo -e "clear\t清除tmis服务器生成的日志" 
	echo -e "help\t查看帮助" 
	echo -e "\033[32m===========================================================================\033[0m"
	exit 1
fi


### 清除tmis服务器生成的日志
if [ "$1" = "clear" ]; then
	echo -e "\033[32m===========================================================================\033[1m"
	echo -e "确实要清除日志文件tmis.log么？(y/n)" 
	read choice
	
	# 判断是否是真的要删除
	if [ "$choice" = "y" -o "$choice" = "Y" ]; then
		
		rm -rf tmis.log
		# 判断删除文件有没有错误
		if [ "$?" != "0" ]; then
			echo "删除文件出错!"
		else
			echo "删除文件成功"
		fi
		
	fi
	echo -e "\033[32m===========================================================================\033[0m"
	exit 1
fi


### 查看tmis服务器状态
if [ "$1" = "state" ]; then
	echo -e "\033[32m===========================================================================\033[1m"
	echo -e "Proto  Recv-Q  Send-Q\t Local Address\t  Foreign Address\t   State"
	netstat -na | grep "tcp.*8888" 
	echo -e "\033[32m===========================================================================\033[0m"
	exit 1
fi


### 关闭tmis服务器
#  ps -ajx | grep "./tmis_server" ,	查找所有的满足含有关键字“./tmis_server”条件的
#  ps -ajx | grep "./tmis_server" | sed -n "/?/=",	查找出对应的行
#  得到对应的pid
#  杀进程
if [ "$1" = "stop" ]; then
	echo -e "\033[32m===========================================================================\033[1m"
	
	line=`ps -ajx | grep "./tmis_server" | sed -n "/?/="`   # 获得行号
	# echo $line
	pid=`ps -ajx | grep "./tmis_server" | sed -n ${line}p | awk '{print $2}'`  # 获得pid
	# echo $pid
	`kill -9 ${pid}`
	
	# 错误判断
	if [ "$?" != "0" ]; then
		echo "关闭tmis服务器出错!"
	else
		echo "关闭tmis服务器成功"
	fi
	
	echo -e "\033[32m===========================================================================\033[0m"
	exit 1
fi


### 开启tmis服务器
if [ "$1" = "start" ]; then
	echo -e "\033[32m===========================================================================\033[1m"
	
	# 首先查看是不是已经启动了
	line=`ps -ajx | grep "./tmis_server" | sed -n "/?/="`   # 获得行号
	
	# 首先判断有没有启动，如果没有启动，那么启动
	if [ -z "$line" ]; then
		echo "tmis服务器没有启动..."
		process_path="./tmis_server"
		$process_path &
		echo "tmis服务器正在启动..."
		
		line2=`ps -ajx | grep "./tmis_server" | sed -n "/?/="`   # 再次获得行号
		if [ -z "$line2" ]; then
			echo "tmis服务器启动失败..."
		else 
			echo "tmis服务器启动成功..."
		fi
	else
		echo "tmis服务器已经启动了..."
	fi
	
	echo -e "\033[32m===========================================================================\033[0m"
	exit 1
fi

### 处理输入一个参数，但是这个参数不是正常使用的参数情况
### 也就是处理输入1个错误参数的情况
echo -e "\033[32m使用方法：$0 start|stop|state|log|clear|help\033[0m"
exit -1
