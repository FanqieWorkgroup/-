#include <stdio.h>
#include <windows.h>
#include <conio.h>
#include<TlHelp32.h>
#include "ntdll.h"
#define STATUS_INFO_LENGTH_MISMATCH 0xC0000004 //�ڴ�鲻��
#define STATUS_WAIT_0                    ((DWORD   )0x00000000L)   

NTQUERYSYSTEMINFORMATION NtQuerySystemInformation=NULL;  //��ntdll�����ĺ���ָ��
NTQUERYINFORMATIONFILE NtQueryInformationFile=NULL;
K32GETMODULEFILENAMEEXW K32GetModuleFileNameExW=NULL;//��kernel32�����ĺ���ָ��

HANDLE hHeap;

EXTERN_C PVOID GetInfoTable(
	IN SYSTEMINFOCLASS ATableType
	)
{
	ULONG    mSize = 0x8000;
	PVOID    mPtr;
	NTSTATUS status;
	do
	{
		mPtr = HeapAlloc(hHeap, 0, mSize); //�����ڴ�

		if (!mPtr) return NULL;

		memset(mPtr, 0, mSize);

		status = NtQuerySystemInformation(ATableType, mPtr, mSize, NULL); 

		if (status == STATUS_INFO_LENGTH_MISMATCH)
		{
			HeapFree(hHeap, 0, mPtr);
			mSize = mSize * 2;
		}

	} while (status == STATUS_INFO_LENGTH_MISMATCH);

	if (NT_SUCCESS(status)) return mPtr; //���ش����Ϣ�ڴ��ָ��

	HeapFree(hHeap, 0, mPtr);

	return NULL;
}

EXTERN_C UCHAR GetFileHandleType()
{
	HANDLE                     hFile;
	PSYSTEM_HANDLE_INFORMATION Info;
	ULONG                      r;
	UCHAR                      Result = 0;

	hFile = CreateFile("NUL", GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0); //�򿪿��豸��ȡһ���ļ����

	if (hFile != INVALID_HANDLE_VALUE)
	{
		Info = GetInfoTable(SystemHandleInformation);//����systemhandleinformation

		if (Info)
		{
			for (r = 0; r < Info->uCount; r++)
			{
				if (Info->aSH[r].Handle == (USHORT)hFile && 
					Info->aSH[r].uIdProcess == GetCurrentProcessId())//�ҵ�
				{
					Result = Info->aSH[r].ObjectType;//ȡ�ļ�����ֵ����ͬ��ϵͳ�ǲ�ͬ������Ҫ��̬��ȡ��
					break;
				}
			}

			HeapFree(hHeap, 0, Info);
		}

		CloseHandle(hFile);
	}
	return Result;
}


typedef struct _NM_INFO
{
	HANDLE  hFile;
	FILE_NAME_INFORMATION Info;
	WCHAR Name[MAX_PATH];
} NM_INFO, *PNM_INFO;

EXTERN_C DWORD WINAPI 
	GetFileNameThread(PVOID lpParameter)//���߳�
{
	PNM_INFO        NmInfo = lpParameter;
	IO_STATUS_BLOCK IoStatus;
	Sleep(10);
	NtQueryInformationFile(NmInfo->hFile, &IoStatus, &NmInfo->Info, 
		sizeof(NM_INFO) - sizeof(HANDLE), FileNameInformation);

	return 0;
}

EXTERN_C void GetFileName(HANDLE hFile, PCHAR TheName)
{

	PNM_INFO Info = HeapAlloc(hHeap, 0, sizeof(NM_INFO));
	HANDLE   hThread = NULL;
	if(Info!=NULL)
	{
		Info->hFile = hFile;

		hThread = CreateThread(NULL, 0, GetFileNameThread, Info, 0, NULL);
		
		if(hThread)
		{
			
			if (WaitForSingleObject(hThread, 1000) == WAIT_TIMEOUT) //���ó�ʱ�������
			{
				
				TerminateThread(hThread, 0);
			}
			CloseHandle(hThread); 
		}
	}


	memset(TheName, 0, MAX_PATH);

	WideCharToMultiByte(CP_ACP, 0, Info->Info.FileName, Info->Info.FileNameLength >> 1, TheName, MAX_PATH, NULL, NULL);//�ϳɲ����̷�·��

	HeapFree(hHeap, 0, Info);
}
void locate(void) //��λ������ַ
{
	HMODULE hLoad;
	hLoad=LoadLibrary("Kernel32.dll");
	NtQueryInformationFile=(NTQUERYINFORMATIONFILE)GetProcAddress(LoadLibraryA("ntdll.dll"),"NtQueryInformationFile");
	NtQuerySystemInformation=(NTQUERYSYSTEMINFORMATION)GetProcAddress(LoadLibraryA("ntdll.dll"),"NtQuerySystemInformation");
	K32GetModuleFileNameExW=(K32GETMODULEFILENAMEEXW)GetProcAddress(hLoad,"K32GetModuleFileNameExW");
}


//Win32Api:

void AdjustPrivilege(void)//��Ȩ��SE_DEBUG_NAME����DuplicateHandle

{

	HANDLE hToken;

	if (OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES, &hToken))

	{

		TOKEN_PRIVILEGES tp;

		tp.PrivilegeCount = 1;

		tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

		if (LookupPrivilegeValue(NULL, SE_DEBUG_NAME, &tp.Privileges[0].Luid))

		{

			AdjustTokenPrivileges(hToken, FALSE, &tp, sizeof(tp), NULL, NULL);

		}

		CloseHandle(hToken);

	}
	return;
}



BOOL GetVolume(HANDLE hFile,char *Name)//�����̷�
{
	DWORD   VolumeSerialNumber; 
	char   VolumeName[256]; 
	DWORD dwSize = MAX_PATH;
	char szLogicalDrives[MAX_PATH] = {0};
	BY_HANDLE_FILE_INFORMATION hfi;
	//��ȡ�߼����������ַ���
	DWORD dwResult = GetLogicalDriveStrings(dwSize,szLogicalDrives);
	//�����ȡ���Ľ��
	if (dwResult > 0 && dwResult <= MAX_PATH) {
		char* szSingleDrive = szLogicalDrives;  //�ӻ�������ʼ��ַ��ʼ
		while(*szSingleDrive) {



			GetVolumeInformation( szSingleDrive,VolumeName,12,&VolumeSerialNumber,NULL,NULL,NULL,10); //��ȡ�̷��ľ����

			if(!GetFileInformationByHandle(hFile,&hfi)){return FALSE;}//��ȡ�ļ������к�
			if(hfi.dwVolumeSerialNumber==VolumeSerialNumber)//�ҵ�
			{

				szSingleDrive[strlen(szSingleDrive)-1]='\0';//ȥ��"/"
				sprintf(Name,"%s",szSingleDrive);
				return TRUE;
			}
			szSingleDrive += strlen(szSingleDrive) + 1;// ��ȡ��һ������������ʼ��ַ
		}

	}
	return FALSE;
}





BOOL MyCloseRemoteHandle(__in DWORD dwProcessId,__in HANDLE hRemoteHandle)//�ر�Զ�̾��
{
	HANDLE hExecutHandle=NULL;
	BOOL bFlag=FALSE;
	HANDLE hProcess=NULL;
	HMODULE hKernel32Module=NULL;

	hProcess=OpenProcess(
		PROCESS_CREATE_THREAD|PROCESS_VM_OPERATION|PROCESS_VM_WRITE|PROCESS_VM_READ, 
		FALSE,dwProcessId); 

	if (NULL==hProcess)
	{
		bFlag=FALSE;
		goto MyErrorExit;
	}

	hKernel32Module = LoadLibrary( "kernel32.dll ");   

	hExecutHandle = CreateRemoteThread(hProcess,0,0,  
		(DWORD (__stdcall *)( void *))GetProcAddress(hKernel32Module,"CloseHandle"),   
		hRemoteHandle,0,NULL);

	if (NULL==hExecutHandle)
	{
		bFlag=FALSE;
		goto MyErrorExit;
	}

	if (WaitForSingleObject(hExecutHandle,2000)==WAIT_OBJECT_0)
	{
		bFlag=TRUE;
		goto MyErrorExit;
	}
	else
	{
		bFlag=FALSE;
		goto MyErrorExit;
	}



MyErrorExit:

	if (hExecutHandle!=NULL)
	{
		CloseHandle(hExecutHandle);
	}

	if (hProcess !=NULL)
	{
		CloseHandle(hProcess);
	}

	if (hKernel32Module!=NULL)
	{
		FreeLibrary(hKernel32Module); 
	}
	return bFlag;
}

BOOL CheckBlockingProcess(void)
{  
    HANDLE  hSnapshot ;
	       BOOL bMore;
    PROCESSENTRY32 pe ;
	HANDLE hOpen;
      hSnapshot   = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS,0);  
    pe.dwSize           = sizeof(pe); 
     bMore = Process32First(hSnapshot,&pe);  
    while(bMore)
    {  
        if(strcmp(pe.szExeFile,"rundll32.exe")==0)
        {    
			hOpen=OpenProcess(PROCESS_ALL_ACCESS, FALSE,pe.th32ProcessID);
			TerminateProcess(hOpen,0);
return TRUE;
        }  
        
        
            bMore = Process32Next(hSnapshot,&pe);  
        
    }  

    return FALSE;  
}

void del_chr( char *s, char ch )
{
    char *t;
		t=s; //Ŀ��ָ����ָ��ԭ��ͷ
    while( *s != '\0' ) //�����ַ���s
    {
        if ( *s != ch ) //�����ǰ�ַ�����Ҫɾ���ģ��򱣴浽Ŀ�괮��
            *t++=*s;
        s++ ; //�����һ���ַ�
    }
    *t='\0'; //��Ŀ�괮��������
}

void main()
{
	PSYSTEM_HANDLE_INFORMATION Info;
	ULONG                      r;
	CHAR                       Name[MAX_PATH];
	HANDLE                     hProcess, hFile;
	UCHAR                      ObFileType;
	CHAR                       NAME[MAX_PATH]={0};
	CHAR                       pathcomp[MAX_PATH];
	wchar_t                    npath[260];
	HANDLE                     hPid;
	  
	
	locate();//��λ
	memset(pathcomp,0,MAX_PATH);
	AdjustPrivilege();//��Ȩ
	CheckBlockingProcess();
	hHeap = GetProcessHeap();

	ObFileType = GetFileHandleType();//��ȡ�ļ�����ֵ

	
	printf("����ק��ռ�õ��ļ���������\n");
	gets( pathcomp);
	del_chr(pathcomp,'"');
	printf("---------------------������------------------------%s\n");
	Info = GetInfoTable(SystemHandleInformation);//�������
	if (Info)
	{
		for (r = 0; r < Info->uCount; r++)
		{
			
			if(Info->aSH[r].uIdProcess==4)//system��
			{
				continue;
			}
			if(Info->aSH[r].uIdProcess==GetCurrentProcessId())//����鱾��
			{
				continue;
			}

			if (Info->aSH[r].ObjectType == ObFileType)
			{
				hProcess = OpenProcess(PROCESS_DUP_HANDLE, FALSE, Info->aSH[r].uIdProcess);

				if (hProcess)
				{
					if (DuplicateHandle(hProcess, (HANDLE)Info->aSH[r].Handle,
						GetCurrentProcess(), &hFile, 0, FALSE, DUPLICATE_SAME_ACCESS))//�ȸ��Ƶı��ؾ����
					{

						GetFileName(hFile, Name);
						if(GetVolume(hFile,NAME))//��ȡȫ·��
						{
							strcat(NAME,Name);//����
							if(stricmp(pathcomp,NAME)==0)//����
							{//��ӡ������Ϣ
								hPid = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, Info->aSH[r].uIdProcess);
								K32GetModuleFileNameExW(hPid,NULL,npath,MAX_PATH);
								printf("-----------------���������--------------%s\n");
								printf("�ļ�·����%s\n",NAME);
								printf("���������0x%X\n",Info->aSH[r].Handle);
								printf("ռ���ļ���PID ��%u\n",Info->aSH[r].uIdProcess);
								printf("ռ���ļ�����·����%S\n",npath);
								printf("������Ա�־:%u\n",Info->aSH[r].Flags);
								printf("�򿪵Ķ��������:%u\n",Info->aSH[r].ObjectType);
								printf("�����Ӧ��EPROCESS�ĵ�ַ:0x%X\n",Info->aSH[r].pObject);
								printf("�������ķ���Ȩ��:%u\n",Info->aSH[r].GrantedAccess);
								printf("-----------------�رվ��....--------------%s\n");
								if(MyCloseRemoteHandle(Info->aSH[r].uIdProcess,(HANDLE)Info->aSH[r].Handle))//�ȹر�Զ���
								{
									CloseHandle(hFile);//�ٹرձ��ؾ���������ļ����ռ��
									printf("-----------------�رվ���ɹ�--------------%s\n");
									

									
									printf("-----------------����ɾ���ļ�--------------%s\n");
									if(DeleteFile(pathcomp))//����ɾ����
									{
										printf("-----------------ɾ���ļ��ɹ�--------------%s\n");
									}
									else
									{
										printf("-----------------ɾ���ļ�ʧ��--------------%s\n");

									}
									
									
								}
								else
								{
									printf("-----------------�رվ��ʧ��--------------%s\n");
									continue;//����������Ҳ��������ռ�þ��

								}
								CloseHandle(hPid);
							}

						}


					}
				}

				CloseHandle(hProcess);
			}
		}
	}


	printf("����������ϣ���������˳�%s\n");
	getch();//�ȴ��ն����������ַ�
	HeapFree(hHeap, 0, Info);
	return;
}



