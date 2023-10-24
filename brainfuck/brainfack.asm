;FLAT asm
org     0x100    

start: 
;инициализируем буфер вывода нулями 
mov bx,offset array 
mov ax,offset array  
add ax,32
next0:
mov [bx],0  
inc bx
cmp bx,ax
jnz next0   
;печатаем приглашение к вводу
mov ah,02H   
mov dl,0Dh
int 21h  
mov dl,0Ah
int 21h
mov dl,'#'
int 21h
;загружаем строку в буфер  
mov ah,0aH
mov dx,offset buffer 
int 21h 
;переводим строку
mov ah,02H   
mov dl,0Dh
int 21h  
mov dl,0Ah
int 21h  
;разбираем полученные данные
mov bx,offset buffer + 1  ;указатель на текст программы  
mov si,offset array       ;указатель на ячейки пам¤ти 
mov ch,0                  ;счетчик скобок
char_compare:
inc bx    
;загружаем операнд
mov al,[bx]
cmp al,0;если на входе ноль вернутьс¤ на начало
jz start
;провер¤ем операнд
cmp al,'>'
jz next_cell
cmp al,'<'  
jz prev_cell
cmp al,'+'
jz inc_cell
cmp al,'-' 
jz dec_cell
cmp al,'.' 
jz print_cell
cmp al,'[' 
jz open_bracket
cmp al,']'  
jz loop_left
jmp char_compare

end: 
ret  
  
next_cell:  
inc si
jmp char_compare    

prev_cell:  
dec si
jmp char_compare  

inc_cell:  
inc [si]
jmp char_compare   

dec_cell:  
dec [si]
jmp char_compare   

print_cell:  
mov dl,[si]  
mov ah,02h
int 21h
jmp char_compare  

open_bracket:
cmp [si],0
;если текуща¤ ¤чейка равна 0 перепрыгиваем то закрывающей скобки
jz loop_right           
jmp char_compare   

loop_right:
;провер¤ем все символы в поисках закрывающей скобки
cmp [bx],'['
jz add_bracket_r 
cmp [bx],']'
jz remove_bracket_r 
inc bx
cmp ch,0  
jnz loop_right  
;избавл¤емс¤ от лишних инкрементов (стоит переделать эту часть)
dec bx
dec bx
jmp char_compare  

add_bracket_r:
inc ch    
inc bx
jmp  loop_right 

remove_bracket_r:
dec ch 
inc bx
jmp  loop_right   

loop_left:
;провер¤ем все символы в поисках открывающей скобки
cmp [bx],']'
jz add_bracket_l 
cmp [bx],'['
jz remove_bracket_l 
dec bx
cmp ch,0  
jnz loop_left  
;избавл¤емс¤ от лишнего дикремента
inc bx
jmp char_compare   

add_bracket_l:
inc ch    
dec bx
jmp  loop_left 

remove_bracket_l:
dec ch 
dec bx
jmp  loop_left 


;буффер дл¤ массива ячеек
array db ?, 32 dup (0), 0, 0         
;буффер для строки
buffer db 255,?, 32 dup (0), 0, 0









