#include "Hook.h"

VOID PageProtectOff()
{
	ULONG_PTR cr0;
	//__disable();										//�����ж�
	Irql = KeRaiseIrqlToDpcLevel();						//������DpcLevelȻ�󱣴�ԭ����IRQL
	cr0 = __readcr0();									//��ȡcr0
	cr0 &= 0xfffffffffffeffff;							//��ҳд�뱣��λ��������
	__writecr0(cr0);									//д��cr0
}

VOID PageProtectOn()
{
	ULONG_PTR cr0;
	cr0 = __readcr0();									//��ȡcr0
	cr0 |= 0x10000;										//��ԭҳ����λ
	__writecr0(cr0);									//д��cr0
	//__enable();										//��������ж�����
	KeLowerIrql(Irql);									//����IRQL�������ֵ
}

ULONG_PTR GetFuncAddress(PWSTR FuncName)
{
	UNICODE_STRING uFunctionName;
	RtlInitUnicodeString(&uFunctionName, FuncName);
	return (ULONG_PTR)MmGetSystemRoutineAddress(&uFunctionName);
}

NTSTATUS __fastcall MyPsLookupProcessByProcessId(__in HANDLE ProcessId, __deref_out PEPROCESS *Process)
{
	NTSTATUS RetStatus;

	RetStatus = ((PSLOOKUPPROCESSBYPROCESSID)(OldFunc))(ProcessId, Process);
	if (NT_SUCCESS(RetStatus) && strstr((CHAR*)PsGetProcessImageFileName(*Process), "calc"))
	{
		KdPrint(("������ͨ��PID��ȡ��������EPROCESS\n"));
		*Process = NULL;
		return STATUS_ACCESS_DENIED;
	}

	return RetStatus;
}

VOID StartHook()
{
	ULONG_PTR PsLookupAddress;

	PsLookupAddress = GetFuncAddress(L"PsLookupProcessByProcessId");
	
	OldFunc = (UCHAR*)sfExAllocate(sizeof(OldCode) + sizeof(JmpOld));					//�ȷ����ڴ�������������ʧ��Ҳû����Ҫ�����ˣ���Ϊ������ת�����ĺ���

	if (OldFunc == NULL)
	{
		KdPrint(("�����ڴ�ʧ�ܣ�\n"));
		return;
	}

	*(ULONG_PTR*)(JmpOld + 6) = PsLookupAddress + 15;									//��ת��ԭ������15���ֽڴ�

	*(ULONG_PTR*)(NewCode + 6) = (ULONG_PTR)MyPsLookupProcessByProcessId;				//��ת��ַָ�������Լ��ĺ���

	RtlCopyMemory((PVOID)OldCode, (PVOID)PsLookupAddress, sizeof(OldCode));				//����ԭ�������������15���ֽڱ�������

	PageProtectOff();

	RtlCopyMemory((PVOID)PsLookupAddress, (PVOID)NewCode, sizeof(NewCode));				//����ת������ԭ������

	PageProtectOn();

	RtlCopyMemory((PVOID)OldFunc, (PVOID)OldCode, sizeof(OldCode));						//����ԭ����ʮ����ֽڳ���

	RtlCopyMemory((PVOID)(OldFunc + sizeof(OldCode)), (PVOID)JmpOld, sizeof(JmpOld));	//������ת��ԭ������ʮ����ֽڴ���ָ�����
}

VOID StopHook()
{
	ULONG_PTR PsLookupAddress;

	PsLookupAddress = GetFuncAddress(L"PsLookupProcessByProcessId");

	PageProtectOff();

	RtlCopyMemory((PVOID)PsLookupAddress, (PVOID)OldCode, sizeof(OldCode));				//��ԭ������ָ��

	PageProtectOn();

	sfExFree(OldFunc);
}

VOID Unload(PDRIVER_OBJECT DriverObject)
{
	StopHook();
	KdPrint(("Unload Success!\n"));
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegString)
{	
	KdPrint(("Entry Driver!\n"));
	StartHook();
	DriverObject->DriverUnload = Unload;
	return STATUS_SUCCESS;
}