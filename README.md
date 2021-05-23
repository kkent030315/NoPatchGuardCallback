<p align="center">
<img src="image.png">
  
<img src="https://img.shields.io/github/workflow/status/kkent030315/NoPatchGuardCallback/MSBuild?style=for-the-badge">
<img src="https://img.shields.io/badge/platform-win--64-00a2ed?style=for-the-badge">
<img src="https://img.shields.io/github/license/kkent030315/NoPatchGuardCallback?style=for-the-badge">
</p>

# NoPatchGuardCallback

x64 Windows PatchGuard Bypass, Register Process-Creation Callbacks

Read: https://www.godeye.club/2021/05/22/001-bypass-patchguard-pssetcreateprocessnotifyroutine.html

## What

This project is to bypass PatchGuard protection against `PsSetCreateProcessNotifyRoutine` by DKOM - *self register* arbitrary callback routine by directly manipulating kernel objects.

## Why

`PsSetCreateProcessNotifyRoutine` is the one of powerful NT Kernel API which allows driver to receive callbacks asynchronously.  
And it is protected by **PatchGuard** - The *Kernel Patch Protection* technology by Microsoft.

PatchGuard has a deep history of how it come in to the world - but not talk here.  

The PatchGuard protects **unsigned** code to be register callbacks by `PsSetCreateProcessNotifyRoutine`.  
`PsSetCreateProcessNotifyRoutineEx`, `PsSetCreateThreadNotifyRoutine` and `PsSetLoadImageNotifyRoutine` is not an exception, and many other.

When `PsSetCreateProcessNotifyRoutine` is called by the unsigned code, `KiRaiseSecurityCheckFailure`, the ISR, immediately interrupts and raises [bugcheck `0x139`](https://docs.microsoft.com/en-US/windows-hardware/drivers/debugger/bug-check-0x139--kernel-security-check-failure) eventually.

![bugcheck 0x139](bugcheck0x139.png)

## Warn

This PoC does not implement callback-block deletion.  
Please make sure to create fresh VM in order to try it.

## License

MIT 🄫 Kento Oki \<hrn832@protonmail.com\>
