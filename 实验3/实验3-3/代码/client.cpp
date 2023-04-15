#include<windows.h>
#include "winsock.h"
#include<iostream>
#include <fstream>
#include <string>
#include<time.h>
#include <pthread.h>
pthread_mutex_t mutex;
#pragma comment(lib,"ws2_32.lib")  //winsock���ӿ��ļ�
#pragma comment(lib, "pthreadVC2.lib")
#pragma warning(disable : 4996)
using namespace std;

SOCKET socketClient, socketServer;
SOCKADDR_IN ServerAddr, ClientAddr;


int tlen;
int length;
int reslen = 0;				//�ѷ����ļ�����
int resflag = 0;
int filelen = 0;
int timeflag = 0;
int len = sizeof(SOCKADDR);
const int bufferNum = 5000;
const int MAX_SIZE = 4096;
const int MAX_TIME = 1000;	//2s
const int windows = 20;		//���ڴ�С

u_short base = 0;			//���ڵײ�
u_short dis_Location = 0;	//��¼ʧ��λ��
u_short nextSeq = 0;		//Ҫ���͵���һ�����к�
u_short lastAck = 0;
u_short packnum = 0;
u_short tmpSeq = 0;
clock_t start;

char* buffer;
int dupAck = 0;				//�ظ�ack����
const int rwnd = 20;		//����ͨ�洰�ڴ�С���̶�ֵ
float cwnd = 1;				//ӵ�����ڴ�С
int ssthresh = 10;			//������״̬��ֵ
int renostate = 0;			//������-0,ӵ������-1,�����ش�,���ٻָ�-2

struct HeadMsg{
	u_short ack;			//ȷ�����              16λ
	u_short SEQ;			//���                  16λ
	u_short datasize;		//���ݳ���				16λ
	u_short checksum;		//У���				16λ
	u_short type;			//ACK��־λ				16λ   ack=4�����һ�����ݰ���
	u_short tag;			//��ʶ��SYN=1;FIN=2     16λ--------����Ϊ12�ֽ�
};

//��ʼ���׽��ֿ�
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

	ServerAddr.sin_family = AF_INET;//ָ��IP��ʽ  
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.2");//IP��ַΪ����IP
	ServerAddr.sin_port = htons(12888);//ָ���˿ں�

	ClientAddr.sin_family = AF_INET;//ָ��ip��ʽ  
	ClientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");
	ClientAddr.sin_port = htons(8888);//ָ���˿ں�

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
	HeadMsg header1 = { 0,0,0,0,0,1 };		//�������ݰ�SYN=1
	//����У���
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
			cout << "����ʧ��" << endl;
			Sleep(20);
			continue;
		}
		memcpy(&mes_recv, recv_buf, sizeof(mes_recv));
		if (Checksum((u_short*)(recv_buf), sizeof(mes_recv)) != 0){
			cout << "У����󣡣���" << endl;
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
	//	�������ݰ�
	for (u_short i = base; i <= dislocation; i++){
		if (dislocation <base || i>base + windows){
			pthread_mutex_lock(&mutex);
			cout << "Error !!!" << endl;
			pthread_mutex_unlock(&mutex);
			break;
		}
		tmpSeq = i;
		HeadMsg message = { i,i,MAX_SIZE,0,1,3 };		//���ͱ��ĳ�ʼ��
		memset(send_buf, 0, bufferNum);
		memcpy(send_buf, &message, sizeof(message));

		//ȷ�����ݰ�λ�� ���� ȷ��ʧ��λ��
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
			//���ݰ����һ���֣�����ʱ����ACK=4
			else{
				tlen = length - reslen1;
				message.datasize = (u_short)tlen;
				message.type = 4;						//���ն��˳�
				memcpy(send_buf + sizeof(message), buffer + reslen1, message.datasize);
				resflag = 1;
			}
		}
		//����У���
		memcpy(send_buf, &message, sizeof(message));
		message.checksum = Checksum((u_short*)(send_buf), sizeof(message) + message.datasize);
		memcpy(send_buf, &message, sizeof(message));

		//���ݷ���
		int res = sendto(socketClient, send_buf, bufferNum, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
		if (res == SOCKET_ERROR){
			cout << "����ʧ��" << endl;
			continue;
		}
		pthread_mutex_lock(&mutex);
		cout << "--> [ Resend Client ] seq:" << message.SEQ << " ack:" << message.ack << " ACK:" << message.type;
		cout << " check:" << message.checksum << " length:" << message.datasize << endl;
		pthread_mutex_unlock(&mutex);
	}
	//Ҫ���͵���һ�����к�Ϊʧ��λ��+1
	nextSeq = dislocation + 1;
	//��ʼ��ʱ
	start = clock();
	return 0;
}


DWORD WINAPI Timer(LPVOID lpParameter){
	//ȷ����Ϣ����
	while (1){
		//������ʱ
		clock_t finish = clock();
		if (finish - start > MAX_TIME){
			pthread_mutex_lock(&mutex);
			cout << "==========================   Timeout  ===========================" << endl;
			pthread_mutex_unlock(&mutex);

			//��ʱδ�յ��µ�ACK������������״̬
			ssthresh = cwnd / 2;//��ֵ��Ϊ���ڴ�С��һ��
			cwnd = 1;			//���ڴ�С��Ϊ1
			dupAck = 0;			//�����ظ�ack����
			if (renostate != 0)	//��ʱ�����������״̬
				renostate = 0;
			send_package(buffer);
		}

		//�߳̽�������
		if ((base == nextSeq && nextSeq == packnum) || timeflag == 1){
			break;
		}
	}
	return 0;
}

DWORD WINAPI RevThread(LPVOID lpParameter){
	char* recv_buf = new char[bufferNum];

	//ȷ����Ϣ����
	while (1){
		memset(recv_buf, 0, bufferNum);
		int res = recvfrom(socketClient, recv_buf, bufferNum, 0, (SOCKADDR*)&ClientAddr, &len);
		HeadMsg recv_Msg;
		memcpy(&recv_Msg, recv_buf, sizeof(recv_Msg));
		recv_Msg.checksum = Checksum((u_short*)(recv_buf), sizeof(recv_Msg));

		/*
		��ʼ״̬��constatus = 0, �������׶�
		��ʼ�����ڴ�СNΪ1����ֵssthresh = 10��
		ÿ�յ�һ��ACK, ���ڴ�С��1����ʱ���ڴ�С��ָ����������
		���������������ڻ��ߵ�����ֵssthreshʱ������ӵ������׶Σ�
		����ʱû���յ��µ�ACK, ����ֵ��Ϊ���ڴ�С��һ�룬���ڴ�С��Ϊ1�����½����������׶Σ����ش�δȷ�ϵ����ݰ���
		���յ�����ACK, ��ʼ�������ۼ��յ�3���ظ�ACKʱ����������ش��׶Σ���ֵ��Ϊ���ڴ�С��һ�룬��
		�ڴ�С����Ϊ��ֵ��3��������ٻָ��׶Ρ����յ�����ACKʱ��cwnd����

		constatus=1, ӵ������׶�
		ÿ�յ�һ��ACK,���ڴ�СN����1/���ڴ�С,��ʱ���ڴ�С���������ӣ�
		����ʱû���յ��µ�ACK,����ֵ��Ϊ���ڴ�С��һ�룬���ڴ�С��Ϊ1�����½����������׶Σ�
		���յ�����ACK,��ʼ�������ۼ��յ�3���ظ�ACKʱ����������ش��׶Σ���ֵ��Ϊ���ڴ�С��һ�룬��
		�ڴ�С����Ϊ��ֵ��3��������ٻָ��׶Ρ�

		constatus=2, ���ٻָ��׶�
		ÿ�յ�һ������ACK,���ڴ�С��1���������������ڻ��ߵ�����ֵssthreshʱ������ӵ������׶Σ�
		���յ������ظ� ACK ʱ��������ٻָ��׶Σ�
		ÿ�յ�һ���ظ� ACK��cwnd �� 1��
		���ֳ�ʱ֮������������׶Σ�
		�յ��µ� ACK ����ӵ������׶�
		*/

		int flag = 0;
		//��������󣬸���base
		if (recv_Msg.tag == 3 && (recv_Msg.type == 1 || recv_Msg.type == 4)) {
			//��Ϣ��������
			if (base <= recv_Msg.ack - 1) {
				if (recv_Msg.SEQ == packnum - 1) {
					flag = 0;
				}
				//	����������״̬
				if (renostate == 0) {
					cwnd++;		//���ڼ�1
					dupAck = 0;	//��������ack����
					base++;
					//	���ڴ�С������ֵ�������ӵ������״̬
					if (cwnd >= ssthresh) {
						renostate = 1;//ӵ������״̬
					}
					pthread_mutex_lock(&mutex);//�޸�1
					cout << "[ ��״̬˳������ ] ��ʱ����λ�� " << base << " ��һ�����͵����ݰ����к�Ϊ " << tmpSeq;
					cout << " ���ڴ�СΪ " << cwnd << " ������ֵΪ " << ssthresh << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
				}
				//	����ӵ������״̬
				else if (renostate == 1) {
					base++;
					cwnd += 1.0 / cwnd;			//���ڴ�СN����1 / ���ڴ�С
					dupAck = 0;					//��������ack����
					pthread_mutex_lock(&mutex);
					cout << "[ ӵ������״̬˳������ ] ��ʱ����λ�� " << base << "��һ�����͵����ݰ����к�Ϊ " << tmpSeq;
					cout << " ���ڴ�СΪ " << cwnd << " ������ֵΪ " << ssthresh << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
				}
				//	���ڿ��ٻָ�״̬ ���� �յ��µ�ACK��ת��ӵ������״̬
				else if (renostate == 2){
					base++;
					dupAck = 0;			//��������ack����
					cwnd = ssthresh;	//���ڴ�С����Ϊ��ֵ
					renostate = 1;		//����ӵ������״̬
					pthread_mutex_lock(&mutex);
					cout << "[ ���ٻָ�״̬˳������ ] ��ʱ����λ�� " << base << "��һ�����͵����ݰ����к�Ϊ " << tmpSeq;
					cout << " ���ڴ�СΪ " << int(cwnd) << " ������ֵΪ " << ssthresh << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
					Sleep(2);
				}
				start = clock();

			}
			//������Ϣ������
			else{
				if (recv_Msg.SEQ == packnum - 1){
					flag = 1;
				}
				//	���ڿ��ٻָ�״̬ ���� �յ�����ACK
				if (renostate == 2){
					cwnd += 1;	// ���ڴ�С��1
					pthread_mutex_lock(&mutex);
					cout << "[ ���ٻָ��׶��յ�����ack ]" << "��ʱ���ڴ�СΪ " << cwnd << " ������ֵΪ " << ssthresh << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
				}
				else{
					dis_Location = recv_Msg.SEQ;	//��¼ʧ��λ��
					dupAck++;						//�ظ�ACK����++
					pthread_mutex_lock(&mutex);
					cout << "# ���յ��ظ�ack����Ϊ: " << dupAck << endl;
					cout << "<-- [ Server ] seq:" << recv_Msg.SEQ << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.type;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.datasize << endl;
					pthread_mutex_unlock(&mutex);
					//	���ظ�ACK����3�Σ�������ٻָ��׶�
					if (dupAck == 3){
						ssthresh = cwnd / 2;	//��ֵ��Ϊ���ڴ�С��һ��
						cwnd = ssthresh + 3;	//���ڴ�С����Ϊ��ֵ��3
						cout << "[ �յ��ظ�ACK3�� ] ���ڴ�СΪ " << int(cwnd) << " ������ֵΪ " << ssthresh << endl;
						renostate = 2;			//���ٻָ��׶�
						send_package(buffer);
						Sleep(20);
					}
				}
			}
			if (dupAck >= 3){
				renostate = 2;					//���ٻָ��׶�
				Sleep(20);
			}
		}
		//�߳̽�������
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
	// �Զ����Ʒ�ʽ���ļ�
	ifstream fin(filename, ifstream::in | ios::binary);
	if (!fin) {
		cout << "# fOpen Error" << endl;
		return 0;
	}
	fin.seekg(0, fin.end);		// ָ�붨λ���ļ�������
	length = fin.tellg();		// ��ȡ�ļ���С���ֽڣ�
	fin.seekg(0, fin.beg);		// ָ�붨λ���ļ�ͷ
	buffer = new char[length];
	memset(buffer, 0, length);
	fin.read(buffer, length);	// ��ȡ�ļ����ݵ�������buffer��
	filelen = length;
	fin.close();//�ر��ļ�

	char* send_buf = new char[bufferNum];
	char* recv_buf = new char[bufferNum];
	u_short first_Seq = 0;		//��ʼ���к�

	//��������
	packnum = (length % MAX_SIZE == 0) ? length / MAX_SIZE : length / MAX_SIZE + 1;
	//�ļ��ֶ�ѭ������
	while (1){
		//����δ��
		if (nextSeq < base + cwnd && nextSeq < packnum){
			//�õ����͵����ݶ�
			HeadMsg header = { nextSeq,nextSeq,MAX_SIZE,0,1,3 };//���ͱ��ĳ�ʼ��
			memset(send_buf, 0, bufferNum);
			memcpy(send_buf, &header, sizeof(header));
			tmpSeq = nextSeq;

			//���ݰ��з�
			//���ݰ����Ȳ��������ֵ
			reslen = nextSeq * MAX_SIZE;
			if (length <= MAX_SIZE){
				header.datasize = (u_short)length;
				memcpy(send_buf + sizeof(header), buffer, length);
			}
			//�����з�
			else if ((length - reslen) > MAX_SIZE){
				memcpy(send_buf + sizeof(header), buffer + reslen, header.datasize);
				//reslen += (int)mess.mlength;
			}
			//���ݰ����һ���֣�����ʱ����ACK=4
			else{
				tlen = length - reslen;
				header.datasize = (u_short)tlen;
				header.type = 4;//���ն��˳�
				memcpy(send_buf + sizeof(header), buffer + reslen, header.datasize);
				resflag = 1;
			}

			//����У���
			memcpy(send_buf, &header, sizeof(header));
			header.checksum = Checksum((u_short*)(send_buf), sizeof(header) + header.datasize);
			memcpy(send_buf, &header, sizeof(header));
			if (base == nextSeq){
				start = clock();
			}
			//���ݷ���
			int res = sendto(socketClient, send_buf, bufferNum, 0, (SOCKADDR*)&ServerAddr, sizeof(SOCKADDR));
			nextSeq++;//��һ���ݰ�����
			if (res == SOCKET_ERROR){
				cout << "����ʧ��" << endl;
				continue;
			}
			pthread_mutex_lock(&mutex);
			//cout << "-->���ݴ���ɹ�" << endl;
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
	//��ʼ���׽��ֿ�
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
			send("C:\\Users\\Lenovo\\Desktop\\ʵ��3-3\\Client\\Debug\\1.jpg");
			break;
		case 2:
			send("C:\\Users\\Lenovo\\Desktop\\ʵ��3-3\\Client\\Debug\\2.jpg");
			break;
		case 3:
			send("C:\\Users\\Lenovo\\Desktop\\ʵ��3-3\\Client\\Debug\\3.jpg");
			break;
		case 4:
			send("C:\\Users\\Lenovo\\Desktop\\ʵ��3-3\\Client\\Debug\\helloworld.txt");
			break;
		default:
			cout << "����" << endl;
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