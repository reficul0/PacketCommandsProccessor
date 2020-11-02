[![Codacy Badge](https://app.codacy.com/project/badge/Grade/d402a715ed334152bc8448b328c76bb8)](https://www.codacy.com/gh/reficul0/PacketCommandsProccessor/dashboard?utm_source=github.com&amp;utm_medium=referral&amp;utm_content=reficul0/PacketCommandsProccessor&amp;utm_campaign=Badge_Grade)
[![MIT License](https://img.shields.io/badge/license-MIT-blue.svg?style=flat)](https://github.com/RocketChat/Rocket.Chat/raw/master/LICENSE)

# PacketCommandsProccessor
[Требует доработки]

Программа для пакетной обработки команд. Команды считываются построчно из стандартного
ввода и обрабатываются блоками по N команд. Одна команда - одна строка, значение роли не играет. Если
данные закончились - блок завершается принудительно.
+ Размер блока можно изменить динамически, если перед началом блока и сразу после дать команды { и }
соответственно. Предыдущий пакет при этом принудительно завершается.
+ Такие(вложенные в {})блоки могут быть включены друг в друга при этом вложенность блоков игнорируются.
+ Если данные закончились внутри блока - блоки игнорируется целиком.
