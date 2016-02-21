
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <assert.h>
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#define RECV 90
#define SEND 80
#define WRQ 60
#define RRQ 50
#define TIME_OUT 30
#define RETRANSMIT_PACKET 20

#define DATA_OP 3
#define ACK_OP 4
#define ERROR_OP 5
#define PORT 6900

#define ILLEGAL_PACKET 130 //v
#define RECV_ERROR 120 //v
#define DUPLICATE_PACKET 110 //v
#define ERR_WHEN_OPEN_FILE 40 //v
#define SYS_ERROR 10
#define ERR_MODE 15
#define FILE_NOT_FOUND 25


char data [516];		//data packet
char errorMsg [200];	//error packet
char ACK [4];			//Ack packet
char packetBuff[516];
char newTransfer [200];	//RRQ/WRT packet
unsigned int blockNum;			//block number (for send)
int fd;					//file descriptor for read/write
int sock;				//client's socket
int sock_1;			//socket for 2nd client (in case someone will try to connect)
int TIDPort;			//TID port ?? check what it means
int errnum;			//error number as described at the rfc783 protocol
int op;				// (RRQ) OR (WRQ), need for some funcion
fd_set sendSelect;		//select's struct
fd_set rcvSelect;		//select's struct
struct timeval timeout;	//timeout struct
struct sockaddr_in addr;
struct sockaddr_in client_adrr;
struct sockaddr_in addr_second;
int sockaddrLen;

void setErrMsg();
int getErrMsgSize();
int sendErrorMsg();
int closeServer();
void closeConnection();
int power(int a, int b);
int writeToFile(int size);
int readFromFile();
int openFile(char * fileName);
int getIntValue(char lsb , char msb);
int getRandomPort();
void setHeaderForDataPacket ();
int sendDataPackets(int size);
int sendFile();
void setACK();
int sendACK();
int recvOp();
int recvErrorMsg();
int recvACK();
int recvFile();
int setUDPConnection(struct sockaddr_in *addr,int port);
int selectFTP(int flag);
int checkFile(char * fileName);
int checkMode();



void setErrMsg(){
	memset(errorMsg, 0, 200);
	errorMsg[0]=0;
	errorMsg[1]=ERROR_OP;
	errorMsg[2]=0;
	errorMsg[3]= (errnum < 8 || errnum>0) ? errnum : 0;
	switch(errnum){
		case (1):
			sprintf(errorMsg+4, "%s", "File not found");
			errorMsg[4 + strlen("File not found")]=0;
			break;
		case (2):
				sprintf(errorMsg+4, "%s", "Access violation");
				errorMsg[4 + strlen("Access violation")]=0;
				break;
		case (3):
				sprintf(errorMsg+4, "%s", "Disk full or allocation exceeded");
				errorMsg[4 + strlen("Disk full or allocation exceeded")]=0;
				break;
			case (4):
				sprintf(errorMsg+4, "%s", "Illegal TFTP operation");
				errorMsg[4 + strlen("Illegal TFTP operation")]=0;
				break;
			case (5):
				sprintf(errorMsg+4, "%s", "Unknown transfer ID");
				errorMsg[4 + strlen("Unknown transfer ID")]=0;
				break;
			case (6):
				sprintf(errorMsg+4, "%s", "File already exists");
				errorMsg[4 + strlen("File already exists")]=0;
				break;
			case (7):
				sprintf(errorMsg+4, "%s", "No such user");
				errorMsg[4 + strlen("No such user")]=0;
				break;
		case (ERR_WHEN_OPEN_FILE):
				sprintf(errorMsg+4, "%s", "Some error was occurred during opening the file");
				errorMsg[4 + strlen("Some error was occurred during opening the file")]=0;
				break;
		case (ERR_MODE):
				sprintf(errorMsg+4, "%s", "Mode does not supported");
				errorMsg[4 + strlen("Mode does not supported")]=0;
				break;
		case (SYS_ERROR):
				sprintf(errorMsg+4, "%s", "Some error was occurred during run time");
				errorMsg[4 + strlen("Some error was occurred during run time")]=0;
				break;
		case (FILE_NOT_FOUND):
				sprintf(errorMsg+4, "%s", "File not found");
				errorMsg[4 + strlen("File not found")]=0;
				break;
		default :
				sprintf(errorMsg+4, "%s", "Unknown problem have occurred");
				errorMsg[4 + strlen("Unknown problem have occurred")]=0;
				break;
	}

}


int getErrMsgSize() {
	return 4 + strlen(errorMsg+4)+1;
}

int recvPacket(){
	int cnt=0;
	if((cnt=recvfrom(sock,packetBuff,516,0, (struct sockaddr *) &client_adrr, (socklen_t *) &sockaddrLen))<0){
		closeConnection();
		return -1;
	}
	return cnt;
}

void parseData(int offset, int len, char* buff){
	int i=0;
	for(i=0;i<len;i++){
		buff[i]=packetBuff[i+offset];
	}
}
int sendErrorMsg(){
	int state = selectFTP(SEND);
	switch (state){
		case (-1): // error
			closeServer(); // exit
			return -1;
		case (0) : // time out
			closeConnection();
			return 0;
		case (1) :
			setErrMsg();
			//send(sock, errorMsg, getErrMsgSize(), 0);
			sendto(sock, errorMsg, getErrMsgSize(), 0,(struct sockaddr *)&client_adrr,sockaddrLen);
			return 1;
	}
	return -1;
}
int closeServer(){
	if(sock!=-1)
		close(sock);
	if(sock_1!=-1)
		close(sock_1);
	close(fd);
	printf("%s\n", strerror(errno));
	exit(-1);
}

void closeConnection(){
//	close(sock);
//	sock=-1;
	memset((char*) &client_adrr, 0, sizeof(client_adrr));
}

int power(int a, int b){
	int result=1,i;
	for(i=0;i<b;i++)
		result*=a;
	return result;
}


int writeToFile(int size){

	return write(fd,packetBuff+4,size);
}

int readFromFile(){
	return read(fd,data+4,512);
}

int openFile(char * fileName){
	if(op==RRQ){
		fd = open (fileName,O_RDWR);
		if(fd ==-1){
			if( errno == ENOENT){//file dsn't exist
				errnum =FILE_NOT_FOUND;
				sendErrorMsg();
			}
			else
				errnum = ERR_WHEN_OPEN_FILE;
		}
	}
	else if(op==WRQ){
		fd = open (fileName,O_RDWR | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR);
		if(fd ==-1){
			if( errno == EEXIST){//file already exist
				errnum =6;
				sendErrorMsg();
			}
			else
				errnum = ERR_WHEN_OPEN_FILE;
		}
	}
	return fd;
}

int getIntValue(char lsb , char msb){
	int res=0,mask=1,exp=0;
	for(exp=0;exp<8;exp++){
		if(lsb&mask)
			res+=power(2,exp);
	mask = mask<<1;
	}
	mask=1;
	for(exp=8;exp<16;exp++){
		if(msb&mask)
			res+=power(2,exp);
	mask = mask<<1;
	}
	return res;
}

int getRandomPort(){
	return (rand()%(65000 - 49153)) + 49153;
}

void setHeaderForDataPacket (){
	memset(data,0,516);
	data[0] =0;
	data[1] = (int) DATA_OP;
	data [2] = blockNum/256;//(blockNum & 0xff00)>>8;
	data [3] = blockNum%256;//blockNum & 0xff;
}
int sendDataPackets(int size){
	int state = selectFTP(SEND);
	switch (state){
		case (-1): // error
			closeServer(); // exit
			return -1;
		case (0) : // time out
			closeConnection();
			return 0;
		case (1) :
			setHeaderForDataPacket();
			if(sendto(sock, data, size+4, 0,(struct sockaddr *)&client_adrr,sockaddrLen)<0)
				closeServer();		//SYSCALL error, exiting
			return 1;
	}
	return -1;

}
int sendFile(){
	int readCnt;
	int recvNum;
	while(1){
		readCnt=readFromFile();
		if(readCnt<0)
			return sendErrorMsg();
reTransmit:
		sendDataPackets(readCnt);
		recvNum = recvACK();
		if(recvNum==-1){
			errnum = SYS_ERROR;
			sendErrorMsg();
			break;
		}
		if(recvNum==TIME_OUT||recvNum==RETRANSMIT_PACKET) //timeout
			goto reTransmit;
		if(readCnt<512&&recvNum==1){//last packet
			printf("Finish to send the file\n");
			closeConnection();
			break;
		}

	}
	return 1;
}
/////////////////////////////////////////////


/////////////////////////////////////////
void setACK(){
	memset(ACK,0,4);
	ACK[0]=0;
	ACK[1] = ACK_OP;
	ACK[2] = ( blockNum & 0xff00)>>8;
	ACK[3] = blockNum & 0xff;

}
int sendACK(){
	int state = selectFTP(SEND);
	switch (state){
		case (-1): // error
			closeServer(); // exit
			return -1;
		case (0) : // time out
			closeConnection();
			return 0;
		case (1) :
			setACK();
			sendto(sock, ACK, 4, 0,(struct sockaddr *)&client_adrr,sockaddrLen);
//			send(sock, ACK, 4, 0);
			return 1;
	}
	return -1;
}

///////////////////////////////////////////////////////////////

/*
//selectFTP is called in recvOp, thus so, we shouldnt call it again
int recvDataPackets(){
	return read(sock, data+4, 512);
}
*/

int recvOp(){

	char headers [4] ;
	parseData(0, 4, headers);
	op = getIntValue(headers[1],headers[0]);
	if(op==3){//data packet
		sprintf(data,"%s", headers);
		if( getIntValue(headers[3],headers[2])!=blockNum+1)
			return DUPLICATE_PACKET;
		return 1;
	}
	if(op==5){// error msg
		sprintf(errorMsg,"%s", headers);
		return RECV_ERROR;
	}
	return ILLEGAL_PACKET;
}
int recvErrorMsg(){
	int cnt = recvPacket();
//	int cnt = recv(sock, errorMsg+4, 195);
	if(cnt<0){
		closeServer(); // exit
		return -1;
	}
	parseData(4, 195, errorMsg+4);
	errorMsg[199]=0;
	printf("%s\n", errorMsg+4);
	closeConnection();
	return 1;
}
int recvACK(){
	int state = selectFTP(RECV);
	int check;
	switch (state){
		case (-1): // error
			closeServer(); // exit
			return -1;
		case (0) : // time out
			return TIME_OUT;
		case (1) :
			check =recvPacket(); //recv(sock, ACK, 4, 0);
			if(check<0){
				closeServer(); // exit
				return -1;
			}
			parseData(0, 4, ACK);
			break;
	}
	if(getIntValue(ACK[1],ACK[0])!=4){
		closeConnection();
		return -1;
	}
	int blockNumber = getIntValue(ACK[3], ACK[2]);
	if(blockNumber!=blockNum)
		return RETRANSMIT_PACKET;
	else{
		blockNum++;
		return 1;
	}
}
int recvFile(){
	int sendNum;
	int op;
	int cnt;
	while(1){
		cnt = recvPacket();
		if(cnt < 0){
			errnum=SYS_ERROR;
			return sendErrorMsg();
			}
		op=recvOp();
		if(op==RECV_ERROR)
			return recvErrorMsg();
		if(op==ILLEGAL_PACKET){
			errnum=4;
			return sendErrorMsg();
		}
		if(op==TIME_OUT)//send ACK again
			goto reTransmit;
		if(op==DUPLICATE_PACKET)
			goto reTransmit;
		if(writeToFile(cnt-4)<0){ //disk full
			errnum=3;
			return sendErrorMsg();
		}
		blockNum++;
		if((blockNum & 0xffff)==0){ //allocation exceeded
			errnum=3;
			return sendErrorMsg();
		}
reTransmit:
		sendNum = sendACK();
		if(sendNum==-1){
			errnum = SYS_ERROR;
			sendErrorMsg();
			break;
		}
		if(sendNum==0) //timeout
			goto reTransmit;
		if(cnt<516 && sendNum==1){
			printf("Finish to receive file\n");
			sendACK();
			closeConnection();
			break;
		}
	}
	return 1;
}

//////////////////////////////////////////////




int setUDPConnection(struct sockaddr_in *addr,int port){
	int yes=1;
	memset((char *) addr, 0,sizeof(&addr));
	(*addr).sin_family = AF_INET;
	(*addr).sin_port = htons(port);
	(*addr).sin_addr.s_addr = htonl(INADDR_ANY);
	int socket_1 = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (socket_1 < 0)
		closeServer();
	if (setsockopt(socket_1, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
	    closeServer();
	}
	if (bind(socket_1, (struct sockaddr *) addr, sizeof(*addr))==-11111 )
		closeServer();
	if(op==RRQ || op==WRQ)//the server is connect to a list one client
		close(sock);
	return socket_1;
}



//////////////////////////////////////////////


//flag is 1 for receive and 0 for send
int selectFTP(int flag){		//check whether the first lines should be in the rcvNewConn..
	FD_ZERO(&sendSelect);
	FD_ZERO(&rcvSelect);
	//FD_SET(denySock, &sendSelect);
	if(flag==1)
		FD_SET(sock, &rcvSelect);
	else
		FD_SET(sock, &sendSelect);
	int nfds;
	//SELECT:
	nfds=(sock>sock_1 ? sock+1 : sock_1+1);
	int selectRV=select(nfds, &rcvSelect, &sendSelect, NULL, &timeout);
	if(selectRV<0) //error
		return -1;
	if(selectRV==0)//timeout
		return 0;

	if(FD_ISSET(sock, &sendSelect) || FD_ISSET(sock, &rcvSelect)){
		return 1;
	}
	return -1;
}

int checkFile(char * fileName){
	return (strlen(fileName)>100||strlen(fileName)<1) ? 0 : 1;

}

int checkMode(char * mode){
	if(strlen(mode)<5)
		return 0;
	int boolean=1;
	if((mode[0]!='o' && mode[0]!='O'))
		boolean=0;
	if((mode[1]!='c' && mode[1]!='C'))
		boolean=0;
	if((mode[2]!='t' && mode[2]!='T'))
		boolean=0;
	if((mode[3]!='e' && mode[3]!='E'))
		boolean=0;
	if((mode[4]!='t' && mode[4]!='T'))
		boolean=0;
	return boolean;

}
///////////////////////////////////////////////
int reciveNewConnection(){
	char packet [516];
	char fileName [101];
	char mode [50];
	int cnt;
	if((cnt=recvfrom(sock_1,packet,516,0, (struct sockaddr *) &client_adrr, (socklen_t *) &sockaddrLen))<0){
		closeConnection();
		return -1;
	}
	sock=setUDPConnection(&addr_second,getRandomPort());
	sprintf(fileName ,"%s", packet+2);
	sprintf(mode ,"%s",packet+2+strlen(fileName)+1);
	switch(getIntValue(packet[1],packet[0])){
		case(1)://RRQ
			op=RRQ;
			if(!checkFile(fileName)||!checkMode(mode)){		//case the file doesnt exist or the mode is not octet
				errnum=ERR_MODE;
				sendErrorMsg();
				closeConnection();
				return -1;
			}
			openFile(fileName);
			if(fd==-1&&errno!= ENOENT){		//case some Syscall failed and file exists
				if(errno== ENOENT){
					closeConnection();
					return 1;
				}
				closeServer();
				return -1;
			}
			blockNum=1;
			sendFile();
			break;
		case(2): //WRQ
			op=WRQ;
			if(!checkFile(fileName)||!checkMode(mode)){		//case the file doesnt exist or the mode is not octet
				errnum=4;
				sendErrorMsg();
				closeConnection();
				return -1;
			}
			openFile(fileName);
			if(fd==-1){//case some Syscall failed
				if(errno == EEXIST){//file exist
					closeConnection();
					return 1;
				}
				closeServer();//exit
			}

			blockNum=0;
			sendACK();
			recvFile();
			break;
		default:
			closeConnection();
			return -1;
	}
	return 1;

}

int main(int argc, char* argv[]){
	timeout.tv_sec=0;
	timeout.tv_usec=200;
	printf("Server is on..\nWaiting for new client.\n");
	sockaddrLen=sizeof(addr);
	sock_1 = setUDPConnection(&addr,PORT);
	while(1){
		reciveNewConnection();
		printf("Waiting for new client.\n");
		if(fd>0)
			close(fd);
	}
	close(sock_1);
	return 0;
}

