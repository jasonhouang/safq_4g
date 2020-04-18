#!/usr/bin/env python3
import subprocess
import threading
from time import sleep

def flash_stm32_output_reader(proc, is_flash_started):
    for line in iter(proc.stdout.readline, b''):
        print('{0}'.format(line.decode('utf-8')), end='')
        if line.decode('utf-8').find('Cortex-M4 identified') != -1:
            print("start flash device")
            try:
                proc.stdin.write(('loadbin ./rtthread.bin 0x08020000\n').encode('utf-8'))
                proc.stdin.flush()
                is_flash_started = True
            except Exception as e:
                proc.stdin.write(b'q\n')
                print(e)
                break
        if is_flash_started and line.decode('utf-8').find('O.K.') != -1:
            try:
                proc.stdin.write(b'r\n')
                proc.stdin.flush()
                sleep(1)
                proc.stdin.write(b'g\n')
                proc.stdin.flush()
                sleep(1)
                proc.stdin.write(b'q\n')
                proc.stdin.flush()
            except Exception as e:
                proc.stdin.write(b'q\n')
                print(e)
                break


def start_flash_device():
    bashCmd = ["JLinkExe", "-if", "SWD", "-speed", "2000", "-AutoConnect", "1", "-device", "stm32f407zg"]
    p = subprocess.Popen(bashCmd, stdin = subprocess.PIPE, stdout=subprocess.PIPE, stderr = subprocess.PIPE)
    is_flash_started = False
    t = threading.Thread(target=flash_stm32_output_reader, args=(p, is_flash_started))
    t.start()
    print("start flash")
    t.join()

if __name__ == '__main__':
    start_flash_device()
    print("exit")
