#include<iostream>  
#include<winsock2.h>  
#include<string.h>  
#pragma comment(lib, "ws2_32.lib")  
#pragma warning(disable:4996)

using namespace std;

//########################����ͻ��˽ṹ��######################
typedef struct CLIENT
{
    SOCKET client;//�ͻ����׽���
    char username[16];//�ͻ���
    char buf[256];//��Ϣƴ�ӻ���
    int flag;//��ǰ�ͻ��˱��
    char now_time[32];
}CLIENT;

//######################�����������������#######################
CLIENT chat[10];

//��ǰ�ͻ��˵ı��
int num = 0;

//#########################ʱ���������#############################
string getTime() {
    //��ȡϵͳʱ��
    time_t timep;
    time(&timep);
    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&timep));
    return tmp;
}

//#########################������Ϣ��ת��###########################
DWORD WINAPI AnswerThread(LPVOID lparam)
{
    CLIENT* Client = (CLIENT*)lparam;
    char revData[128];
    int i = 0;
    char temp[2] = ":";
    char tmp[2] = " ";
    char recv_time[32] = { NULL };
    while (1)
    {
        //��������
        memset(revData, 0, sizeof(revData));
        int recv_lent = recv(Client->client, recv_time, 32, 0);
        int ret = recv(Client->client, revData, 128, 0);
        if (ret > 0)
        {
            //���û���������������Ϣƴ�ӳ�һ���ַ���
            strcpy(Client->buf, recv_time);
            strcat(Client->buf, tmp);
            strcat(Client->buf, Client->username);
            strcat(Client->buf, temp);
            strcat(Client->buf, revData);
            //cout << Client->buf << endl;
            //�ڷ������˴�ӡ��Ϣ
            printf(Client->buf);
            printf("\n");
            //�����Լ���������ͻ��˷�������
            for (i = 0; i < num; i++)
            {
                if (i != Client->flag)
                    send(chat[i].client, Client->buf, strlen(Client->buf), 0);
            }
        }
    }
    closesocket(Client->client);
    printf("�̺߳�%d: �ҽ�����\n", GetCurrentThreadId());
    return 0;
}

//###########################������###########################

int main(int argc, char* argv[])
{
    //winsock��ʼ��
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0)
    {
        cout << "#############��ʼ���׽��ֿ�ʧ�ܣ�################" << endl;
        return 0;
    }
    //socket()
    SOCKET Srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Srv == INVALID_SOCKET)
    {
        cout << "�׽��ִ���ʧ��";
        WSACleanup();
        return 0;
    }

    //�� localhost:49711 �󶨵��ոմ�����socket
    sockaddr_in sin;
    sin.sin_family = AF_INET;
    sin.sin_port = htons(49711);
    sin.sin_addr.S_un.S_addr = INADDR_ANY;

    if (bind(Srv, (LPSOCKADDR)&sin, sizeof(sin)) == SOCKET_ERROR)
    {
        cout << "BIDING ERROR OCCURED" << endl;
        closesocket(Srv);
        WSACleanup();
        return -1;
    }
    //��ʼ����
    listen(Srv, 2);

    //�ȴ����Կͻ��˵���������
    sockaddr_in ClientAddr;
    int len = sizeof(ClientAddr);
    cout << "############�̺߳�" << GetCurrentThreadId() << ": ��ʼ����ɣ��������ȴ�����...###################\n";
    cout << "======================================================================" << endl;
    cout << "====================================================" << endl;
    cout << "###                 �н���������                 ###" << endl;
    cout << "====================================================" << endl;
    while (1)
    {
        chat[num].client = accept(Srv, (SOCKADDR*)&ClientAddr, &len);//���ܿͻ���
        //cout << endl;
        //cout << "�ȴ�����......" << endl;
        if (chat[num].client == SOCKET_ERROR)
        {
            cout << "�ͻ��˷������󣬷�������������ʧ��" << endl << WSAGetLastError();
            /*closesocket(serversocket);
            WSACleanup();*/
            return 0;
        }
        chat[num].flag = num;
        recv(chat[num].client, chat[num].username, sizeof(chat[num].username), 0); //�����û���


        cout << "��" << chat[num].username << "��������������ӳɹ�" << "��" << endl;

        DWORD  dwThreadId;
        HANDLE  hThread;

        //�����̴߳�����Ϣ������ת��              
        hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)AnswerThread, &chat[num], 0, &dwThreadId);
        if (hThread == NULL)
        {
            printf("�̺߳�%d: �����̳߳���\n", GetCurrentThreadId());
        }
        else
        {
            printf("%s����\n", chat[num].username);
            CloseHandle(hThread);
        }
        //��Ǽ�һ
        num++;
    }

    //closesocket()
    closesocket(Srv);

    //winsockע��
    WSACleanup();
    return 0;
}
