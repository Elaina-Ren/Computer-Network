#include<windows.h>
#include "winsock.h"
#include<iostream>
#include <fstream>
#include <string>
#include<time.h>
#include <pthread.h>
pthread_mutex_t mutex;
#pragma comment(lib,"ws2_32.lib")  //winsock链接库文件
#pragma comment(lib, "pthreadVC2.lib")
#pragma warning(disable : 4996)
using namespace std;

SOCKET socketClient, socketServer;
SOCKADDR_IN ServerAddr, ClientAddr;


int tlen;
int length;
int reslen = 0;				//已发送文件长度
int resflag = 0;
int filelen = 0;
int timeflag = 0;
int len = sizeof(SOCKADDR);
const int bufferNum = 5000;
const int MAX_SIZE = 4096;
const int MAX_TIME = 1000;	//2s
const int windows = 20;		//窗口大小

u_short base = 0;			//窗口底部
u_short dis_Location = 0;	//记录失序位置
u_short nextSeq = 0;		//要发送的下一个序列号
u_short lastAck = 0;
u_short packnum = 0;
u_short tmpSeq = 0;
clock_t start;

char* buffer;
int dupAck = 0;				//重复ack个数
const int rwnd = 20;		//接收通告窗口大小，固定值
float cwnd = 1;				//拥塞窗口大小
int ssthresh = 10;			//慢启动状态阈值
int renostate = 0;			//慢启动-0,拥塞避免-1,快速重传,快速恢复-2

struct HeadMsg{
	u_short ack;			//确认序号              16位
	u_short SEQ;			//序号                  16位
	u_short datasize;		//数据长度				16位
	u_short checksum;		//校验和				16位
	u_short type;			//ACK标志位				16位   ack=4（最后一个数据包）
	u_short tag;			//标识：SYN=1;FIN=2     16位--------以上为12字节
};

//初始化套接字库
int Init(){
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "# WSAStartup ERROR =" << WSAGetLastError() << endl;
		return 0;
	}
	else
		cout << "# WSAStartUp						Success" << endl;

	socketClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socketClient == INVALID_SOCKET){
		cout << "# SOCKET EORROR = " << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}
	else
		cout << "# SocketInit						Success" << endl;

	ServerAddr.sin_family = AF_INET;//指定IP格式  
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.2");//IP地址为本地IP
	ServerAddr.sin_port = htons(12888);//指定端口号

	ClientAddr.sin_family = AF_INET;//指定ip格式  
	ClientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	ClientAddr.sin_port = htons(8888);//指定端口号

	if (bind(socketClient, (LPSOCKADDR)&ClientAddr, sizeof(ClientAddr)) == SOCKET_ERROR){
		cout << "# BIND ERROR = " << WSAGetLastError << endl;
		WSACleanup();
		return 0;
	}
	cout << "# SocketBind						Success" << endl;
	cout << "==================================================================" << endl;
	return 0;
}

u_short Checksum(u_short* message, int size){
	int count = (size + 1) / 2;
	u_short* buf = (u_short*)malloc(size + 1);
	memset(buf, 0, size + 1);
	memcpy(buf, message, size);
	u_long checksum = 0;
	while (count--) {
		checksum += *buf++;
		if (checksum & 0xFFFF0000) {
			checksum &= 0xFFFF;
			checksum++;
		}
	}
	return ~(checksum & 0xFFFF);
}

int Connect(){
	cout << "# Starting Connection" << endl;
	char* send_buf = new char[bufferNum];
	char* recv_buf = new char[bufferNum];
	HeadMsg header1 = { 0,0,0,0,0,1 };		//发送数据包SYN=1
	//计算校验和
	memset(send_buf, 0, bufferNum);
	memcpy(send_buf, &header1, sizeof(header1));
	header1.checksum = Checksum((u_short*)(send_buf), sizeof(header1));
	memset(send_buf, 0, bufferNum);
	memcpy(send_buf, &header1, sizeof(header1));
	int res = sendto(socketClient, send_buf, bufferNum, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
	if (res == SOCKET_ERROR){
		cout << "SENDTO ERROR = " << WSAGetLastError() << endl;
		return 0;
	}

	int len = sizeof(SOCKADDR);
	HeadMsg mes_recv;
	while (1){
		//int res = recvfrom(socketClient, recv_buf, bufferNum, 0, (SOCKADDR*)&ServerAddr, &len);
		if (recvfrom(socketClient, recv_buf, bufferNum, 0, (SOCKADDR*)&ServerAddr, &len) == SOCKET_ERROR){
			cout << "接收失败" << endl;
			Sleep(20);
			continue;
		}
		memcpy(&mes_recv, recv_buf, sizeof(mes_recv));
		if (Checksum((u_short*)(recv_buf), sizeof(mes_recv)) != 0){
			cout << "校验错误！！！" << endl;
			continue;
		}
		if ((mes_recv.ack == header1.SEQ + 1) && (mes_recv.tag == 1) && (mes_recv.type == 1)){
			cout << "# ConnectionEstablishment				Success" << endl;
			cout << "=================================================================" << endl;
			return 1;
		}
	}
	delete[] send_buf;
	delete[] recv_buf;
	return 0;
}


int send_package(char* buffer){
	base;
	u_short dislocation = dis_Location;
	pthread_mutex_lock(&mutex);
	cout << "Resending messages from seq = " << base << " to seq = " << dislocation << endl;
	pthread_mutex_unlock(&mutex);
	char* send_buf = new char[bufferNum];
	int reslen1 = 0;
	//	发送数据包
	for (u_short i = base; i <= dislocation; i++){
		if (dislocation <base || i>base + windows){
			pthread_mutex_lock(&mutex);
			cout << "Error !!!" << endl;
			pthread_mutex_unlock(&mutex);
			break;
		}
		tmpSeq = i;
		HeadMsg message = { i,i,MAX_SIZE,0,1,3 };		//发送报文初始化
		memset(send_buf, 0, bufferNum);
		memcpy(send_buf, &message, sizeof(message));

		//确认数据包位置 —— 确认失序位置
		if (i == base){
			reslen1 = base * MAX_SIZE;
			memcpy(send_buf + sizeof(message), buffer + reslen1, message.datasize);
			reslen1 += (int)message.datasize;
		}
		else{
			if ((length - reslen1) > MAX_SIZE){
				memcpy(send_buf + sizeof(message), buffer + reslen1, message.datasize);
				reslen1 += (int)message.datasize;
			}
			//数据包最后一部分，传输时设置ACK=4
			else{
				tlen = length - reslen1;
				message.datasize = (u_short)tlen;
				message.type = 4;						//接收端退出
				memcpy(send_buf + sizeof(message), buffer + reslen1, message.datasize);
				resflag = 1;
			}
		}
		//计算校验和
		memcpy(send_buf, &message, sizeof(message));
		message.checksum = Checksum((u_short*)(send_buf), sizeof(message) + message.datasize);
		memcpy(send_buf, &message, sizeof(message));

		//数据发送
		int res = sendto(socketClient, send_buf, bufferNum, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
		if (res == SOCKET_ERROR){
			cout << "发送失败" << endl;
			continue;
		}
		pthread_mutex_lock(&mutex);
		cout << "--> [ Resend Client ] seq:" << message.SEQ << " ack:" << message.ack << " ACK:" << message.type;
		cout << " check:" << message.checksum << " length:" << message.datasize << endl;
		pthread_mutex_unlock(&mutex);
	}
	//要发送的下一个序列号为失序位置+1
	nextSeq = dislocation + 1;
	//开始计时
	start = clock();
	return 0;
}


DWORD WINAPI Timer(LPVOID lpParameter){
	//确认消息接收
	while (1){
		//结束计时
		clock_t finish = clock();
		if (finish - start > MAX_TIME){
			pthread_mutex_lock(&mutex);
			cout << "==========================   Timeout  ===========================" << endl;
			pthread_mutex_unlock(&mutex);

			//超时未收到新的ACK，进入慢启动状态
			ssthresh = cwnd / 2;//阈值设为窗口大小的一半
			cwnd = 1;			//窗口大小设为1
			dupAck = 0;			//重置重复ack个数
			if (renostate != 0)	//超时则进入慢启动状态
				renostate = 0;
			send_package(buffer);
		}

		//线程结束条件
		if ((base == nextSeq && nextSeq == packnum) || timeflag == 1){
			break;
		}
	}
	return 0;
}

DWORD WINAPI RevThread(LPVOID lpParameter){
	char* recv_buf = new char[bufferNum];

	//确认消息接收
	while (1){
		memset(recv_buf, 0, bufferNum);
		int res = recvfrom(socketClient, recv_buf, bufferNum, 0, (SOCKADDR*)&ClientAddr, &len);
		HeadMsg recv_Msg;
		memcpy(&recv_Msg, recv_buf, sizeof(recv_Msg));
		recv_Msg.checksum = Checksum((u_short*)(recv_buf), sizeof(recv_Msg));

		/*
		初始状态：constatus = 0, 慢启动阶段
		初始化窗口大小N为1，阈值ssthresh = 10；
		每收到一个ACK, 窗口大小加1，此时窗口大小呈指数级增长；
		若窗口增长到大于或者等于阈值ssthresh时，进入拥塞避免阶段；
		若超时没有收到新的ACK, 则阈值设为窗口大小的一半，窗口大小设为1，重新进入慢启动阶段，并重传未确认的数据包；
		若收到冗余ACK, 开始计数，累计收到3个重复ACK时，进入快速重传阶段，阈值设为窗口大小的一半，窗
		口大小设置为阈值加3，进入快速恢复阶段。接收到冗余ACK时，cwnd不变

		constatus=1, 拥塞避免阶段
		每收到一个ACK,窗口大小N增加1/窗口大小,此时窗口大小呈线性增加；
		若超时没有收到新的ACK,则阈值设为窗口大小的一半，窗口大小设为1，重新进入慢启动阶段；
		若收到冗余ACK,开始计数，累计收到3个重复ACK时，进入快速重传阶段，阈值设为窗口大小的一半，窗
		口大小设置为阈值加3，进入快速恢复阶段。

		constatus=2, 快速恢复阶段
		每收到一个冗余ACK,窗口大小加1，窗口增长到大于或者等于阈值ssthresh时，进入拥塞避免阶段；
		当收到三次重复 ACK 时，进入快速恢复阶段；
		每收到一个重复 ACK，cwnd 增 1；
		出现超时之后进入慢启动阶段；
		收到新的 ACK 进入拥塞避免阶段
		*/

		int flag = 0;
		//检验和无误，更改base
		if (recv_Msg.tag == 3 && (recv_Msg.type == 1 || recv_Msg.type == 4)) {
			//消息接收正常
			if (base <= recv_Msg.ack - 1) {
				if (recv_Msg.SEQ == packnum - 1) {
					flag = 0;
				}
				//	处于慢启动状态
				if (renostate == 0) {
					cwnd++;		//窗口加1
					dupAck = 0;	//重置冗余ack个数
					base++;
					//	窗口大小超过阈值，则进入拥塞避免状态
					if (cwnd >= ssthresh) {
						renostate = 1;//拥塞控制状态
					}
					pthread_mutex_lock(&mutex);//修改1
					cout << "[ 慢状态顺利接收 ] 此时窗口位于 " << base << " 下一个发送的数据包序列号为 " << tmpSeq;
					cout << " 窗口大小为 " << cwnd << " 窗口阈值为 " << ssthresh << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
				}
				//	处于拥塞避免状态
				else if (renostate == 1) {
					base++;
					cwnd += 1.0 / cwnd;			//窗口大小N增加1 / 窗口大小
					dupAck = 0;					//重置冗余ack个数
					pthread_mutex_lock(&mutex);
					cout << "[ 拥塞控制状态顺利接收 ] 此时窗口位于 " << base << "下一个发送的数据包序列号为 " << tmpSeq;
					cout << " 窗口大小为 " << cwnd << " 窗口阈值为 " << ssthresh << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
				}
				//	处于快速恢复状态 —— 收到新的ACK将转入拥塞避免状态
				else if (renostate == 2){
					base++;
					dupAck = 0;			//重置冗余ack个数
					cwnd = ssthresh;	//窗口大小设置为阈值
					renostate = 1;		//进入拥塞避免状态
					pthread_mutex_lock(&mutex);
					cout << "[ 快速恢复状态顺利接收 ] 此时窗口位于 " << base << "下一个发送的数据包序列号为 " << tmpSeq;
					cout << " 窗口大小为 " << int(cwnd) << " 窗口阈值为 " << ssthresh << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
					Sleep(2);
				}
				start = clock();

			}
			//接收消息有问题
			else{
				if (recv_Msg.SEQ == packnum - 1){
					flag = 1;
				}
				//	处于快速恢复状态 —— 收到冗余ACK
				if (renostate == 2){
					cwnd += 1;	// 窗口大小加1
					pthread_mutex_lock(&mutex);
					cout << "[ 快速恢复阶段收到冗余ack ]" << "此时窗口大小为 " << cwnd << " 窗口阈值为 " << ssthresh << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
				}
				else{
					dis_Location = recv_Msg.SEQ;	//记录失序位置
					dupAck++;						//重复ACK数量++
					pthread_mutex_lock(&mutex);
					cout << "# 接收到重复ack个数为: " << dupAck << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
					//	当重复ACK超过3次，进入快速恢复阶段
					if (dupAck == 3){
						ssthresh = cwnd / 2;	//阈值设为窗口大小的一半
						cwnd = ssthresh + 3;	//窗口大小设置为阈值加3
						cout << "[ 收到重复ACK3次 ] 窗口大小为 " << int(cwnd) << " 窗口阈值为 " << ssthresh << endl;
						renostate = 2;			//快速恢复阶段
						send_package(buffer);
						Sleep(20);
					}
				}
			}
			if (dupAck >= 3){
				renostate = 2;					//快速恢复阶段
				Sleep(20);
			}
		}
		//线程结束条件
		if (recv_Msg.SEQ == packnum - 1 && flag != 1){
			resflag = 2;
			timeflag = 1;
			break;
		}
	}
	return 0;
}


int send(string filename){
	cout << endl;
	// 以二进制方式打开文件
	ifstream fin(filename, ifstream::in | ios::binary);
	if (!fin) {
		cout << "# fOpen Error" << endl;
		return 0;
	}
	fin.seekg(0, fin.end);		// 指针定位在文件结束处
	length = fin.tellg();		// 获取文件大小（字节）
	fin.seekg(0, fin.beg);		// 指针定位在文件头
	buffer = new char[length];
	memset(buffer, 0, length);
	fin.read(buffer, length);	// 读取文件数据到缓冲区buffer中
	filelen = length;
	fin.close();//关闭文件

	char* send_buf = new char[bufferNum];
	char* recv_buf = new char[bufferNum];
	u_short first_Seq = 0;		//初始序列号

	//包的数量
	packnum = (length % MAX_SIZE == 0) ? length / MAX_SIZE : length / MAX_SIZE + 1;
	//文件分段循环发送
	while (1){
		//窗口未满
		if (nextSeq < base + cwnd && nextSeq < packnum){
			//得到发送的数据段
			HeadMsg header = { nextSeq,nextSeq,MAX_SIZE,0,1,3 };//发送报文初始化
			memset(send_buf, 0, bufferNum);
			memcpy(send_buf, &header, sizeof(header));
			tmpSeq = nextSeq;

			//数据包切分
			//数据包长度不足最大传输值
			reslen = nextSeq * MAX_SIZE;
			if (length <= MAX_SIZE){
				header.datasize = (u_short)length;
				memcpy(send_buf + sizeof(header), buffer, length);
			}
			//正常切分
			else if ((length - reslen) > MAX_SIZE){
				memcpy(send_buf + sizeof(header), buffer + reslen, header.datasize);
				//reslen += (int)mess.mlength;
			}
			//数据包最后一部分，传输时设置ACK=4
			else{
				tlen = length - reslen;
				header.datasize = (u_short)tlen;
				header.type = 4;//接收端退出
				memcpy(send_buf + sizeof(header), buffer + reslen, header.datasize);
				resflag = 1;
			}

			//计算校验和
			memcpy(send_buf, &header, sizeof(header));
			header.checksum = Checksum((u_short*)(send_buf), sizeof(header) + header.datasize);
			memcpy(send_buf, &header, sizeof(header));
			if (base == nextSeq){
				start = clock();
			}
			//数据发送
			int res = sendto(socketClient, send_buf, bufferNum, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
			nextSeq++;//下一数据包发送
			if (res == SOCKET_ERROR){
				cout << "发送失败" << endl;
				continue;
			}
			pthread_mutex_lock(&mutex);
			//cout << "-->数据传输成功" << endl;
			cout << "--> [ Client ] seq:" << header.SEQ << " ack:" << header.ack << " ACK:" << header.type;
			cout << " check:" << header.checksum << " length:" << header.datasize << endl;
			pthread_mutex_unlock(&mutex);
		}
		if (nextSeq == 1){
			CreateThread(NULL, 0, &Timer, NULL, 0, 0);
			CreateThread(NULL, 0, &RevThread, NULL, 0, 0);
		}
		if (resflag == 2){
			CloseHandle(Timer);
			CloseHandle(RevThread);
			cout << "# Thread Closed!" << endl;
			break;
		}
	}
	delete[] send_buf;
	delete[] recv_buf;
	return 0;
}


int disConnect(){
	char* send_buf = new char[bufferNum];
	char* recv_buf = new char[bufferNum];
	HeadMsg header1 = { 0,0,0,0,0,2 };
	memset(send_buf, 0, bufferNum);
	memcpy(send_buf, &header1, sizeof(header1));
	header1.checksum = Checksum((u_short*)(send_buf), sizeof(header1));
	memset(send_buf, 0, bufferNum);
	memcpy(send_buf, &header1, sizeof(header1));
	//int res = sendto(socketClient, send_buf, bufferNum, 0, (sockaddr*)&ServerAddr, sizeof(sockaddr));
	if (sendto(socketClient, send_buf, bufferNum, 0, (sockaddr*)&ServerAddr, sizeof(sockaddr)) == SOCKET_ERROR){
		cout << "# Fail to send messages" << endl;
		printf("send error:%d\n", WSAGetLastError());
		return 0;
	}
	int len = sizeof(sockaddr);
	HeadMsg mes_recv;
	delete[] send_buf;
	delete[] recv_buf;
	return 0;
}


int main()
{
	mutex = PTHREAD_MUTEX_INITIALIZER;
	cout << "  /$$$$$$    /$$       /$$$$$$   /$$$$$$$$  /$$    /$$ /$$$$$$$$" << endl;
	cout << " / $$__ $$  | $$      | _  $$_/ | $$_____/ | $$$  | $$| __  $$__ /" << endl;
	cout << " | $$   __/ | $$        | $$    | $$       | $$$$ | $$    | $$" << endl;
	cout << " | $$       | $$        | $$    | $$$$$    | $$ $$  $$    | $$" << endl;
	cout << " | $$       | $$        | $$    | $$__/    | $$  $$ $$    | $$" << endl;
	cout << " | $$    $$ | $$        | $$    | $$       | $$    $$$    | $$" << endl;
	cout << " | $$$$$$/  | $$$$$$$$ / $$$$$$ | $$$$$$$$ | $$     $$    | $$" << endl;
	cout << "  ______/   |________/|______/  |________/ |__ /   __/    |__ / " << endl;
	cout << "=================================================================" << endl;
	cout << "Starting client service" << endl;
	//初始化套接字库
	Init();

	int rflag = Connect();
	if (rflag){
		int choofl;
		cout << "# Starting sending message" << endl;
		cout << "# Please input the filepath" << endl;
		printf("+-----------------+-----------------------------+                  \n");
		printf("| [ 1 ]           | 1.jpg                       |\n");
		printf("| [ 2 ]           | 2.jpg                       |\n");
		printf("| [ 3 ]           | 3.jpg                       |\n");
		printf("| [ 4 ]           | helloworld.txt              |\n");
		printf("+-----------------+-----------------------------+\n");
		cout << "# Filename:";
		cin >> choofl;
		clock_t totalstart = clock();
		switch (choofl){
		case 1:
			send("C:\\Users\\Lenovo\\Desktop\\实验3-3\\Client\\Debug\\1.jpg");
			break;
		case 2:
			send("C:\\Users\\Lenovo\\Desktop\\实验3-3\\Client\\Debug\\2.jpg");
			break;
		case 3:
			send("C:\\Users\\Lenovo\\Desktop\\实验3-3\\Client\\Debug\\3.jpg");
			break;
		case 4:
			send("C:\\Users\\Lenovo\\Desktop\\实验3-3\\Client\\Debug\\helloworld.txt");
			break;
		default:
			cout << "有误" << endl;
			break;
		}

		clock_t totalend = clock();
		cout << endl;
		printf("+-----------------+-----------------------------+                  \n");
		printf("| Time:           | %-20f seconds|\n", (double)(totalend - totalstart) / CLOCKS_PER_SEC);
		printf("| Totally send:   | %-20d bytes  |\n", filelen * 8);
		printf("| Rate:           | %-20f bytes/s|\n", double(filelen * 8 / ((double)(totalend - totalstart) / CLOCKS_PER_SEC)));
		printf("+-----------------+-----------------------------+\n");
		cout << endl << "# Service Closed!" << endl;
		cout << "=================================================================" << endl;
		disConnect();
		cout << endl;
	}

	Sleep(200);
	closesocket(socketClient);
	WSACleanup();
	system("pause");
	return 0;
}