# PacketCommandsProccessor

Программа для пакетной обработки команд. Команды считываются построчно из стандартного
ввода и обрабатываются блоками по N команд. Одна команда - одна строка, значение роли не играет. Если
данные закончились - блок завершается принудительно.
+ Размер блока можно изменить динамически, если перед началом блока и сразу после дать команды { и }
соответственно. Предыдущий пакет при этом принудительно завершается.
+ Такие(вложенные в {})блоки могут быть включены друг в друга при этом вложенность блоков игнорируются.
+ Если данные закончились внутри блока - блоки игнорируется целиком.