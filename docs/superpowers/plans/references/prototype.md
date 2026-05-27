1. 에셋 생성 / 경로 / 네이밍 관리
목적
반복적인 에셋 생성과 정리 작업을 자동화해서 프로젝트 구조를 일정하게 유지하는 영역.
주요 자동화 대상
폴더 구조 자동 생성
에셋 네이밍 규칙 검사
잘못된 경로의 에셋 정리
미사용 에셋 후보 탐지
깨진 참조 검사

2. C++ 클래스 생성 반복
목적
언리얼에서 자주 반복되는 C++ 보일러플레이트 생성을 자동화하는 영역.
주요 자동화 대상
AActor 기반 클래스 생성
UObject / UActorComponent 생성
Interface 생성
Delegate 선언
.Build.cs 의존성 추가
UPROPERTY 메타데이터 추가
UFUNCTION 메타데이터 추가

3. DataAsset / DataTable / Config 관리
목적
게임 시스템 규모가 커질수록 늘어나는 데이터 정의와 설정 파일을 안정적으로 관리하는 영역.
주요 자동화 대상
DataTable Row 구조 관리
필수 필드 누락 검사
중복 ID 검사
잘못된 타입 검사
Config 값 유효성 검사
DataAsset 네이밍 / 경로 검사
