#include <iostream>
#include <fstream>
#include <WinSock2.h>
#include <string>
#include <time.h>
#pragma pack(1)			//�������ݰ�1Byte���룬���û������ָ�����4Byte���룬����u_short���ͻ���2B����Ϣ��2B���㣬�������ת����char*��ʽ
#pragma comment(lib,"ws2_32.lib")
#pragma warning(disable:4996);
using namespace std;

//�������ȫ�ֱ���
const int MAXSIZE = 4096;
const unsigned char SYN = 0x1; //SYN = 1 ACK = 0
const unsigned char ACK = 0x2;//SYN = 0, ACK = 1��FIN = 0
const unsigned char ACK_SYN = 0x3;//SYN = 1, ACK = 1
const unsigned char FIN = 0x4;//FIN = 1 ACK = 0
const unsigned char FIN_ACK = 0x5;
const unsigned char OVER = 0x7;//������־
double MAX_TIME = 0.5 * CLOCKS_PER_SEC;
char path[100];



//У��ͼ�����غ���
u_short Checksum(u_short* message, int size) {
	int count = (size + 1) / 2;
	u_short* buf = (u_short*)malloc(size + 1);
	memset(buf, 0, size + 1);
	memcpy(buf, message, size);
	u_long sum = 0;
	while (count--) {
		sum += *buf++;
		if (sum & 0xFFFF0000) {
			sum &= 0xFFFF;
			sum++;
		}
	}
	return ~(sum & 0xFFFF);
}



//���ĸ�ʽ
struct HeadMsg {
	u_short datasize = 0;			// ���ݳ��ȣ�16λ
	u_short Checksum = 0;			// У��ͣ�16λ
	unsigned char type = 0;			// ��Ϣ����
	unsigned char seq = 0;			// ���кţ����Ա�ʾ0-255
};



//�ͻ�������������غ���
int Hand_shake(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen) {
	HeadMsg header;
	char* sendbuf = new char[sizeof(header)];
	u_short sum;

	//��һ������
	header.type = SYN;
	header.Checksum = Checksum((u_short*)&header, sizeof(header));
	memcpy(sendbuf, &header, sizeof(header));

	//�ж��Ƿ�ɹ�����
	if (sendto(socketClient, sendbuf, sizeof(header), 0, (SOCKADDR*)&servAddr, servAddrlen) == SOCKET_ERROR) {
		return -1;
	}
	else {
		cout << "# Client: [SYN] Seq = 0" << endl;
	}
	//��¼��һ�����ֵķ���ʱ�䣬�ж��Ƿ�ʱ
	clock_t start = clock();
	//��������ģʽ
	u_long mode = 1;
	ioctlsocket(socketClient, FIONBIO, &mode);


	//�ڶ�������
	while (recvfrom(socketClient, sendbuf, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
		//�жϵ�һ���Ƿ�ʱ --- ��ʱ���ش�
		if (clock() - start > MAX_TIME) {
			header.type = SYN;
			header.Checksum = Checksum((u_short*)&header, sizeof(header));
			memcpy(sendbuf, &header, sizeof(header));
			sendto(socketClient, sendbuf, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
			start = clock();
			cout << "��һ�����ֳ�ʱ�����ڽ����ش�" << endl;
		}
	}

	//У��ͼ���
	memcpy(&header, sendbuf, sizeof(header));
	if (header.type == ACK && Checksum((u_short*)&header, sizeof(header) == 0)) {
		cout << "# Server: [SYN, ACK] Seq = 0 Ack = 1" << endl;
	}
	else {
		cout << "���Ӵ���" << endl;
		return -1;
	}

	//����������
	header.type = ACK_SYN;
	header.Checksum = 0;
	header.Checksum = Checksum((u_short*)&header, sizeof(header));
	if (sendto(socketClient, sendbuf, sizeof(header), 0, (SOCKADDR*)&servAddr, servAddrlen) == SOCKET_ERROR) {
		return -1;
	}
	else {
		cout << "# Client: [ACK] Seq = 1 Ack = 1" << endl;
		cout << "======================= CONNECT IN SUCCESS =======================" << endl;
	}
	return 1;
}

//�����ļ���غ���
void send_package(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len, int& order)
{
	HeadMsg header;
	char* buffer = new char[MAXSIZE + sizeof(header)];
	header.datasize = len;
	header.seq = unsigned char(order);//���к�
	memcpy(buffer, &header, sizeof(header));
	memcpy(buffer + sizeof(header), message, sizeof(header) + len);
	header.Checksum = Checksum((u_short*)buffer, sizeof(header) + len);
	memcpy(buffer, &header, sizeof(header));
	sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//����
	cout << "<-- Send to Server " << len << " bytes!" << " Flag:" << int(header.type) << " SEQ:" << int(header.seq) << " Checksum:" << int(header.Checksum) << endl;
	clock_t start = clock();//��¼����ʱ��
	//����ack����Ϣ
	while (1) {
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		while (recvfrom(socketClient, buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
			if (clock() - start > MAX_TIME) {
				header.datasize = len;
				header.seq = u_char(order);//���к�
				header.type = u_char(0x0);
				memcpy(buffer, &header, sizeof(header));
				memcpy(buffer + sizeof(header), message, sizeof(header) + len);
				header.Checksum = Checksum((u_short*)buffer, sizeof(header) + len);//����У���
				memcpy(buffer, &header, sizeof(header));
				sendto(socketClient, buffer, len + sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);//����
				cout << "--> Reply to Client " << len << " bytes! Flag:" << int(header.type) << " SEQ:" << int(header.seq) << endl;
				clock_t start = clock();//��¼����ʱ��
			}
		}
		memcpy(&header, buffer, sizeof(header));//���������յ���Ϣ����ȡ
		u_short check = Checksum((u_short*)&header, sizeof(header));
		if (header.seq == u_short(order) && header.type == ACK) {
			cout << "Send has been confirmed! Flag:" << int(header.type) << " SEQ:" << int(header.seq) << endl;
			break;
		}
		else {
			continue;
		}
	}
	u_long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);//�Ļ�����ģʽ
}

void send(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen, char* message, int len)
{
	int packagenum = len / MAXSIZE + (len % MAXSIZE != 0);
	int seqnum = 0;
	for (int i = 0; i < packagenum; i++) {
		send_package(socketClient, servAddr, servAddrlen, message + i * MAXSIZE, i == packagenum - 1 ? len - (packagenum - 1) * MAXSIZE : MAXSIZE, seqnum);
		seqnum = (seqnum + 1) % 256;
	}
	//���ͽ�����Ϣ
	HeadMsg header;
	char* Buffer = new char[sizeof(header)];
	header.type = OVER;
	header.Checksum = Checksum((u_short*)&header, sizeof(header));
	memcpy(Buffer, &header, sizeof(header));
	sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
	cout << "# Send End!" << endl;
	clock_t start = clock();
	while (1) {
		u_long mode = 1;
		ioctlsocket(socketClient, FIONBIO, &mode);
		while (recvfrom(socketClient, Buffer, MAXSIZE, 0, (sockaddr*)&servAddr, &servAddrlen) <= 0) {
			if (clock() - start > MAX_TIME) {
				char* Buffer = new char[sizeof(header)];
				header.type = OVER;
				header.Checksum = Checksum((u_short*)&header, sizeof(header));
				memcpy(Buffer, &header, sizeof(header));
				sendto(socketClient, Buffer, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
				cout << "# Time Out! ReSend End!" << endl;
				start = clock();
			}
		}
		memcpy(&header, Buffer, sizeof(header));//���������յ���Ϣ����ȡ
		u_short check = Checksum((u_short*)&header, sizeof(header));
		if (header.type == OVER) {
			cout << "=======================�Է��ѳɹ������ļ�!=======================" << endl;
			break;
		}
		else {
			continue;
		}
	}
	u_long mode = 0;
	ioctlsocket(socketClient, FIONBIO, &mode);//�Ļ�����ģʽ
}



int Hand_wave(SOCKET& socketClient, SOCKADDR_IN& servAddr, int& servAddrlen) {
	HeadMsg header;
	char* buf = new char[sizeof(header)];
	u_short sum;

	//����
	header.type = FIN;
	header.Checksum = Checksum((u_short*)&header, sizeof(header));
	memcpy(buf, &header, sizeof(header));
	//�ж��Ƿ�ɹ�����
	if (sendto(socketClient, buf, sizeof(header), 0, (SOCKADDR*)&servAddr, servAddrlen) == SOCKET_ERROR) {
		return -1;
	}
	else {
		cout << "# Client: [FIN, ACK] Fin: Set|Ack: Set" << endl;
	}
	//��¼��һ�λ��ֵķ���ʱ�䣬�ж��Ƿ�ʱ
	clock_t start = clock();
	//��������ģʽ
	u_long mode = 1;
	ioctlsocket(socketClient, FIONBIO, &mode);


	//�ȴ�����
	while (1) {
		if (recvfrom(socketClient, buf, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) > 0) {
			memcpy(&header, buf, sizeof(header));
			if (header.type == ACK) {
				cout << "# Server: [ACK] Fin: Not Set|Ack: Set" << endl;
				break;
			}
			else {
				return -1;
				break;
			}
		}
		else {
			if (clock() - start > MAX_TIME) {
				header.type = FIN;
				header.Checksum = Checksum((u_short*)&header, sizeof(header));		//����У���
				memcpy(buf, &header, sizeof(header));
				sendto(socketClient, buf, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
				start = clock();
				cout << "��һ�λ��ֳ�ʱ�����ڽ����ش�" << endl;
			}
		}
	}
	

	//����
	header.type = FIN_ACK;
	header.Checksum = Checksum((u_short*)&header, sizeof(header));
	memcpy(buf, &header, sizeof(header));
		//�ж��Ƿ�ɹ�����
	if (sendto(socketClient, buf, sizeof(header), 0, (SOCKADDR*)&servAddr, servAddrlen) == SOCKET_ERROR) {
		return -1;
	}
	else {
		cout << "# Server: [FIN, ACK] Fin: Set|Ack: Set" << endl;
	}
		//��¼�����λ��ֵķ���ʱ�䣬�ж��Ƿ�ʱ
	start = clock();


	//���Ĵλ���
	while (1) {
		if (recvfrom(socketClient, buf, sizeof(header), 0, (sockaddr*)&servAddr, &servAddrlen) > 0) {
			cout << "# Client: [ACK] Fin: Not Set|Ack: Set" << endl;
			break;
		}
		else {
			if (clock() - start > MAX_TIME) {
				header.type = FIN;
				header.Checksum = Checksum((u_short*)&header, sizeof(header));		//����У���
				memcpy(buf, &header, sizeof(header));
				sendto(socketClient, buf, sizeof(header), 0, (sockaddr*)&servAddr, servAddrlen);
				start = clock();
				break;
				cout << "�����λ��ֳ�ʱ�����ڽ����ش�" << endl;
			}
		}
	}
	return 1;
}


void initial()
{
	//��ű�WSAStartup�������ú󷵻ص�Windows Sockets���ݵ����ݽṹ
	WSADATA wsadata;
	WORD edi = MAKEWORD(2, 2);//ʹ��socket2.2�汾  
	int err;
	err = WSAStartup(edi, &wsadata);
	if (err != 0) {
		cout << "# WSAStartup ERROR =" << WSAGetLastError() << endl;
	}
	else {
		cout << "# WSAStartUp						Success" << endl;
	}
	return;
}


int main() {
	cout << "  /$$$$$$    /$$       /$$$$$$   /$$$$$$$$  /$$    /$$ /$$$$$$$$" << endl;
	cout << " / $$__ $$  | $$      | _  $$_/ | $$_____/ | $$$  | $$| __  $$__ /" << endl;
	cout << " | $$   __/ | $$        | $$    | $$       | $$$$ | $$    | $$" << endl;
	cout << " | $$       | $$        | $$    | $$$$$    | $$ $$  $$    | $$" << endl;
	cout << " | $$       | $$        | $$    | $$__/    | $$  $$ $$    | $$" << endl;
	cout << " | $$    $$ | $$        | $$    | $$       | $$    $$$    | $$" << endl;
	cout << " | $$$$$$/  | $$$$$$$$ / $$$$$$ | $$$$$$$$ | $$     $$    | $$" << endl;
	cout << "  ______/   |________/|______/  |________/ |__ /   __/    |__ / " << endl;
	cout << "=================================================================" << endl;

	//��ʼ������
	initial();

	//����SOCKET ȷ��Э�� --- server
	SOCKADDR_IN server_addr;
	SOCKET server;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(49711);
	server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
	//server_addr.sin_addr.s_addr = htonl(2130706433);

	server = socket(AF_INET, SOCK_DGRAM, 0);
	if (server == INVALID_SOCKET) {
		cout << "# SOCKET EORROR = " << WSAGetLastError() << endl;
		return -1;
	}
	else
		cout << "# SocketInit						Success" << endl;

	cout << "==================================================================" << endl;

	int len = sizeof(server_addr);
	//��������
	Hand_shake(server, server_addr, len);

	string filename;
	cout << "========================= Input File name ========================" << endl;
	cin >> filename;
	ifstream fin(filename.c_str(), ifstream::binary);//�Զ����Ʒ�ʽ���ļ�
	char* buffer = new char[10000000];
	int index = 0;
	unsigned char temp = fin.get();
	while (fin) {
		buffer[index++] = temp;
		temp = fin.get();
	}
	fin.close();
	send(server, server_addr, len, (char*)(filename.c_str()), filename.length());
	clock_t start = clock();
	send(server, server_addr, len, buffer, index);
	clock_t end = clock();
	cout << "# Time:            " << (end - start) / CLOCKS_PER_SEC << " seconds" << endl;
	cout << "# Rate:            " << ((float)index) / ((end - start) / CLOCKS_PER_SEC) << " bytes/s" << endl;
	cout << "# Service Closed!" << endl;
	cout << "=================================================================" << endl;

	Hand_wave(server, server_addr, len);
	system("pause");
	return 0;

}