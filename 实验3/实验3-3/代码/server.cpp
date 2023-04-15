#include "windows.h"
#include "winsock.h"  //winsock相关函数头文件
#include "string.h"
#include<time.h>
#include<iostream>
#include<fstream>
#pragma comment(lib,"ws2_32.lib")  //winsock链接库文件
using namespace std;

SOCKET socketServer, socketClient;
SOCKADDR_IN ServerAddr, ClientAddr;

const int bufnum = 5000;
const int window = 10;
float lossnum = 0.001;

struct HeadMsg{
	u_short ack;		//确认序号
	u_short seq;		//序号
	u_short mlength;	//数据长度
	u_short checksum;	//校验和
	u_short tag_ack;	//ACK标志位
	u_short tag;		//标识：SYN=1;FIN=2;PSH=3
};

int Init(){
	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		cout << "# WSAStartup ERROR =" << WSAGetLastError() << endl;
		return 0;
	}
	else 
		cout << "# WSAStartUp						Success" << endl;

	socketServer = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	socketClient = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

	if (socketServer == INVALID_SOCKET) {
		cout << "# SOCKET EORROR = " << WSAGetLastError() << endl;
		WSACleanup();
		return 0;
	}
	else
		cout << "# SocketInit						Success" << endl;

	//绑定本地地址到socket
	ServerAddr.sin_family = AF_INET;
	ServerAddr.sin_port = htons(12888);//指定端口号
	ServerAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.2");
	
	ClientAddr.sin_family = AF_INET;//指定IP格式 
	ClientAddr.sin_port = htons(8888);//指定端口号
	ClientAddr.sin_addr.S_un.S_addr = inet_addr("127.0.0.1");

	if (bind(socketServer, (LPSOCKADDR)&ServerAddr, sizeof(ServerAddr)) == SOCKET_ERROR) {
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
	u_long chesum = 0;
	while (count--) {
		chesum += *buf++;
		if (chesum & 0xffff0000) {
			chesum &= 0xffff;
			chesum++;
		}
	}
	return ~(chesum & 0xffff);
}

int Connect(){
	cout << "# Starting Connection" << endl;
	char* send_buf = new char[bufnum];
	char* recv_buf = new char[bufnum];
	int len = sizeof(SOCKADDR);
	HeadMsg header1;
	while (1){
		int res = recvfrom(socketServer, recv_buf, bufnum, 0, (SOCKADDR*)&ClientAddr, &len);
		memcpy(&header1, recv_buf, sizeof(header1));
		//cout << "# Checksum : " << Checksum((u_short*)(recv_buf), sizeof(mes2)) << endl;
		if (Checksum((u_short*)(recv_buf), sizeof(header1)) != 0){
			cout << "|| 校验和错误" << endl;
			continue;
		}
		if (res == SOCKET_ERROR){
			cout << "SENDTO ERROR = " << WSAGetLastError() << endl;
			Sleep(2000);
			continue;
		}
		//	判断SYN=1
		if (header1.tag == 1){
			//cout << "Client:SYN=1 seq=0 ack=0" << endl;
			u_short tmpack = header1.seq + 1;
			HeadMsg send_Msg = { tmpack,0,0,0,1,1 };
			memset(send_buf, 0, bufnum);
			memcpy(send_buf, &send_Msg, sizeof(send_Msg));
			send_Msg.checksum = Checksum((u_short*)(send_buf), sizeof(send_Msg));
			memset(send_buf, 0, bufnum);
			memcpy(send_buf, &send_Msg, sizeof(send_Msg));
			int res = sendto(socketServer, send_buf, bufnum, 0, (SOCKADDR*)&ClientAddr, sizeof(SOCKADDR));
			if (res == SOCKET_ERROR){
				cout << "发送失败" << endl;
				return 0;
			}
			else{
				cout << "# ConnectionEstablishment				Success" << endl;
				cout << "=================================================================" << endl;
				return 1;
			}
		}
		else{
			cout << "连接失败" << endl;
			return 0;
		}
	}
	delete[] send_buf;
	delete[] recv_buf;
	return 0;
}

int tryflag = 0;
int disConnect(){
	char* send_buf = new char[bufnum];
	char* recv_buf = new char[bufnum];
	int len = sizeof(SOCKADDR);
	HeadMsg header2;
	while (1){
		int res = recvfrom(socketServer, recv_buf, bufnum, 0, (SOCKADDR*)&ClientAddr, &len);
		memcpy(&header2, recv_buf, sizeof(header2));
		if (header2.tag == 2 && header2.tag_ack == 0){
			u_short tmpack = header2.seq + 1;
			HeadMsg send_Msg = { tmpack,0,0,0,1,2 };
			memset(send_buf, 0, bufnum);
			memcpy(send_buf, &send_Msg, sizeof(send_Msg));
			send_Msg.checksum = Checksum((u_short*)(send_buf), sizeof(send_Msg));
			memset(send_buf, 0, bufnum);
			memcpy(send_buf, &send_Msg, sizeof(send_Msg));
			int res = sendto(socketServer, send_buf, bufnum, 0, (SOCKADDR*)&ClientAddr, sizeof(SOCKADDR));
			if (res == SOCKET_ERROR){
				cout << "发送失败" << endl;
				return 0;
			}
			else{
				cout << "# Service Closed!" << endl;
				cout << "=================================================================" << endl;
				return 1;
			}
		}
		else{
			cout << "连接失败" << endl;
			return 0;
		}
	}
	delete[] send_buf;
	delete[] recv_buf;
	return 0;
}

//丢包程序
BOOL lossInLossRatio(float lossRatio) {
	int lossBound = (int)(lossRatio * 100);
	int r = rand() % 101;
	if (r <= lossBound) {
		cout << "=======================  # Packet Loss  =========================" << endl;	
		return true;
	}
	return false;
}


int Receive(string file1){
	cout << "# Starting sending message" << endl;
	char* send_buf = new char[bufnum];
	char* recv_buf = new char[bufnum];
	char* buffer = new char[11968994];//1857353 1655808 5898505
	int len = sizeof(SOCKADDR);
	HeadMsg mess;			//消息发送
	u_short tseq = 0;		//当前seq
	u_short nextSeq = 0;	//下一个期望的seq
	int sumlen = 0;
	while (1){
		HeadMsg recv_Msg;	//消息接收
		memset(recv_buf, 0, bufnum);
		int res = recvfrom(socketServer, recv_buf, bufnum, 0, (SOCKADDR*)&ClientAddr, &len);
		//模拟丢包
		BOOL b = lossInLossRatio(lossnum);
		if (b){
			continue;
		}
		if (res == SOCKET_ERROR){
			cout << "|| 接收失败" << endl;
			Sleep(20);
			continue;
		}
		memcpy(&recv_Msg, recv_buf, sizeof(recv_Msg));
		recv_Msg.checksum = Checksum((u_short*)(recv_buf), sizeof(recv_Msg) + recv_Msg.mlength);

		//校验错误
		if (recv_Msg.checksum != 0){
			cout << "|| Checksum Error" << endl;
			break;
		}

		//校验成功
		if (recv_Msg.tag == 3){
			if (recv_Msg.ack == nextSeq){
				cout << "<-- [ Client ] seq:" << recv_Msg.seq << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.tag_ack;
				cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.mlength << endl;
				u_short tmpack = recv_Msg.ack + 1;
				HeadMsg mes_send = { tmpack,nextSeq,0,0,recv_Msg.tag_ack,3 };

				memset(send_buf, 0, bufnum);
				memcpy(send_buf, &mes_send, sizeof(mes_send));
				mes_send.checksum = Checksum((u_short*)(send_buf), sizeof(mes_send));
				memset(send_buf, 0, bufnum);
				memcpy(send_buf, &mes_send, sizeof(mes_send));
				nextSeq = recv_Msg.ack + 1;

				//向文件里写
				memcpy(buffer + sumlen, recv_buf + sizeof(recv_Msg), recv_Msg.mlength);
				sumlen += (int)recv_Msg.mlength;
				int res = sendto(socketServer, send_buf, bufnum, 0, (SOCKADDR*)&ClientAddr, sizeof(SOCKADDR));
				if (res == SOCKET_ERROR){
					cout << "发送失败" << endl;
					return 0;
				}
				else{
					//cout << "顺利接收回复成功!!!" << endl;
					if (recv_Msg.tag_ack == 4){
						cout << endl;
						cout << "# Package length: " << sumlen << endl;
						break;
					}
					continue;
				}
			}
			//失序
			else{
				u_short tmpack = nextSeq;
				HeadMsg mes_send = { nextSeq - 1,recv_Msg.seq,0,0,1,3 };

				memset(send_buf, 0, bufnum);
				memcpy(send_buf, &mes_send, sizeof(mes_send));
				mes_send.checksum = Checksum((u_short*)(send_buf), sizeof(mes_send));
				memset(send_buf, 0, bufnum);
				memcpy(send_buf, &mes_send, sizeof(mes_send));
				int res = sendto(socketServer, send_buf, bufnum, 0, (SOCKADDR*)&ClientAddr, sizeof(SOCKADDR));
				if (res == SOCKET_ERROR){
					cout << "发送失败" << endl;
					return 0;
				}
				else{
					cout << "# Resend Begin " << endl;
					cout << "<-- [ Client ] seq:" << recv_Msg.seq << " ack:" << recv_Msg.ack << " ACK:" << recv_Msg.tag_ack;
					cout << " check:" << recv_Msg.checksum << " length:" << recv_Msg.mlength << endl;
					cout << "--> [ Server ] seq:" << mes_send.seq << " ack:" << mes_send.ack << " ACK:" << mes_send.tag_ack;
					cout << " check:" << mes_send.checksum << " length:" << mes_send.mlength << endl;
					cout << "# Resend Over  " << endl;
					cout << endl;
					continue;
				}
			}
		}
	}

	// 以二进制方式写入文件
	ofstream fout(file1, ofstream::out | ios::binary);
	if (!fout) {
		cout << "Error: cannot open file!" << endl;
		return false;
	}
	fout.write(buffer, sumlen);
	fout.close();
	delete[] send_buf;
	delete[] recv_buf;
	return 0;
}



int main(){
	cout << " / $$$$$$   / $$$$$$$$ / $$$$$$$ / $$   / $$ / $$$$$$$$  / $$$$$$$" << endl;
	cout << "/ $$__  $$ | $$_____/ | $$__  $$ | $$   | $$ | $$_____/ | $$__  $$" << endl;
	cout << "| $$   __/ | $$       | $$    $$ | $$   | $$ | $$       | $$    $$" << endl;
	cout << "| $$$$$$   | $$$$$    | $$$$$$$/ |  $$ / $$/ | $$$$$    | $$$$$$$/" << endl;
	cout << " ____  $$  | $$__/    | $$__  $$     $$ $$/  | $$__/    | $$__  $$" << endl;
	cout << "/ $$    $$ | $$       | $$    $$      $$$/   | $$       | $$    $$" << endl;
	cout << "| $$$$$$/  | $$$$$$$$ | $$  | $$       $/    | $$$$$$$$ | $$  | $$" << endl;
	cout << " ______/   |________/ |__/  |__ /     _/     |________/ |__/  |__ /" << endl;
	cout << "==================================================================" << endl;
	cout << "Starting server service" << endl;
	//	初始化
	Init();

	//	传输数据
	if (Connect()){
		//Receive("C:\\Users\\Lenovo\\Desktop\\实验3-3\\Server\\Debug\\1.jpg");
		//Receive("C:\\Users\\Lenovo\\Desktop\\实验3-3\\Server\\Debug\\2.jpg");
		Receive("C:\\Users\\Lenovo\\Desktop\\实验3-3\\Server\\Debug\\3.jpg");
		//Receive("C:\\Users\\Lenovo\\Desktop\\实验3-3\\Server\\Debug\\helloworld.txt");
		disConnect();
	}
	
	Sleep(100);
	closesocket(socketServer);
	WSACleanup();
	system("pause");
	return 0;
}
