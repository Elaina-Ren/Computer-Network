#include<iostream>  
#include<winsock2.h>  
#include<string.h>  
#pragma comment(lib, "ws2_32.lib")  
#pragma warning(disable:4996)

using namespace std;

//########################定义客户端结构体######################
typedef struct CLIENT
{
    SOCKET client;//客户端套接字
    char username[16];//客户名
    char buf[256];//消息拼接缓存
    int flag;//当前客户端标号
    char now_time[32];
}CLIENT;

//######################定义聊天室最多人数#######################
CLIENT chat[10];

//当前客户端的标记
int num = 0;

//#########################时间输出函数#############################
string getTime() {
    //获取系统时间
    time_t timep;
    time(&timep);
    char tmp[64];
    strftime(tmp, sizeof(tmp), "%Y-%m-%d %H:%M:%S", localtime(&timep));
    return tmp;
}

//#########################接收消息并转发###########################
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
        //接收数据
        memset(revData, 0, sizeof(revData));
        int recv_lent = recv(Client->client, recv_time, 32, 0);
        int ret = recv(Client->client, revData, 128, 0);
        if (ret > 0)
        {
            //将用户名、“：”和消息拼接成一个字符串
            strcpy(Client->buf, recv_time);
            strcat(Client->buf, tmp);
            strcat(Client->buf, Client->username);
            strcat(Client->buf, temp);
            strcat(Client->buf, revData);
            //cout << Client->buf << endl;
            //在服务器端打印消息
            printf(Client->buf);
            printf("\n");
            //给除自己外的其他客户端发送数据
            for (i = 0; i < num; i++)
            {
                if (i != Client->flag)
                    send(chat[i].client, Client->buf, strlen(Client->buf), 0);
            }
        }
    }
    closesocket(Client->client);
    printf("线程号%d: 我结束了\n", GetCurrentThreadId());
    return 0;
}

//###########################主函数###########################

int main(int argc, char* argv[])
{
    //winsock初始化
    WORD sockVersion = MAKEWORD(2, 2);
    WSADATA wsaData;
    if (WSAStartup(sockVersion, &wsaData) != 0)
    {
        cout << "#############初始化套接字库失败！################" << endl;
        return 0;
    }
    //socket()
    SOCKET Srv = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (Srv == INVALID_SOCKET)
    {
        cout << "套接字创建失败";
        WSACleanup();
        return 0;
    }

    //将 localhost:49711 绑定到刚刚创建地socket
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
    //开始监听
    listen(Srv, 2);

    //等待来自客户端的连接请求
    sockaddr_in ClientAddr;
    int len = sizeof(ClientAddr);
    cout << "############线程号" << GetCurrentThreadId() << ": 初始化完成，服务器等待连接...###################\n";
    cout << "======================================================================" << endl;
    cout << "====================================================" << endl;
    cout << "###                 葱姜蒜三联盟                 ###" << endl;
    cout << "====================================================" << endl;
    while (1)
    {
        chat[num].client = accept(Srv, (SOCKADDR*)&ClientAddr, &len);//接受客户端
        //cout << endl;
        //cout << "等待连接......" << endl;
        if (chat[num].client == SOCKET_ERROR)
        {
            cout << "客户端发出请求，服务器接收请求失败" << endl << WSAGetLastError();
            /*closesocket(serversocket);
            WSACleanup();*/
            return 0;
        }
        chat[num].flag = num;
        recv(chat[num].client, chat[num].username, sizeof(chat[num].username), 0); //接收用户名


        cout << "【" << chat[num].username << "与服务器建立连接成功" << "】" << endl;

        DWORD  dwThreadId;
        HANDLE  hThread;

        //创建线程处理消息接收与转发              
        hThread = CreateThread(NULL, NULL, (LPTHREAD_START_ROUTINE)AnswerThread, &chat[num], 0, &dwThreadId);
        if (hThread == NULL)
        {
            printf("线程号%d: 创建线程出错\n", GetCurrentThreadId());
        }
        else
        {
            printf("%s上线\n", chat[num].username);
            CloseHandle(hThread);
        }
        //标记加一
        num++;
    }

    //closesocket()
    closesocket(Srv);

    //winsock注销
    WSACleanup();
    return 0;
}
