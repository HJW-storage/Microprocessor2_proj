# Microprocessor2 - Term Project
## collaborator
<table>
  <tr>
  <!--
  <td align="center"><b>Team Leader</b></sub></a><br /></td>
  <td align="center"><b>Autonomous Driving</b></sub></a><br /></td>
  <td align="center"><b>S/W</b></sub></a><br /></td>
  <td align="center"><b>S/W</b></sub></a><br /></td>
  -->
  </tr>
    <td align="center"><a href="https://github.com/HJW-storage"><img src="https://user-images.githubusercontent.com/103934004/229440749-5e448f84-ee88-48d5-8d2e-22881c1d4baf.jpeg" width="100px;" alt=""/><br /><sub><b>Hong Ji Whan</b></sub></a><br /></td>
    <td align="center"><img src="https://user-images.githubusercontent.com/113449410/231176805-0df7a553-98de-4d5c-a073-c084e019d3ac.jpg" width="100px;" alt=""/><br /><sub><b>Kin han su</b></sub><br /></td>
  </tr>
</table>
<!-- ![RC_CAR body](https://user-images.githubusercontent.com/113449410/231169656-a39c019c-36e5-48e5-b870-96224d49c9e3.jpg) -->

<!-- ### 이미지 사이즈 조절 -->



## 프로젝트 소개
### 과제명 - 초보 운전자 주행 및 주차 보조 모델 RC카 제작
#### 주행보조 및 주차 보조를 통한 운전자의 편의성 증대와 차량 긁힘 및 손상 방지를 위한 모델 RC카 설계

## 개발 목표
* 리모컨(Controller)
  - 블루투스 HC-06 모듈로 블루투스 통신
  - 조이스틱으로 전진, 후진, 전진 우회전, 전진 좌회전, 후진 우회전, 후진 좌회전 신호를 전송
  - UART 통신을 이용하여 제어 신호 데이터를 차량에 전송 
* 차량
  - 블루투스 HC-06 모듈로 블루투스 통신
  - UART 통신을 이용하여 리모컨으로부터 제어 신호 데이터를 수신
  - 제어 신호에 따라 장애물이 없는 경우, 차량은 전진, 후진, 전진 우회전, 전진 좌회전, 후진 우회전, 후진 좌회전 동작을 수행
  - 장애물이 있는 경우, 장애물 방향으로의 차량 동작은 금지되고, 아트메가 내장 부저음이 출력. 운전자가 부저음을 듣고 장애물이 차량 근처에 있음을 인지.
  - 운전자는 장애물 방향으로의 차량 조작 동작은 전면 금지되고(운전자의 실수로 장애물 방향으로의 조작 동작을 예방하는 차원), 해당 장애물을 피하는 방향(반대 방향)의 동작만 실행. 
  - EX) 차량 앞쪽에 장애물이 있는 경우 : 전진, 전진 우(좌)회전 불가. 후진, 후진 우(좌)회전만 가능. 
                                    
## 시작 가이드
###  Requirements  
###  For building and running the project you need:
* Microchip studio sw program 
* Atmega128 

## 동작 영상
###  전체 시나리오에 따른 동작 영상
####  동작 개요
####  리모컨으로 RC카 조종 → RC카는 리모컨의 명령에 따라 동작한다. → 해당 동작 방향에 장애물이 있다면 동작하지 않음. 장애물이 있다는 부저음을 울리고 적색 LED를 점등하여 운전자에게 장애물이 있다고 주의해달라는 신호를 전달한다. → 장애물 방향이 아닌 방향의 동작만 작동한다. → 운행 종료 후에는 EEPROM에 저장된 카운트를 백분율로 환산하여 LCD에 표시한다. 해당 정보는 운전자의 운전 패턴 분석 및 운전 취약 부분을 파악하는데 사용할 수 있다. 
* 주행 테스트 영상 https://www.youtube.com/watch?v=j78lOuiBxhw
* 센서 테스트 영상 https://www.youtube.com/watch?v=IxSM7H-Afjw
* 전방 장애물 인식, 전진 불가능 https://www.youtube.com/watch?v=5wfeqD1_rDc
* 후방 장애물 인식. 후진 불가능 https://www.youtube.com/watch?v=bDmryylZ1Io
* 좌측 장애물이 있어 좌회전만 불가능 https://www.youtube.com/watch?v=hXixcmgLJZ4

## HW 구성도 
### Car body
<img src="https://user-images.githubusercontent.com/113449410/231181395-a209ace5-1c47-425d-8cff-d4a0e292b178.png"  width="500" height="300">

### Controller
<img src="https://user-images.githubusercontent.com/113449410/231181509-5d642896-f78a-47aa-9b83-5dc152e6be20.png"  width="500" height="300">

## 개발 이미지 - RC Car
<img src="https://user-images.githubusercontent.com/113449410/231184237-7381bc5b-d67c-490a-82f4-ad78534c6a38.jpg"  width="500" height="300">
