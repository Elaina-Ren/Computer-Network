#include <stdio.h>
#include<iostream>
#include<string>
#include "winsock.h"  //winsock��غ���ͷ�ļ�
#include <conio.h>
#include<time.h>
#pragma warning(disable:4996)
#pragma comment(lib,"ws2_32.lib")  //winsock���ӿ��ļ�
#define RECV_OVER 1
#define RECV_YET 0

using namespace std;

char userName[16] = { 0 };
int iStatus = RECV_YET;

//#########################ʱ���������#############################
string getTime() {
	//��ȡϵͳʱ��
	time_t t = time(0);
	char tmp[64];
	strftime(tmp, sizeof(tmp), "%Y/%m/%d %X %A", localtime(&t));
	return tmp;
}


//###########################������Ϣ#############################
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
			cout << "��Ϣ����ʧ��!" << endl;
			break;
		}
		else
		{
			cout << buf <<"���ѽ��ա�" << endl;
			//cout << server_name << "###" << endl;
		}
	}
	return 0;
}

//###########################��������###########################
DWORD WINAPI  ThreadSend(LPVOID lparam)
{
	SOCKET Cli = (SOCKET)(LPVOID)lparam;
	char buffer[128] = { 0 };
	char SendTime[32] = { NULL };
	int selen = 0;
	int lent = 0;
	while (1)
	{
		//��ȡϵͳʱ��
		//��ȡϵͳʱ��
		time_t t = time(0);
		memset(buffer, 0, sizeof(buffer));
		strftime(SendTime, sizeof(SendTime), "%Y-%m-%d %H:%M:%S", localtime(&t));

		//Ϊ����ʾ���ۣ���һ���޻��ԵĶ�ȡ�ַ�������ȡ�س�
		int c = getch();
		
		printf("%s  %s:", SendTime, userName);
		gets_s(buffer);
		if (strcmp(buffer, "exit") == 0)
		{
			//strftime(SendTime, sizeof(SendTime), "%Y-%m-%d %H:%M:%S", localtime(&t));
			lent = send(Cli, SendTime, 32, 0);
			selen = send(Cli, buffer, 128, 0);
			//##########����Ϣ����exit,over############
			cout << "������3����˳�" << endl;
			Sleep(3000);
			closesocket(Cli);
			WSACleanup();
			return 0;
		}
		
		lent = send(Cli, SendTime, 32, 0);
		selen = send(Cli, buffer, 128, 0);
		if (selen < 0 && lent < 0)
		{
			cout << "��Ϣ����ʧ�ܣ�" << endl;
			break;
		}
	}
	return 0;
}


int main()
{
	//��ʼ���׽���
	WORD wVersionRequested = MAKEWORD(2, 2);	//�汾�� 
	WSADATA wsaData;							//socket��ϸ��Ϣ 
	WSAStartup(wVersionRequested, &wsaData);	//��ʼ��Socket DLL,Э��ʹ�õ�Socket�汾
	if (WSAStartup(wVersionRequested, &wsaData) != 0)
	{
		cout << "#############��ʼ���׽��ֿ�ʧ�ܣ�################" << endl;
	}
	
	//socket()
	SOCKET Cli = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (Cli == INVALID_SOCKET)
	{
		cout << "SOCKET ERROR";
		WSACleanup();
		return 0;
	}


	//���÷�������ַ
	sockaddr_in SrvAddr;
	SrvAddr.sin_family = AF_INET;
	SrvAddr.sin_port = htons(49711);
	SrvAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	cout << "####################������������####################" << endl;
	int len = sizeof(SrvAddr);
	if (connect(Cli, (SOCKADDR*)&SrvAddr, len) == SOCKET_ERROR) {
		cout << endl << "CONNECTING ERROR" << endl;
		cout << WSAGetLastError() << endl;
		return 0;
	}

	cout << "######################���ӳɹ�######################" << endl;
	//��ʼ�����Ʋ������ʾ
	cout << "----------------TYPING IN YOUR NICKNAME-------------" << endl;

	//printf("You have been in the room\n");
	//��ȡ�û���
	//printf("�������������: ");
	gets_s(userName);
	send(Cli, userName, sizeof(userName), 0);
	printf("\n\n");

	cout << "====================================================" << endl;
	cout << "###                 �н���������                 ###" << endl;
	cout << "====================================================" << endl;
	/// </summary>
	/// <returns></returns>
	///
	
	DWORD  dwThreadId1, dwThreadId2;
	//HANDLE  hThread1, hThread2;
	//���������̺߳ͷ����߳�                   
	HANDLE hThread1 = CreateThread(NULL, 0, ThreadRecv, (LPVOID)Cli, 0, NULL);
	HANDLE hThread2 = CreateThread(NULL, 0, ThreadSend, (LPVOID)Cli, 0, NULL);

	//���̹رգ�����������
	if (WaitForSingleObject(hThread1, INFINITE) == WAIT_OBJECT_0 ||
		WaitForSingleObject(hThread2, INFINITE) == WAIT_OBJECT_0) {
		CloseHandle(hThread1);
		CloseHandle(hThread2);
		return 0;
	}

	//ѭ����ֹ���߳��˳�
	for (int k = 0; k < 1000; k++)
		Sleep(10000000);
	closesocket(Cli);
	WSACleanup();

	return 0;
}