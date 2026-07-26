# Танки State Machine (draft)

 enum SystemState {  
     STATE\_DISCONNECTED,   
     STATE\_DISARMED,   
     STATE\_STARTFAIL,    
     STATE\_START,   
     STATE\_RUNNING,  
     STATE\_SHUTDOWN,   
 };

enum AudioMode {  
     AUDIO\_MODE\_MUTE,  
     AUDIO\_MODE\_SIREN,  
     AUDIO\_MODE\_START,  
     AUDIO\_MODE\_ENGINE,  
     AUDIO\_MODE\_STOP,  
 };

Описание  AudioMode:  
 Громкость определяется каналом 5 (VRA), кроме случая  AUDIO\_MODE\_SIREN  
1\. AUDIO\_MODE\_MUTE \- нет звука  
2\. AUDIO\_MODE\_SIREN \- звучит сирена. Громкость определяется средним значением  между каналом 5 (VRA) и максимальным звуком.  
3\. AUDIO\_MODE\_START \- играет start.wav один раз, затем idle.wav loop.    
4\. AUDIO\_MODE\_ENGINE \- продолжает играть idle.wav loop, параметры управляются стиком.   
5\. AUDIO\_MODE\_STOP \- играет stop.wav один раз

1\. Старт (RESET) программы: Начальный звук \- AUDIO\_MODE\_MUTE. Если пульт подключен (Канал 3 \>1000), SWA \= 1000, то переход в STATE\_DISARMED. Если пульт отключен  (Канал 3 \<=1000 или вообще нет сигнала iBUS), то переход в STATE\_DISCONNECTED.

2\. STATE\_DISCONNECTED:  
Аудио переходит в AUDIO\_MODE\_SIREN на 15 секунд, затем AUDIO\_MODE\_MUTE  
Переходы:   
Если появляется коннект (Канал 3 \>1000), то переход в  STATE\_DISARMED

3\. STATE\_DISARMED: аудио в AUDIO\_MODE\_MUTE.  
Переходы:  
а) Если теряется коннект \- переход в STATE\_DISCONNECTED  
б) Если включается SWA и стики не в исходном положении, то переход в STATE\_STARTFAIL  
в) Если включается SWA и стики в нормальном исходном положении, то переход в STATE\_START.

4\. STATE\_STARTFAIL: аудио в AUDIO\_MODE\_SIREN   
Переходы:  
а) Если теряется коннект \- переход в STATE\_DISCONNECTED  
б) Если выключается SWA, то переход в STATE\_DISARMED  
в) Если стики в нормальном исходном положении, то переход в STATE\_START.

5\. STATE\_START: аудио в AUDIO\_MODE\_START  
Управление движением заблокировано (как сейчас).  
Переходы:  
а) Если теряется коннект \- переход в STATE\_SHUTDOWN  
б) Если выключается SWA, то переход в STATE\_SHUTDOWN

6\. STATE\_RUNNING: аудио в AUDIO\_MODE\_ENGINE, в зависимости от положения стика (как сейчас)  
Переходы:  
а) Если теряется коннект \- переход в STATE\_SHUTDOWN  
б) Если выключается SWA, то переход в STATE\_SHUTDOWN  
в) Если стики остаются в нормальном исходном положении (+/- ошибка) дольше чем 30 секунд, то переход в STATE\_SHUTDOWN.

7\. STATE\_SHUTDOWN: аудио в AUDIO\_MODE\_STOP  
Управление движением плавно переводится из текущего в исходное положение (за 2 секунды) и блокируется ещё на 3 секунды.   
Все переходы из этого состояния осуществляются только после блокировки:  
а) Если потерян коннект \- переход в STATE\_DISCONNECTED  
б) Если коннект есть, но выключен SWA, то переход в STATE\_DISARMED

