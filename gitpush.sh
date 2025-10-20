#!/bin/bash
if [ -z "$1" ]; then
   echo "Не указан номер версии"
   exit 1
fi
git add src/*
git add public/*
git add package.json
git add gitpush.sh
git commit -m "version $1"
git push
echo "Версия $1 отправлена в Git"
