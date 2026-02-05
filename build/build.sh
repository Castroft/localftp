#!/bin/sh

cd ..

LFTP_DIR=`pwd`

export LFTP_DIR

echo -e "\033[32m Begin to build local ftp shell ! \033[0m"

cd $LFTP_DIR/build
make clean
rm -f lftp
make -j16 all > compile_log
make clean


# 测试服务器的 IP 地址 
# get 10.247.16.158 server.txt
# put 10.247.16.158 Makefile

ls -l lftp
if [ $? -eq 0 ]
then
    echo -e "\033[32mBuild lftp Success!\033[0m"
else
    echo -e "\033[31;1mBuild lftp Fail!\033[0m"
fi