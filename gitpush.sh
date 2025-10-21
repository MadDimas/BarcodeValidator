#!/bin/bash
if [ -z "$1" ]; then
   echo "Не указан номер версии"
   exit 1
fi
git add BarcodeValidator.ino
git add partitions.csv
git add data/*
git add gitpush.sh
git commit -m "version $1"
git push
echo "Версия $1 отправлена в Git"
