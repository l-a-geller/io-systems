# Лабораторная работа 2

**Название:** "Разработка драйверов блочных устройств"

**Цель работы:** Получить знания и навыки разработки драйверов блочных
устройств для операционной системы Linux

## Описание функциональности драйвера

- Драйвер создает блочное устройство (виртуальный жесткмй диск с размером 50 Мбайт) в оперативной памяти
- Созданный диск разбит на три первичных раздела с размерами 10Мбайт, 25Мбайт и 15Мбай

## Инструкция по сборке

```bash
git clone https://github.com/l-a-geller/io-systems
cd io-systems/lab2
make
insmod main.ko
make mount
...
perform everything you need
...
make umount
rmmod main.ko
```

## Инструкция пользователя и примеры использования

1) Просмотр информации о созданном блочном устройстве и его разделах
```
sudo fdisk -l /dev/lab2
```
```
Disk /dev/lab2: 50 MiB, 52428800 bytes, 102400 sectors
Units: sectors of 1 * 512 = 512 bytes
Sector size (logical/physical): 512 bytes / 512 bytes
I/O size (minimum/optimal): 512 bytes / 512 bytes
Disklabel type: dos
Disk identifier: 0x00000000

Device      Boot Start    End Sectors Size Id Type
/dev/lab2p1          1  20479   20479  10M 83 Linux
/dev/lab2p2      20480  71679   51200  25M 83 Linux
/dev/lab2p3      71680 102399   30720  15M 83 Linux
```
```
lsblk -l /dev/lab2
```
```
NAME   MAJ:MIN RM SIZE RO TYPE MOUNTPOINT
lab2   252:0    0  50M  0 disk 
lab2p1 252:1    0  10M  0 part 
lab2p2 252:2    0  25M  0 part 
lab2p3 252:3    0  15M  0 part 
```

2) Протестировать запись & чтение с устройства
```
sudo dd if=/dev/zero of=/dev/lab2p1 count=1
```
```
1+0 records in
1+0 records out
512 bytes copied, 0,00029337 s, 1,7 MB/s
```
```
sudo echo "test_string" > /dev/lab2p1
```
```
sudo xxd /dev/lab2p1 | less
```
```
00000000: 7465 7374 6573 7465 730a 0000 0000 0000  test_string.....
00000010: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000020: 0000 0000 0000 0000 0000 0000 0000 0000  ................
00000030: 0000 0000 0000 0000 0000 0000 0000 0000  ................
```

3) Измерить скорость передачи данных при копировании файлов между разделами созданного виртуального диска, между разделами виртуального и реального жестких дисков
```
sudo dd if=/dev/lab2p1 of=/dev/lab2p2 count=1
```
```
512 bytes copied, 0,000428629 s, 1,2 MB/s
```
```
sudo dd if=/dev/sda1 of=/dev/lab2p2 count=1
```
```
512 bytes copied, 0,000363176 s, 1,4 MB/s
```
