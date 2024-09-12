# CSED312_OS

# 개발환경설정 (docker)

1. ./make_container.sh
2. Visual studio code
install dev containers, remote explorer
3. Find pintos container
4. attach to VS code
5. drag and drop 3 files (set_env.sh, download_pintos.sh, set_pintos.sh)
6. chmod +x set_env.sh download_pintos.sh set_pintos.sh
7. ./set_env.sh
8. ./download_pintos.sh
9. Restart terminal
10. ./set_pintos.sh

# 이후, 로컬 폴더와 싱크 맞추는 법

0. git init을 통해 remote와 local 저장소 연결
1. VScode를 통해 container 내부에 있는 pintos 폴더 다운로드 
local 저장소에 'pintos 폴더 안의 내용물'을 저장
이때, pintos 폴더 자체를 넣어버리면 PATH가 꼬이니까, 안전하게 내용물만 이동
2. docker에서 container ID 확인
3. sudo docker commit -p [컨테이너 ID] [저장할 이미지 이름]
4. sudo docker images로 이미지가 잘 저장되었는지 확인
5. local 저장소와 docker container 내부의 pintos 폴더의 싱크를 맞추는 방법
docker run -it --rm --name pintos --mount type=bind,source=/Users/{사용자명}/{local저장소}},target=/home/pintos {저장한 이미지 이름}}
6. make check로 올바르게 돌아가는지 확인
7. local에서 123.txt 파일 생성하여 싱크 맞춰지는지 확인
8. sourcetree 등 git application을 통해 편하게 사용할 수 있다.