#!/bin/bash

# 변수 설정
SERVER="stu28@141.223.108.159"
PORT="22"
REMOTE_DEST_DIR="pintos"
REMOTE_TMP_BACKUP="~/tmp/pintos_backup_$(date +%Y%m%d%H%M%S)" # 사용자 홈 디렉토리 내의 백업 폴더 생성
LOCAL_SRC_DIR="$(pwd)"  # 현재 디렉토리를 절대 경로로 지정

# 원격 서버에 접속하여 pintos 폴더 백업
echo "Backing up pintos folder on remote server..."
ssh -p $PORT $SERVER << EOF
    # 사용자 홈 디렉토리 내의 /tmp 폴더 내에 새로운 백업 폴더 생성
    echo "Creating backup directory: $REMOTE_TMP_BACKUP"
    mkdir -p $REMOTE_TMP_BACKUP || { echo "Failed to create backup directory. Check permissions."; exit 1; }

    # pintos 폴더 내용을 /tmp 백업 폴더로 복사
    echo "Backing up contents of $REMOTE_DEST_DIR to $REMOTE_TMP_BACKUP"
    cp -r ~/$REMOTE_DEST_DIR/* $REMOTE_TMP_BACKUP/ || { echo "Failed to backup contents. Check permissions and path."; exit 1; }

    # pintos 폴더 내용 삭제 (선택 사항)
    echo "Removing all files in $REMOTE_DEST_DIR"
    rm -rf ~/$REMOTE_DEST_DIR/*
EOF

# 로컬 디렉토리에서 원격 서버의 pintos 폴더로 파일 전송
echo "Transferring local $LOCAL_SRC_DIR folder contents to remote server $REMOTE_DEST_DIR..."
scp -r -P $PORT $LOCAL_SRC_DIR/* $SERVER:~/$REMOTE_DEST_DIR/

echo "Sync completed."
