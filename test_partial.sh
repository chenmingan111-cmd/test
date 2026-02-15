#!/bin/bash
# 模拟半包发送
# 先发 "Hel"，停顿 1 秒，再发 "lo\n"
(printf "Hel"; sleep 1; echo "lo") | nc localhost 8080
