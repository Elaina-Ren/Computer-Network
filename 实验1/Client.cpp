#include <stdio.h>
#include<iostream>
#include<string>
#include "winsock.h"  //winsock相关函数头文件
#include <conio.h>
#include<time.h>
#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")  //winsock链接库文件
#define RECV_OVER 1
#define RECV_YET 0

using namespace std;

char userName[16] = { 0 };
int iStatus = RECV_YET;

//#########################时间输出函数#############################
string getTime() {
	//获取系统时间
	time_t t = time(0);
	char tmp[64];
	strftime(tmp, sizeof(tmp), "%Y/%m/%d %X %A", localtime(&t));
	return tmp;
}


//###########################接收消息#############################
DWORD WINAPI  ThreadRecv(LPVOID lparam)
{
	SOCKET Cli = (SOCKET)(LPVOID)lparam;
	char buf[128] = { 0 };
	int relen = 0;
	while (1)
	{
		memset(buf, 0, sizeof(buf));
		relen = recv(Cli, buf, 128, 0);
		//recv_lent = recv(clientsocket, recv_time, 32, 0);
		//recv_len = recv(clientsocket, recv_buf, 1024, 0);
		if (relen < 0)
		{
			cout << "消息接收失败!" << endl;
			break;
		}
		else
		{
			cout << buf <<"【已接收】" << endl;
			//cout << server_name << "###" << endl;
		}
	}
	return 0;
}

//###########################发送数据###########################
DWORD WINAPI  ThreadSend(LPVOID lparam)
{
	SOCKET Cli = (SOCKET)(LPVOID)lparam;
	char buffer[128] = { 0 };
	char SendTime[32] = { NULL };
	int selen = 0;
	int lent = 0;
	while (1)
	{
		//获取系统时间
		//获取系统时间
		time_t t = time(0);
		memset(buffer, 0, sizeof(buffer));
		strftime(SendTime, sizeof(SendTime), "%Y-%m-%d %H:%M:%S", localtime(&t));

		//为了显示美观，加一个无回显的读取字符函数读取回车
		int c = getch();
		
		printf("%s  %s:", SendTime, userName);
		gets_s(buffer);
		if (strcmp(buffer, "exit") == 0)
		{
			//strftime(SendTime, sizeof(SendTime), "%Y-%m-%d %H:%M:%S", localtime(&t));
			lent = send(Cli, SendTime, 32, 0);
			selen = send(Cli, buffer, 128, 0);
			//##########对消息分类exit,over############
			cout << "程序将在3秒后退出" << endl;
			Sleep(3000);
			closesocket(Cli);
			WSACleanup();
			return 0;
		}
		
		lent = send(Cli, SendTime, 32, 0);
		selen = send(Cli, buffer, 128, 0);
		if (selen < 0 && lent < 0)
		{
			cout << "消息发送失败！" << endl;
			break;
		}
	}
	return 0;
}


int main()
{
	//初始化套接字
	WORD wVersionRequested = MAKEWORD(2, 2);	//版本号 
	WSADATA wsaData;							//socket详细信息 
	WSAStartup(wVersionRequested, &wsaData);	//初始化Socket DLL,协商使用的Socket版本
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		cout << "#############初始化套接字库失败！################" << endl;
	}
	
	//socket()
	SOCKET Cli = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (Cli == INVALID_SOCKET)
	{
		cout << "SOCKET ERROR";
		WSACleanup();
		return 0;
	}


	//设置服务器地址
	sockaddr_in SrvAddr;
	SrvAddr.sin_family = AF_INET;
	SrvAddr.sin_port = htons(49711);
	SrvAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	cout << "####################服务器连接中####################" << endl;
	int len = sizeof(SrvAddr);
	if (connect(Cli, (SOCKADDR*)&SrvAddr, len) == SOCKET_ERROR) {
		cout << endl << "CONNECTING ERROR" << endl;
		cout << WSAGetLastError() << endl;
		return 0;
	}

	cout << "######################连接成功######################" << endl;
	//初始化名称并输出提示
	cout << "----------------TYPING IN YOUR NICKNAME-------------" << endl;

	//printf("You have been in the room\n");
	//获取用户名
	//printf("请输入你的名字: ");
	gets_s(userName);
	send(Cli, userName, sizeof(userName), 0);
	printf("\n\n");

	cout << "====================================================" << endl;
	cout << "###                 葱姜蒜三联盟                 ###" << endl;
	cout << "====================================================" << endl;
	/// </summary>
	/// <returns></returns>
	///
	
	DWORD  dwThreadId1, dwThreadId2;
	//HANDLE  hThread1, hThread2;
	//开启接受线程和发送线程                   
	HANDLE hThread1 = CreateThread(NULL, 0, ThreadRecv, (LPVOID)Cli, 0, NULL);
	HANDLE hThread2 = CreateThread(NULL, 0, ThreadSend, (LPVOID)Cli, 0, NULL);

	//进程关闭，主函数返回
	if (WaitForSingleObject(hThread1, INFINITE) == WAIT_OBJECT_0 ||
		WaitForSingleObject(hThread2, INFINITE) == WAIT_OBJECT_0) {
		CloseHandle(hThread1);
		CloseHandle(hThread2);
		return 0;
	}

	//循环防止主线程退出
	for (int k = 0; k < 1000; k++)
		Sleep(10000000);
	closesocket(Cli);
	WSACleanup();

	return 0;
}