/*

	MIT License

	Copyright (c) 2021 Kento Oki

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.

*/

#include "main.h"

PVOID NpgQueryModuleImageBase( PCHAR ModuleName )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;
	PVOID Address = 0;
	ULONG NeededSize = 0;

	ntStatus = ZwQuerySystemInformation(
		SystemModuleInformation, 0, 0, &NeededSize );

	if ( ntStatus != STATUS_INFO_LENGTH_MISMATCH )
	{
		NPGCB_KDPRINT( "ZwQuerySystemInformation Failed\n" );
		return Address;
	}

	PRTL_PROCESS_MODULES ModuleList = ( PRTL_PROCESS_MODULES )
		ExAllocatePool( NonPagedPool, NeededSize );

	if ( !ModuleList )
	{
		NPGCB_KDPRINT( "Failed to allocate pool\n" );
		return Address;
	}

	ntStatus = ZwQuerySystemInformation(
		SystemModuleInformation, ModuleList, NeededSize, 0 );

	if ( !NT_SUCCESS( ntStatus ) )
	{
		NPGCB_KDPRINT( "ZwQuerySystemInformation Failed\n" );
		ExFreePool( ModuleList );
		return Address;
	}

	for ( ULONG i = 0; i < ModuleList->NumberOfModules; ++i )
	{
		RTL_PROCESS_MODULE_INFORMATION entry = ModuleList->Modules[ i ];

		if ( strstr( ( PCHAR )entry.FullPathName, ModuleName ) )
		{
			Address = entry.ImageBase;
			break;
		}
	}

	ExFreePool( ModuleList );
	return Address;
}

//
// the callback receiver
//
VOID PsCreateProcessNotifyCallback(
	IN HANDLE ParentId,
	IN HANDLE ProcessId,
	IN BOOLEAN Create )
{
	NPGCB_KDPRINT( "PsCreateProcessNotifyCallback Called\n" );
}

NTSTATUS NpgInitialize()
{
	NTSTATUS ntStatus = STATUS_SUCCESS;

	//
	// the ntoskrnl image base
	//
	PVOID NtosKrnlImageBase =
		NpgQueryModuleImageBase( "ntoskrnl.exe" );

	if ( !NtosKrnlImageBase )
	{
		NPGCB_KDPRINT( "Failed to locate ntoskrnl image base\n" );
		return STATUS_UNSUCCESSFUL;
	}

	//
	// both ExAllocateCallBack and ExCompareExchangeCallBack are
	// undocumented NT Kernel internal function
	//
	// these RVAs are might not be work on another Windows builds
	// or cause system crash
	//

	*( PVOID* )( &ExAllocateCallBack ) =
		( PVOID )( ( UINT_PTR )NtosKrnlImageBase + RVA_EX_ALLOCATE_CALLBACK );

	*( PVOID* )( &ExCompareExchangeCallBack ) =
		( PVOID )( ( UINT_PTR )NtosKrnlImageBase + RVA_EX_COMPARE_EXCHANGE_CALLBACK );

	NPGCB_KDPRINT( "ExAllocateCallBack @ 0x%p\n", ExAllocateCallBack );
	NPGCB_KDPRINT( "ExCompareExchangeCallBack @ 0x%p\n", ExCompareExchangeCallBack );

	if ( !ExAllocateCallBack || !ExCompareExchangeCallBack )
	{
		NPGCB_KDPRINT( "Failed to locate NT kernel internal functions\n" );
		return STATUS_UNSUCCESSFUL;
	}

	return ntStatus;
}

NTSTATUS NpgRegisterCallback(
	PCREATE_PROCESS_NOTIFY_ROUTINE NotifyRoutine )
{
	NTSTATUS ntStatus = STATUS_SUCCESS;

	NPGCB_KDPRINT( "NpgRegisterCallback Called\n" );
	NPGCB_KDPRINT( " ---> NotifyRoutine: 0x%p\n", NotifyRoutine );

	PVOID NtosKrnlImageBase =
		NpgQueryModuleImageBase( "ntoskrnl.exe" );

	if ( !NtosKrnlImageBase )
	{
		NPGCB_KDPRINT( "Failed to locate ntoskrnl image base\n" );
		return STATUS_UNSUCCESSFUL;
	}

	NPGCB_KDPRINT( "ntoskrnl @ %p\n", NtosKrnlImageBase );

	//
	// allocate the new callback-block
	//
	PCALLBACK_ROUTINE_BLOCK CallbackBlock =
		ExAllocateCallBack( PsCreateProcessNotifyCallback, FALSE );

	if ( !CallbackBlock )
	{
		NPGCB_KDPRINT( "Failed to allocate callback block\n" );
		return STATUS_INSUFFICIENT_RESOURCES;
	}

	NPGCB_KDPRINT( "Allocated Callback : %p\n", CallbackBlock );
	NPGCB_KDPRINT( "                   : CallbackBlock->Function : %p\n", CallbackBlock->Function );
	NPGCB_KDPRINT( "                   : CallbackBlock->Context  : %p\n", CallbackBlock->Context );

	ULONG Counter = 0;

	//
	// this array (sizeof 64) contains all callback blocks
	//
	PVOID* Array = &*( PVOID* )
		( ( UINT_PTR )NtosKrnlImageBase + RVA_PSP_CREATE_PROCESS_NOTIFY_ROUTINE );

	while ( !ExCompareExchangeCallBack(
		&Array[ Counter ],
		CallbackBlock,
		0 ) )
	{
		Counter++;

		//
		// only 64 callbacks available
		// more than 64 entries mean there are no more available slots
		//
		if ( Counter >= PSP_CREATE_PROCESS_NOTIFY_MAX )
		{
			NPGCB_KDPRINT( "No Resource to register new callback!\n" );
			return STATUS_INSUFFICIENT_RESOURCES;
		}
	}

	//
	// increment count
	// i am sorry for the terrible expression
	//
	InterlockedIncrement( ( volatile LONG* )
		&*( UINT32* )
		( ( UINT_PTR )NtosKrnlImageBase + RVA_PSP_CREATE_PROCESS_NOTIFY_ROUTINE_COUNT ) );

	NPGCB_KDPRINT( "Callback is now registered\n" );

	return ntStatus;
}

NTSTATUS DispatchDriverEntry
(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	UNREFERENCED_PARAMETER( DriverObject );
	UNREFERENCED_PARAMETER( RegistryPath );

	NTSTATUS ntStatus = STATUS_SUCCESS;

	//
	// prepare resources
	//
	ntStatus = NpgInitialize();

	if ( !NT_SUCCESS( ntStatus ) )
	{
		NPGCB_KDPRINT( "Failed to initialize\n" );
		return ntStatus;
	}

	//
	// register PsCreateProcessNotifyCallback instead
	// of using PsSetCreateProcessNotifyRoutine
	//
	// PatchGuard will never seeing you!
	//
	ntStatus = NpgRegisterCallback( PsCreateProcessNotifyCallback );

	if ( !NT_SUCCESS( ntStatus ) )
	{
		NPGCB_KDPRINT( "Failed to register callback\n" );
		return ntStatus;
	}

	return ntStatus;
}

//
// this will be called when the driver being unloaded
//
VOID
UnloadDriver
(
	IN PDRIVER_OBJECT DriverObject
)
{
	UNREFERENCED_PARAMETER( DriverObject );
}

//
// main entry point of this driver
//
NTSTATUS DriverEntry
(
	IN PDRIVER_OBJECT DriverObject,
	IN PUNICODE_STRING RegistryPath
)
{
	DriverObject->DriverUnload = UnloadDriver;

	return DispatchDriverEntry( DriverObject, RegistryPath );
}